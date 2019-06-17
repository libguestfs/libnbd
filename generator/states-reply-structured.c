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
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.CHECK:
  struct command_in_flight *cmd;
  uint16_t flags, type;
  uint64_t handle;
  uint32_t length;

  flags = be16toh (h->sbuf.sr.structured_reply.flags);
  type = be16toh (h->sbuf.sr.structured_reply.type);
  handle = be64toh (h->sbuf.sr.structured_reply.handle);
  length = be32toh (h->sbuf.sr.structured_reply.length);

  /* Find the command amongst the commands in flight. */
  for (cmd = h->cmds_in_flight; cmd != NULL; cmd = cmd->next) {
    if (cmd->handle == handle)
      break;
  }
  if (cmd == NULL) {
    /* Unlike for simple replies, this is difficult to recover from.  We
     * would need an extra state to read and ignore length bytes. XXX
     */
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "no matching handle found for server reply, "
               "this is probably a bug in the server");
    return -1;
  }

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
    return -1;
  }

  if (NBD_REPLY_TYPE_IS_ERR (type)) {
    if (length < sizeof h->sbuf.sr.payload.error.error) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "too short length in structured reply error");
      return -1;
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
      return -1;
    }
    if (!(flags & NBD_REPLY_FLAG_DONE)) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "NBD_REPLY_FLAG_DONE must be set in NBD_REPLY_TYPE_NONE");
      return -1;
    }
    SET_NEXT_STATE (%^FINISH_COMMAND);
    return 0;
  }
  else if (type == NBD_REPLY_TYPE_OFFSET_DATA) {
    if (length < sizeof h->sbuf.sr.payload.offset_data) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "too short length in NBD_REPLY_TYPE_OFFSET_DATA");
      return -1;
    }
    h->rbuf = &h->sbuf.sr.payload.offset_data;
    h->rlen = sizeof h->sbuf.sr.payload.offset_data;
    SET_NEXT_STATE (%RECV_OFFSET_DATA);
    return 0;
  }
  else if (type == NBD_REPLY_TYPE_OFFSET_HOLE) {
    if (length != 12) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "invalid length in NBD_REPLY_TYPE_NONE");
      return -1;
    }
    h->rbuf = &h->sbuf.sr.payload.offset_hole;
    h->rlen = sizeof h->sbuf.sr.payload.offset_hole;
    SET_NEXT_STATE (%RECV_OFFSET_HOLE);
    return 0;
  }
  else if (type == NBD_REPLY_TYPE_BLOCK_STATUS) {
    /* XXX We should be able to skip the bad reply in these two cases. */
    if (length < 12 || ((length-4) & 7) != 0) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "invalid length in NBD_REPLY_TYPE_BLOCK_STATUS");
      return -1;
    }
    if (cmd->extent_fn == NULL) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "not expecting NBD_REPLY_TYPE_BLOCK_STATUS here");
      return -1;
    }
    /* We read the context ID followed by all the entries into a
     * single array and deal with it at the end.
     */
    free (h->bs_entries);
    h->bs_entries = malloc (length);
    if (h->bs_entries == NULL) {
      SET_NEXT_STATE (%.DEAD);
      set_error (errno, "malloc");
      return -1;
    }
    h->rbuf = h->bs_entries;
    h->rlen = length;
    SET_NEXT_STATE (%RECV_BS_ENTRIES);
    return 0;
  }
  else {
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "unknown structured reply type (%" PRIu16 ")", type);
    return -1;
  }

 REPLY.STRUCTURED_REPLY.RECV_ERROR:
  uint32_t length, msglen;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    length = be32toh (h->sbuf.sr.structured_reply.length);
    msglen = be16toh (h->sbuf.sr.payload.error.error.len);
    if (msglen > length - sizeof h->sbuf.sr.payload.error.error ||
        msglen > sizeof h->sbuf.sr.payload.error.msg) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "error message length too large");
      return -1;
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
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
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
        return -1;
      }
      break;
    case NBD_REPLY_TYPE_ERROR_OFFSET:
      if (length != sizeof h->sbuf.sr.payload.error.offset) {
        SET_NEXT_STATE (%.DEAD);
        set_error (0, "invalid error payload length");
        return -1;
      }
      h->rbuf = &h->sbuf.sr.payload.error.offset;
      break;
    }
    SET_NEXT_STATE (%RECV_ERROR_TAIL);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.RECV_ERROR_TAIL:
  struct command_in_flight *cmd;
  uint16_t flags;
  uint64_t handle;
  uint32_t error;
  uint64_t offset;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    flags = be16toh (h->sbuf.sr.structured_reply.flags);
    handle = be64toh (h->sbuf.sr.structured_reply.handle);
    error = be32toh (h->sbuf.sr.payload.error.error.error);

    /* Find the command amongst the commands in flight. */
    for (cmd = h->cmds_in_flight; cmd != NULL; cmd = cmd->next) {
      if (cmd->handle == handle)
        break;
    }
    assert (cmd); /* guaranteed by CHECK */

    /* Preserve first error encountered */
    if (cmd->error == 0)
      cmd->error = nbd_internal_errno_of_nbd_error (error);

    /* Sanity check that any error offset is in range */
    if (error == NBD_REPLY_TYPE_ERROR_OFFSET) {
      offset = be64toh (h->sbuf.sr.payload.error.offset);
      if (offset < cmd->offset || offset >= cmd->offset + cmd->count) {
        SET_NEXT_STATE (%.DEAD);
        set_error (0, "offset of error reply is out of bounds, "
                   "offset=%" PRIu64 ", cmd->offset=%" PRIu64 ", "
                   "cmd->count=%" PRIu32 ", "
                   "this is likely to be a bug in the server",
                   offset, cmd->offset, cmd->count);
        return -1;
      }
    }

    if (flags & NBD_REPLY_FLAG_DONE)
      SET_NEXT_STATE (%^FINISH_COMMAND);
    else
      SET_NEXT_STATE (%.READY);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.RECV_OFFSET_DATA:
  struct command_in_flight *cmd;
  uint64_t handle;
  uint64_t offset;
  uint32_t length;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    handle = be64toh (h->sbuf.sr.structured_reply.handle);
    length = be32toh (h->sbuf.sr.structured_reply.length);
    offset = be64toh (h->sbuf.sr.payload.offset_data.offset);

    /* Find the command amongst the commands in flight. */
    for (cmd = h->cmds_in_flight; cmd != NULL; cmd = cmd->next) {
      if (cmd->handle == handle)
        break;
    }
    assert (cmd); /* guaranteed by CHECK */

    if (cmd->type != NBD_CMD_READ) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "invalid command for receiving offset-data chunk, "
                 "cmd->type=%" PRIu16 ", "
                 "this is likely to be a bug in the server",
                 cmd->type);
      return -1;
    }
    assert (cmd->data);

    /* Length of the data following. */
    length -= 8;

    /* Is the data within bounds? */
    if (offset < cmd->offset) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "offset of reply is out of bounds, "
                 "offset=%" PRIu64 ", cmd->offset=%" PRIu64 ", "
                 "this is likely to be a bug in the server",
                 offset, cmd->offset);
      return -1;
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
      return -1;
    }

    /* Set up to receive the data directly to the user buffer. */
    h->rbuf = cmd->data + offset;
    h->rlen = length;
    SET_NEXT_STATE (%RECV_OFFSET_DATA_DATA);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.RECV_OFFSET_DATA_DATA:
  uint16_t flags;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    flags = be16toh (h->sbuf.sr.structured_reply.flags);

    if (flags & NBD_REPLY_FLAG_DONE)
      SET_NEXT_STATE (%^FINISH_COMMAND);
    else
      SET_NEXT_STATE (%.READY);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.RECV_OFFSET_HOLE:
  struct command_in_flight *cmd;
  uint64_t handle;
  uint16_t flags;
  uint64_t offset;
  uint32_t length;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    handle = be64toh (h->sbuf.sr.structured_reply.handle);
    flags = be16toh (h->sbuf.sr.structured_reply.flags);
    offset = be64toh (h->sbuf.sr.payload.offset_hole.offset);
    length = be32toh (h->sbuf.sr.payload.offset_hole.length);

    /* Find the command amongst the commands in flight. */
    for (cmd = h->cmds_in_flight; cmd != NULL; cmd = cmd->next) {
      if (cmd->handle == handle)
        break;
    }
    assert (cmd); /* guaranteed by CHECK */

    if (cmd->type != NBD_CMD_READ) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "invalid command for receiving offset-hole chunk, "
                 "cmd->type=%" PRIu16 ", "
                 "this is likely to be a bug in the server",
                 cmd->type);
      return -1;
    }
    assert (cmd->data);

    /* Is the data within bounds? */
    if (offset < cmd->offset) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "offset of reply is out of bounds, "
                 "offset=%" PRIu64 ", cmd->offset=%" PRIu64 ", "
                 "this is likely to be a bug in the server",
                 offset, cmd->offset);
      return -1;
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
      return -1;
    }

    memset (cmd->data + offset, 0, length);

    if (flags & NBD_REPLY_FLAG_DONE)
      SET_NEXT_STATE (%^FINISH_COMMAND);
    else
      SET_NEXT_STATE (%.READY);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.RECV_BS_ENTRIES:
  struct command_in_flight *cmd;
  uint64_t handle;
  uint16_t flags;
  uint32_t length;
  size_t i;
  uint32_t context_id;
  struct meta_context *meta_context;

  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    handle = be64toh (h->sbuf.sr.structured_reply.handle);
    flags = be16toh (h->sbuf.sr.structured_reply.flags);
    length = be32toh (h->sbuf.sr.structured_reply.length);

    /* Find the command amongst the commands in flight. */
    for (cmd = h->cmds_in_flight; cmd != NULL; cmd = cmd->next) {
      if (cmd->handle == handle)
        break;
    }
    /* guaranteed by CHECK */
    assert (cmd);
    assert (cmd->extent_fn);
    assert (h->bs_entries);
    assert (length >= 12);

    if (cmd->error)
      /* Emit a debug message, but ignore it. */
      debug (h, "ignoring meta context ID %" PRIu32 " after callback failure",
             be32toh (h->bs_entries[0]));
    else {
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
        errno = 0;
        if (cmd->extent_fn (cmd->data, meta_context->name, cmd->offset,
                            &h->bs_entries[1], (length-4) / 4) == -1)
          cmd->error = errno ? errno : EPROTO;
      }
      else
        /* Emit a debug message, but ignore it. */
        debug (h, "server sent unexpected meta context ID %" PRIu32,
               context_id);
    }

    if (flags & NBD_REPLY_FLAG_DONE)
      SET_NEXT_STATE (%^FINISH_COMMAND);
    else
      SET_NEXT_STATE (%.READY);
  }
  return 0;

} /* END STATE MACHINE */
