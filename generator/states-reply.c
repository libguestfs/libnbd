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

/* State machine for receiving reply messages from the server. */

/* STATE MACHINE */ {
 REPLY.START:
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
  conn->rbuf = &conn->sbuf;
  conn->rlen = sizeof conn->sbuf.simple_reply;

  r = conn->sock->ops->recv (conn->sock, conn->rbuf, conn->rlen);
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
    return -1;
  }
  if (r == 0) {
    SET_NEXT_STATE (%.CLOSED);
    return 0;
  }

  conn->rbuf += r;
  conn->rlen -= r;
  SET_NEXT_STATE (%RECV_REPLY);
  return 0;

 REPLY.RECV_REPLY:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0: SET_NEXT_STATE (%CHECK_SIMPLE_OR_STRUCTURED_REPLY);
  }
  return 0;

 REPLY.CHECK_SIMPLE_OR_STRUCTURED_REPLY:
  uint32_t magic;

  magic = be32toh (conn->sbuf.simple_reply.magic);
  if (magic == NBD_SIMPLE_REPLY_MAGIC) {
    SET_NEXT_STATE (%SIMPLE_REPLY.START);
    return 0;
  }
  else if (magic == NBD_STRUCTURED_REPLY_MAGIC) {
    SET_NEXT_STATE (%STRUCTURED_REPLY.START);
    return 0;
  }
  else {
    SET_NEXT_STATE (%.DEAD); /* We've probably lost synchronization. */
    set_error (0, "invalid reply magic");
    return -1;
  }

 REPLY.FINISH_COMMAND:
  struct command_in_flight *prev_cmd, *cmd;
  uint64_t handle;

  /* NB: This works for both simple and structured replies because the
   * handle is stored at the same offset.
   */
  handle = be64toh (conn->sbuf.simple_reply.handle);
  /* Find the command amongst the commands in flight. */
  for (cmd = conn->cmds_in_flight, prev_cmd = NULL;
       cmd != NULL;
       prev_cmd = cmd, cmd = cmd->next) {
    if (cmd->handle == handle)
      break;
  }
  assert (cmd != NULL);

  /* Move it to the end of the cmds_done list. */
  if (prev_cmd != NULL)
    prev_cmd->next = cmd->next;
  else
    conn->cmds_in_flight = cmd->next;
  cmd->next = NULL;
  if (conn->cmds_done) {
    prev_cmd = conn->cmds_done;
    while (prev_cmd->next)
      prev_cmd = prev_cmd->next;
    prev_cmd->next = cmd;
  }
  else
    conn->cmds_done = cmd;

  SET_NEXT_STATE (%.READY);
  return 0;

} /* END STATE MACHINE */
