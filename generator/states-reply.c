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

#include <assert.h>

/* State machine for receiving reply messages from the server.
 *
 * Note that we never block while in this sub-group. If there is
 * insufficient data to finish parsing a reply, requiring us to block
 * until POLLIN, we instead track where in the state machine we left
 * off, then return to READY to actually block. Then, on entry to
 * REPLY.START, we can tell if this is the start of a new reply (rlen
 * is 0, stay put), a continuation of the preamble (reply_cmd is NULL,
 * resume with RECV_REPLY), or a continuation from any other location
 * (reply_cmd contains the state to jump to).
 */

static void
save_reply_state (struct nbd_handle *h)
{
  assert (h->reply_cmd);
  assert (h->rlen);
  h->reply_cmd->state = get_next_state (h);
}

STATE_MACHINE {
 REPLY.START:
  /* If rlen is non-zero, we are resuming an earlier reply cycle. */
  if (h->rlen > 0) {
    if (h->reply_cmd) {
      assert (nbd_internal_is_state_processing (h->reply_cmd->state));
      SET_NEXT_STATE (h->reply_cmd->state);
    }
    else
      SET_NEXT_STATE (%RECV_REPLY);
    return 0;
  }

  /* This state is entered when a read notification is received in the
   * READY state.  Therefore we know the socket is readable here.
   * Reading a zero length now would indicate that the socket has been
   * closed by the server and so we should jump to the CLOSED state.
   * However recv_into_rbuf will fail in this case, so test it as a
   * special case.
   */
  ssize_t r;

  /* We read all replies initially as if they are simple replies, but
   * check the magic in CHECK_SIMPLE_OR_STRUCTURED_REPLY below.
   * This works because the structured_reply header is larger.
   */
  assert (h->reply_cmd == NULL);
  assert (h->rlen == 0);

  h->rbuf = &h->sbuf;
  h->rlen = sizeof h->sbuf.simple_reply;

  r = h->sock->ops->recv (h, h->sock, h->rbuf, h->rlen);
  if (r == -1) {
    /* This should never happen because when we enter this state we
     * should have notification that the socket is ready to read.
     * However if for some reason it does happen, ignore it - we will
     * reenter this same state again next time the socket is ready to
     * read.
     */
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return 0;

    /* sock->ops->recv called set_error already. */
    SET_NEXT_STATE (%.DEAD);
    return 0;
  }
  if (r == 0) {
    SET_NEXT_STATE (%.CLOSED);
    return 0;
  }
#ifdef DUMP_PACKETS
  if (h->rbuf != NULL)
    nbd_internal_hexdump (h->rbuf, r, stderr);
#endif

  h->rbuf += r;
  h->rlen -= r;
  SET_NEXT_STATE (%RECV_REPLY);
  return 0;

 REPLY.RECV_REPLY:
  switch (recv_into_rbuf (h)) {
  case -1: SET_NEXT_STATE (%.DEAD); return 0;
  case 1: SET_NEXT_STATE (%.READY); return 0;
  case 0: SET_NEXT_STATE (%CHECK_SIMPLE_OR_STRUCTURED_REPLY);
  }
  return 0;

 REPLY.CHECK_SIMPLE_OR_STRUCTURED_REPLY:
  struct command *cmd;
  uint32_t magic;
  uint64_t cookie;

  magic = be32toh (h->sbuf.simple_reply.magic);
  if (magic == NBD_SIMPLE_REPLY_MAGIC) {
    SET_NEXT_STATE (%SIMPLE_REPLY.START);
  }
  else if (magic == NBD_STRUCTURED_REPLY_MAGIC) {
    SET_NEXT_STATE (%STRUCTURED_REPLY.START);
  }
  else {
    SET_NEXT_STATE (%.DEAD); /* We've probably lost synchronization. */
    set_error (0, "invalid reply magic");
    return 0;
  }

  /* NB: This works for both simple and structured replies because the
   * handle (our cookie) is stored at the same offset.
   */
  cookie = be64toh (h->sbuf.simple_reply.handle);
  /* Find the command amongst the commands in flight. */
  for (cmd = h->cmds_in_flight; cmd != NULL; cmd = cmd->next) {
    if (cmd->cookie == cookie)
      break;
  }
  if (cmd == NULL) {
    /* An unexpected structured reply could be skipped, since it
     * includes a length; similarly an unexpected simple reply can be
     * skipped if we assume it was not a read. However, it's more
     * likely we've lost synchronization with the server.
     */
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "no matching cookie found for server reply, "
               "this is probably a bug in the server");
    return 0;
  }
  h->reply_cmd = cmd;
  return 0;

 REPLY.FINISH_COMMAND:
  struct command *prev_cmd, *cmd;
  uint64_t cookie;
  bool retire;

  /* NB: This works for both simple and structured replies because the
   * handle (our cookie) is stored at the same offset.
   */
  cookie = be64toh (h->sbuf.simple_reply.handle);
  /* Find the command amongst the commands in flight. */
  for (cmd = h->cmds_in_flight, prev_cmd = NULL;
       cmd != NULL;
       prev_cmd = cmd, cmd = cmd->next) {
    if (cmd->cookie == cookie)
      break;
  }
  assert (cmd != NULL);
  assert (h->reply_cmd == cmd);
  h->reply_cmd = NULL;
  retire = cmd->type == NBD_CMD_DISC;

  /* Notify the user */
  if (CALLBACK_IS_NOT_NULL (cmd->cb.completion)) {
    int error = cmd->error;
    int r;

    assert (cmd->type != NBD_CMD_DISC);
    r = CALL_CALLBACK (cmd->cb.completion, &error);
    switch (r) {
    case -1:
      if (error)
        cmd->error = error;
      break;
    case 1:
      retire = true;
      break;
    }
  }

  /* Move it to the end of the cmds_done list. */
  if (prev_cmd != NULL)
    prev_cmd->next = cmd->next;
  else
    h->cmds_in_flight = cmd->next;
  cmd->next = NULL;
  if (retire)
    nbd_internal_retire_and_free_command (cmd);
  else {
    if (h->cmds_done_tail != NULL)
      h->cmds_done_tail = h->cmds_done_tail->next = cmd;
    else {
      assert (h->cmds_done == NULL);
      h->cmds_done = h->cmds_done_tail = cmd;
    }
  }
  h->in_flight--;
  assert (h->in_flight >= 0);

  SET_NEXT_STATE (%.READY);
  return 0;

} /* END STATE MACHINE */
