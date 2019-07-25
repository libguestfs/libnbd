/* nbd client library in userspace: state machine
 * Copyright (C) 2013-2019 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* State machine for parsing structured replies from the server. */

static unsigned
valid_flags (struct nbd_handle *h)
{
  unsigned valid = LIBNBD_CALLBACK_VALID;
  uint16_t flags = be16toh (h->sbuf.sr.structured_reply.flags);

  if (flags & NBD_REPLY_FLAG_DONE)
    valid |= LIBNBD_CALLBACK_FREE;
  return valid;
}

/*----- End of prologue. -----*/

/* STATE MACHINE */ {
 REPLY.STRUCTURED_REPLY.START:
  /* We've only read the simple_reply.  The structured_reply is longer,
   * so read the remaining part.
   */
  if (!h->structured_replies) {
    set_error (0, "server sent unexpected structured reply");
    SET_NEXT_STATE(%.DEAD);
    return 0;
  }
  h->rbuf = &h->sbuf;
  h->rbuf += sizeof h->sbuf.simple_reply;
  h->rlen = sizeof h->sbuf.sr.structured_reply;
  h->rlen -= sizeof h->sbuf.simple_reply;
  SET_NEXT_STATE (%RECV_REMAINING);
  return 0;

 REPLY.STRUCTURED_REPLY.RECV_REMAINING:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 1:
    save_reply_state (h);
    SET_NEXT_STATE (%.READY);
    return 0;
  case 0:  SET_NEXT_STATE (%CHECK);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.CHECK:
  struct command *cmd = h->reply_cmd;
  uint16_t flags, type;
  uint64_t cookie;
  uint32_t length;

  flags = be16toh (h->sbuf.sr.structured_reply.flags);
  type = be16toh (h->sbuf.sr.structured_reply.type);
  cookie = be64toh (h->sbuf.sr.structured_reply.handle);
  length = be32toh (h->sbuf.sr.structured_reply.length);

  assert (cmd);
  assert (cmd->cookie == cookie);

  /* Reject a server that replies with too much information, but don't
   * reject a single structured reply to NBD_CMD_READ on the largest
   * size we were willing to send. The most likely culprit is a server
   * that replies with block status with way too many extents, but any
   * oversized reply is going to take long enough to resync that it is
   * not worth keeping the connection alive.
   */
  if (length > MAX_REQUEST_SIZE + sizeof h->sbuf.sr.payload.offset_data) {
    set_error (0, "invalid server reply length");
    SET_NEXT_STATE (%.DEAD);
    return 0;
  }

  if (NBD_REPLY_TYPE_IS_ERR (type)) {
    if (length < sizeof h->sbuf.sr.payload.error.error) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "too short length in structured reply error");
      return 0;
    }
    h->rbuf = &h->sbuf.sr.payload.error.error;
    h->rlen = sizeof h->sbuf.sr.payload.error.error;
    SET_NEXT_STATE (%RECV_ERROR);
    return 0;
  }
  else if (type == NBD_REPLY_TYPE_NONE) {
    if (length != 0) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "invalid length in NBD_REPLY_TYPE_NONE");
      return 0;
    }
    if (!(flags & NBD_REPLY_FLAG_DONE)) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "NBD_REPLY_FLAG_DONE must be set in NBD_REPLY_TYPE_NONE");
      return 0;
    }
    SET_NEXT_STATE (%FINISH);
    return 0;
  }
  else if (type == NBD_REPLY_TYPE_OFFSET_DATA) {
    /* The spec states that 0-length requests are unspecified, but
     * 0-length replies are broken. Still, it's easy enough to support
     * them as an extension, so we use < instead of <=.
     */
    if (cmd->type != NBD_CMD_READ) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "invalid command for receiving offset-data chunk, "
                 "cmd->type=%" PRIu16 ", "
                 "this is likely to be a bug in the server",
                 cmd->type);
      return 0;
    }
    if (length < sizeof h->sbuf.sr.payload.offset_data) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "too short length in NBD_REPLY_TYPE_OFFSET_DATA");
      return 0;
    }
    h->rbuf = &h->sbuf.sr.payload.offset_data;
    h->rlen = sizeof h->sbuf.sr.payload.offset_data;
    SET_NEXT_STATE (%RECV_OFFSET_DATA);
    return 0;
  }
  else if (type == NBD_REPLY_TYPE_OFFSET_HOLE) {
    if (cmd->type != NBD_CMD_READ) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "invalid command for receiving offset-hole chunk, "
                 "cmd->type=%" PRIu16 ", "
                 "this is likely to be a bug in the server",
                 cmd->type);
      return 0;
    }
    if (length != sizeof h->sbuf.sr.payload.offset_hole) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "invalid length in NBD_REPLY_TYPE_OFFSET_HOLE");
      return 0;
    }
    h->rbuf = &h->sbuf.sr.payload.offset_hole;
    h->rlen = sizeof h->sbuf.sr.payload.offset_hole;
    SET_NEXT_STATE (%RECV_OFFSET_HOLE);
    return 0;
  }
  else if (type == NBD_REPLY_TYPE_BLOCK_STATUS) {
    if (cmd->type != NBD_CMD_BLOCK_STATUS) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "invalid command for receiving block-status chunk, "
                 "cmd->type=%" PRIu16 ", "
                 "this is likely to be a bug in the server",
                 cmd->type);
      return 0;
    }
    /* XXX We should be able to skip the bad reply in these two cases. */
    if (length < 12 || ((length-4) & 7) != 0) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "invalid length in NBD_REPLY_TYPE_BLOCK_STATUS");
      return 0;
    }
    if (cmd->cb.fn.extent == NULL) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "not expecting NBD_REPLY_TYPE_BLOCK_STATUS here");
      return 0;
    }
    /* We read the context ID followed by all the entries into a
     * single array and deal with it at the end.
     */
    free (h->bs_entries);
    h->bs_entries = malloc (length);
    if (h->bs_entries == NULL) {
      SET_NEXT_STATE (%.DEAD);
      set_error (errno, "malloc");
      return 0;
    }
    h->rbuf = h->bs_entries;
    h->rlen = length;
    SET_NEXT_STATE (%RECV_BS_ENTRIES);
    return 0;
  }
  else {
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "unknown structured reply type (%" PRIu16 ")", type);
    return 0;
  }

 REPLY.STRUCTURED_REPLY.RECV_ERROR:
  uint32_t length, msglen;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 1:
    save_reply_state (h);
    SET_NEXT_STATE (%.READY);
    return 0;
  case 0:
    length = be32toh (h->sbuf.sr.structured_reply.length);
    msglen = be16toh (h->sbuf.sr.payload.error.error.len);
    if (msglen > length - sizeof h->sbuf.sr.payload.error.error ||
        msglen > sizeof h->sbuf.sr.payload.error.msg) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "error message length too large");
      return 0;
    }

    h->rbuf = h->sbuf.sr.payload.error.msg;
    h->rlen = msglen;
    SET_NEXT_STATE (%RECV_ERROR_MESSAGE);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.RECV_ERROR_MESSAGE:
  uint32_t length, msglen;
  uint16_t type;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 1:
    save_reply_state (h);
    SET_NEXT_STATE (%.READY);
    return 0;
  case 0:
    length = be32toh (h->sbuf.sr.structured_reply.length);
    msglen = be16toh (h->sbuf.sr.payload.error.error.len);
    type = be16toh (h->sbuf.sr.structured_reply.type);

    length -= sizeof h->sbuf.sr.payload.error.error + msglen;

    if (msglen)
      debug (h, "structured error server message: %.*s", (int) msglen,
             h->sbuf.sr.payload.error.msg);

    /* Special case two specific errors; ignore the tail for all others */
    h->rbuf = NULL;
    h->rlen = length;
    switch (type) {
    case NBD_REPLY_TYPE_ERROR:
      if (length != 0) {
        SET_NEXT_STATE (%.DEAD);
        set_error (0, "error payload length too large");
        return 0;
      }
      break;
    case NBD_REPLY_TYPE_ERROR_OFFSET:
      if (length != sizeof h->sbuf.sr.payload.error.offset) {
        SET_NEXT_STATE (%.DEAD);
        set_error (0, "invalid error payload length");
        return 0;
      }
      h->rbuf = &h->sbuf.sr.payload.error.offset;
      break;
    }
    SET_NEXT_STATE (%RECV_ERROR_TAIL);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.RECV_ERROR_TAIL:
  struct command *cmd = h->reply_cmd;
  uint32_t error;
  uint64_t offset;
  uint16_t type;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 1:
    save_reply_state (h);
    SET_NEXT_STATE (%.READY);
    return 0;
  case 0:
    error = be32toh (h->sbuf.sr.payload.error.error.error);
    type = be16toh (h->sbuf.sr.structured_reply.type);

    assert (cmd); /* guaranteed by CHECK */
    error = nbd_internal_errno_of_nbd_error (error);

    /* The spec requires the server to send a non-zero error */
    if (error == NBD_SUCCESS) {
      debug (h, "server forgot to set error; using EINVAL");
      error = NBD_EINVAL;
    }
    error = nbd_internal_errno_of_nbd_error (error);

    /* Sanity check that any error offset is in range, then invoke
     * user callback if present.
     */
    if (type == NBD_REPLY_TYPE_ERROR_OFFSET) {
      offset = be64toh (h->sbuf.sr.payload.error.offset);
      if (offset < cmd->offset || offset >= cmd->offset + cmd->count) {
        SET_NEXT_STATE (%.DEAD);
        set_error (0, "offset of error reply is out of bounds, "
                   "offset=%" PRIu64 ", cmd->offset=%" PRIu64 ", "
                   "cmd->count=%" PRIu32 ", "
                   "this is likely to be a bug in the server",
                   offset, cmd->offset, cmd->count);
        return 0;
      }
      if (cmd->type == NBD_CMD_READ && cmd->cb.fn.read) {
        int scratch = error;
        unsigned valid = valid_flags (h);

        /* Different from successful reads: inform the callback about the
         * current error rather than any earlier one. If the callback fails
         * without setting errno, then use the server's error below.
         */
        if (cmd->cb.fn.read (valid, cmd->cb.fn_user_data,
                             cmd->data + (offset - cmd->offset),
                             0, offset, LIBNBD_READ_ERROR, &scratch) == -1)
          if (cmd->error == 0)
            cmd->error = scratch;
        if (valid & LIBNBD_CALLBACK_FREE)
          cmd->cb.fn.read = NULL; /* because we've freed it */
      }
    }

    /* Preserve first error encountered */
    if (cmd->error == 0)
      cmd->error = error;

    SET_NEXT_STATE(%FINISH);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.RECV_OFFSET_DATA:
  struct command *cmd = h->reply_cmd;
  uint64_t offset;
  uint32_t length;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 1:
    save_reply_state (h);
    SET_NEXT_STATE (%.READY);
    return 0;
  case 0:
    length = be32toh (h->sbuf.sr.structured_reply.length);
    offset = be64toh (h->sbuf.sr.payload.offset_data.offset);

    assert (cmd); /* guaranteed by CHECK */

    assert (cmd->data && cmd->type == NBD_CMD_READ);
    cmd->data_seen = true;

    /* Length of the data following. */
    length -= 8;

    /* Is the data within bounds? */
    if (offset < cmd->offset) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "offset of reply is out of bounds, "
                 "offset=%" PRIu64 ", cmd->offset=%" PRIu64 ", "
                 "this is likely to be a bug in the server",
                 offset, cmd->offset);
      return 0;
    }
    /* Now this is the byte offset in the read buffer. */
    offset -= cmd->offset;

    if (offset + length > cmd->count) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "offset/length of reply is out of bounds, "
                 "offset=%" PRIu64 ", length=%" PRIu32 ", "
                 "cmd->count=%" PRIu32 ", "
                 "this is likely to be a bug in the server",
                 offset, length, cmd->count);
      return 0;
    }

    /* Set up to receive the data directly to the user buffer. */
    h->rbuf = cmd->data + offset;
    h->rlen = length;
    SET_NEXT_STATE (%RECV_OFFSET_DATA_DATA);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.RECV_OFFSET_DATA_DATA:
  struct command *cmd = h->reply_cmd;
  uint64_t offset;
  uint32_t length;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 1:
    save_reply_state (h);
    SET_NEXT_STATE (%.READY);
    return 0;
  case 0:
    length = be32toh (h->sbuf.sr.structured_reply.length);
    offset = be64toh (h->sbuf.sr.payload.offset_data.offset);

    assert (cmd); /* guaranteed by CHECK */
    if (cmd->cb.fn.read) {
      int error = cmd->error;
      unsigned valid = valid_flags (h);

      if (cmd->cb.fn.read (valid, cmd->cb.fn_user_data,
                           cmd->data + (offset - cmd->offset),
                           length - sizeof offset, offset,
                           LIBNBD_READ_DATA, &error) == -1)
        if (cmd->error == 0)
          cmd->error = error ? error : EPROTO;
      if (valid & LIBNBD_CALLBACK_FREE)
        cmd->cb.fn.read = NULL; /* because we've freed it */
    }

    SET_NEXT_STATE (%FINISH);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.RECV_OFFSET_HOLE:
  struct command *cmd = h->reply_cmd;
  uint64_t offset;
  uint32_t length;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 1:
    save_reply_state (h);
    SET_NEXT_STATE (%.READY);
    return 0;
  case 0:
    offset = be64toh (h->sbuf.sr.payload.offset_hole.offset);
    length = be32toh (h->sbuf.sr.payload.offset_hole.length);

    assert (cmd); /* guaranteed by CHECK */

    assert (cmd->data && cmd->type == NBD_CMD_READ);
    cmd->data_seen = true;

    /* Is the data within bounds? */
    if (offset < cmd->offset) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "offset of reply is out of bounds, "
                 "offset=%" PRIu64 ", cmd->offset=%" PRIu64 ", "
                 "this is likely to be a bug in the server",
                 offset, cmd->offset);
      return 0;
    }
    /* Now this is the byte offset in the read buffer. */
    offset -= cmd->offset;

    if (offset + length > cmd->count) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "offset/length of reply is out of bounds, "
                 "offset=%" PRIu64 ", length=%" PRIu32 ", "
                 "cmd->count=%" PRIu32 ", "
                 "this is likely to be a bug in the server",
                 offset, length, cmd->count);
      return 0;
    }

    /* The spec states that 0-length requests are unspecified, but
     * 0-length replies are broken. Still, it's easy enough to support
     * them as an extension, and this works even when length == 0.
     */
    memset (cmd->data + offset, 0, length);
    if (cmd->cb.fn.read) {
      int error = cmd->error;
      unsigned valid = valid_flags (h);

      if (cmd->cb.fn.read (valid, cmd->cb.fn_user_data,
                           cmd->data + offset, length,
                           cmd->offset + offset,
                           LIBNBD_READ_HOLE, &error) == -1)
        if (cmd->error == 0)
          cmd->error = error ? error : EPROTO;
      if (valid & LIBNBD_CALLBACK_FREE)
        cmd->cb.fn.read = NULL; /* because we've freed it */
    }

    SET_NEXT_STATE(%FINISH);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.RECV_BS_ENTRIES:
  struct command *cmd = h->reply_cmd;
  uint32_t length;
  size_t i;
  uint32_t context_id;
  struct meta_context *meta_context;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 1:
    save_reply_state (h);
    SET_NEXT_STATE (%.READY);
    return 0;
  case 0:
    length = be32toh (h->sbuf.sr.structured_reply.length);

    assert (cmd); /* guaranteed by CHECK */
    assert (cmd->type == NBD_CMD_BLOCK_STATUS);
    assert (cmd->cb.fn.extent);
    assert (h->bs_entries);
    assert (length >= 12);

    /* Need to byte-swap the entries returned, but apart from that we
     * don't validate them.
     */
    for (i = 0; i < length/4; ++i)
      h->bs_entries[i] = be32toh (h->bs_entries[i]);

    /* Look up the context ID. */
    context_id = h->bs_entries[0];
    for (meta_context = h->meta_contexts;
         meta_context;
         meta_context = meta_context->next)
      if (context_id == meta_context->context_id)
        break;

    if (meta_context) {
      /* Call the caller's extent function. */
      int error = cmd->error;
      unsigned valid = valid_flags (h);

      if (cmd->cb.fn.extent (valid, cmd->cb.fn_user_data,
                             meta_context->name, cmd->offset,
                             &h->bs_entries[1], (length-4) / 4, &error) == -1)
        if (cmd->error == 0)
          cmd->error = error ? error : EPROTO;
      if (valid & LIBNBD_CALLBACK_FREE)
        cmd->cb.fn.extent = NULL; /* because we've freed it */
    }
    else
      /* Emit a debug message, but ignore it. */
      debug (h, "server sent unexpected meta context ID %" PRIu32,
             context_id);

    SET_NEXT_STATE(%FINISH);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.FINISH:
  struct command *cmd = h->reply_cmd;
  uint16_t flags;

  flags = be16toh (h->sbuf.sr.structured_reply.flags);
  if (flags & NBD_REPLY_FLAG_DONE) {
    if (cmd->type == NBD_CMD_BLOCK_STATUS && cmd->cb.fn.extent)
      cmd->cb.fn.extent (LIBNBD_CALLBACK_FREE, cmd->cb.fn_user_data,
                         NULL, 0, NULL, 0, NULL);
    if (cmd->type == NBD_CMD_READ && cmd->cb.fn.read)
      cmd->cb.fn.read (LIBNBD_CALLBACK_FREE, cmd->cb.fn_user_data,
                       NULL, 0, 0, 0, NULL);
    cmd->cb.fn.read = NULL;
    SET_NEXT_STATE (%^FINISH_COMMAND);
  }
  else {
    h->reply_cmd = NULL;
    SET_NEXT_STATE (%.READY);
  }
  return 0;

} /* END STATE MACHINE */
