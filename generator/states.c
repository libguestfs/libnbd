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

/* This isn't "real" C code.  It is read by the generator, parsed, and
 * put into generated files.  Also it won't make much sense unless you
 * read the generator state machine and documentation in
 * generator/generator first.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "internal.h"

static int
recv_into_rbuf (struct nbd_connection *conn)
{
  ssize_t r;
  char buf[BUFSIZ];
  void *rbuf;
  size_t rlen;

  if (conn->rlen == 0)
    return 0;                   /* move to next state */

  /* As a special case conn->rbuf is allowed to be NULL, meaning
   * throw away the data.
   */
  if (conn->rbuf) {
    rbuf = conn->rbuf;
    rlen = conn->rlen;
  }
  else {
    rbuf = &buf;
    rlen = conn->rlen > sizeof buf ? sizeof buf : conn->rlen;
  }

  r = recv (conn->fd, rbuf, rlen, 0);
  if (r == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return 1;                 /* more data */
    set_error (errno, "recv");
    return -1;
  }
  if (r == 0) {
    set_error (0, "recv: server disconnected unexpectedly");
    return -1;
  }
  if (conn->rbuf)
    conn->rbuf += r;
  conn->rlen -= r;
  if (conn->rlen == 0)
    return 0;                   /* move to next state */
  else
    return 1;                   /* more data */
}

static int
send_from_wbuf (struct nbd_connection *conn)
{
  ssize_t r;

  if (conn->wlen == 0)
    return 0;                   /* move to next state */
  r = send (conn->fd, conn->wbuf, conn->wlen, 0);
  if (r == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return 0;                 /* more data */
    set_error (errno, "send");
    return -1;
  }
  conn->wbuf += r;
  conn->wlen -= r;
  if (conn->wlen == 0)
    return 0;                   /* move to next state */
  else
    return 1;                   /* more data */
}

/*----- End of prologue. -----*/

/* STATE MACHINE */ {
 CONNECT:
  assert (conn->fd == -1);
  conn->fd = socket (AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
  if (conn->fd == -1) {
    SET_NEXT_STATE (%DEAD);
    set_error (errno, "socket");
    return -1;
  }

  if (connect (conn->fd, (struct sockaddr *) &conn->connaddr,
               conn->connaddrlen) == -1) {
    if (errno != EINPROGRESS) {
      SET_NEXT_STATE (%DEAD);
      set_error (errno, "connect");
      return -1;
    }
  }
  return 0;

 CONNECTING:
  int status;
  socklen_t len = sizeof status;

  if (getsockopt (conn->fd, SOL_SOCKET, SO_ERROR, &status, &len) == -1) {
    SET_NEXT_STATE (%DEAD);
    set_error (errno, "getsockopt: SO_ERROR");
    return -1;
  }
  /* This checks the status of the original connect call. */
  if (status == 0) {
    conn->rbuf = &conn->sbuf;
    conn->rlen = 16;
    SET_NEXT_STATE (%RECV_MAGIC);
    return 0;
  }
  else {
    SET_NEXT_STATE (%DEAD);
    set_error (status, "connect");
    return -1;
  }

 RECV_MAGIC:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK_MAGIC);
  }
  return 0;

 CHECK_MAGIC:
  if (strncmp (conn->sbuf.handshake.nbdmagic, "NBDMAGIC", 8) != 0) {
    SET_NEXT_STATE (%DEAD);
    set_error (0, "handshake: server did not send expected NBD magic");
    return -1;
  }
  /* XXX Only handle fixed newstyle servers for now. */
  conn->sbuf.handshake.version = be64toh (conn->sbuf.handshake.version);
  if (conn->sbuf.handshake.version != NBD_NEW_VERSION) {
    SET_NEXT_STATE (%DEAD);
    set_error (0, "handshake: server is not a fixed newstyle NBD server");
    return -1;
  }
  conn->rbuf = &conn->gflags;
  conn->rlen = 2;
  SET_NEXT_STATE (%RECV_NEWSTYLE_GFLAGS);
  return 0;

 RECV_NEWSTYLE_GFLAGS:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK_NEWSTYLE_GFLAGS);
  }
  return 0;

 CHECK_NEWSTYLE_GFLAGS:
  uint32_t cflags;

  conn->gflags = be16toh (conn->gflags);
  if ((conn->gflags & NBD_FLAG_FIXED_NEWSTYLE) == 0) {
    SET_NEXT_STATE (%DEAD);
    set_error (0, "handshake: server is not a fixed newstyle NBD server");
    return -1;
  }

  cflags = conn->gflags & (NBD_FLAG_FIXED_NEWSTYLE|NBD_FLAG_NO_ZEROES);
  conn->sbuf.cflags = htobe32 (cflags);
  conn->wbuf = &conn->sbuf;
  conn->wlen = 4;
  SET_NEXT_STATE (%SEND_NEWSTYLE_CFLAGS);
  return 0;

 SEND_NEWSTYLE_CFLAGS:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:
    //    SET_NEXT_STATE (%-TRY_NEWSTYLE_OPT_STRUCTURED_REPLY);
    SET_NEXT_STATE (%TRY_NEWSTYLE_OPT_GO);
  }
  return 0;

  // TRY_NEWSTYLE_OPT_STRUCTURED_REPLY:

 TRY_NEWSTYLE_OPT_GO:
  conn->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  conn->sbuf.option.option = htobe32 (NBD_OPT_GO);
  conn->sbuf.option.optlen =
    htobe32 (/* exportnamelen */ 4 + strlen (h->export) + /* nrinfos */ 2);
  conn->wbuf = &conn->sbuf;
  conn->wlen = sizeof (conn->sbuf.option);
  SET_NEXT_STATE (%SEND_NEWSTYLE_OPT_GO);
  return 0;

 SEND_NEWSTYLE_OPT_GO:
  const uint32_t exportnamelen = strlen (h->export);

  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:
    conn->sbuf.len = htobe32 (exportnamelen);
    conn->wbuf = &conn->sbuf;
    conn->wlen = 4;
    SET_NEXT_STATE (%SEND_NEWSTYLE_OPT_GO_EXPORTNAMELEN);
  }
  return 0;

 SEND_NEWSTYLE_OPT_GO_EXPORTNAMELEN:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:
    conn->wbuf = h->export;
    conn->wlen = strlen (h->export);
    SET_NEXT_STATE (%SEND_NEWSTYLE_OPT_GO_EXPORT);
  }
  return 0;

 SEND_NEWSTYLE_OPT_GO_EXPORT:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:
    conn->sbuf.nrinfos = 0;
    conn->wbuf = &conn->sbuf;
    conn->wlen = 2;
    SET_NEXT_STATE (%SEND_NEWSTYLE_OPT_GO_NRINFOS);
  }
  return 0;

 SEND_NEWSTYLE_OPT_GO_NRINFOS:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:
    conn->rbuf = &conn->sbuf;
    conn->rlen = sizeof (conn->sbuf.option_reply);
    SET_NEXT_STATE (%RECV_NEWSTYLE_OPT_GO_REPLY);
  }
  return 0;

 RECV_NEWSTYLE_OPT_GO_REPLY:
  uint32_t len;

  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:
    /* We always discard the reply payload. */
    len = be32toh (conn->sbuf.option_reply.replylen);
    if (len > 0) {
      conn->rbuf = NULL;
      conn->rlen = len;
      SET_NEXT_STATE (%SKIP_NEWSTYLE_OPT_GO_REPLY_PAYLOAD);
    }
    else
      SET_NEXT_STATE (%CHECK_NEWSTYLE_OPT_GO_REPLY);
  }
  return 0;

 SKIP_NEWSTYLE_OPT_GO_REPLY_PAYLOAD:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK_NEWSTYLE_OPT_GO_REPLY);
  }
  return 0;

 CHECK_NEWSTYLE_OPT_GO_REPLY:
  conn->sbuf.option_reply.magic = be64toh (conn->sbuf.option_reply.magic);
  conn->sbuf.option_reply.option = be32toh (conn->sbuf.option_reply.option);
  conn->sbuf.option_reply.reply = be32toh (conn->sbuf.option_reply.reply);
  if (conn->sbuf.option_reply.magic != NBD_REP_MAGIC ||
      conn->sbuf.option_reply.option != NBD_OPT_GO) {
    SET_NEXT_STATE (%DEAD);
    set_error (0, "handshake: invalid option reply magic or option");
    return -1;
  }
  switch (conn->sbuf.option_reply.reply) {
  case NBD_REP_ACK:
    SET_NEXT_STATE (%READY);
    return 0;
  case NBD_REP_INFO:
    /* Server is allowed to send any number of NBD_REP_INFO, ignore them. */
    conn->rbuf = &conn->sbuf;
    conn->rlen = sizeof (conn->sbuf.option_reply);
    SET_NEXT_STATE (%RECV_NEWSTYLE_OPT_GO_REPLY);
    return 0;
  case NBD_REP_ERR_UNSUP:
    /* XXX fall back to NBD_OPT_EXPORT_NAME */
    SET_NEXT_STATE (%DEAD);
    set_error (0, "handshake: server does not support NBD_OPT_GO");
    return -1;
  default:
    SET_NEXT_STATE (%DEAD);
    set_error (0, "handshake: unknown reply from NBD_OPT_GO: 0x%" PRIx32,
               conn->sbuf.option_reply.reply);
    return -1;
  }

 ISSUE_COMMAND:
  struct command_in_flight *cmd;

  assert (conn->cmds_to_issue != NULL);
  cmd = conn->cmds_to_issue;
  conn->cmds_to_issue = cmd->next;
  cmd->next = conn->cmds_in_flight;
  conn->cmds_in_flight = cmd;

  conn->sbuf.request.magic = htobe32 (NBD_REQUEST_MAGIC);
  conn->sbuf.request.flags = htobe16 (cmd->flags);
  conn->sbuf.request.type = htobe16 (cmd->type);
  conn->sbuf.request.handle = htobe64 (cmd->handle);
  conn->sbuf.request.offset = htobe64 (cmd->offset);
  conn->sbuf.request.count = htobe32 ((uint32_t) cmd->count);
  conn->wbuf = &conn->sbuf;
  conn->wlen = sizeof (conn->sbuf.request);
  SET_NEXT_STATE (%SEND_REQUEST);
  return 0;

 SEND_REQUEST:
  struct command_in_flight *cmd;

  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:
    assert (conn->cmds_in_flight != NULL);
    cmd = conn->cmds_in_flight;
    assert (cmd->handle == be64toh (conn->sbuf.request.handle));
    if (cmd->type == NBD_CMD_WRITE) {
      conn->wbuf = cmd->data;
      conn->wlen = cmd->count;
      SET_NEXT_STATE (%SEND_WRITE_PAYLOAD);
    }
    else
      SET_NEXT_STATE (%READY);
  }
  return 0;

 SEND_WRITE_PAYLOAD:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:  SET_NEXT_STATE (%READY);
  }
  return 0;

 PREPARE_FOR_REPLY:
  /* This state is entered when a read notification is received in the
   * READY state.  Therefore we know the socket is readable here.
   * Reading a zero length now would indicate that the socket has been
   * closed by the server and so we should jump to the CLOSED state.
   * However recv_into_rbuf will fail in this case, so test it as a
   * special case.
   */
  ssize_t r;
  char c;

  r = recv (conn->fd, &c, 1, MSG_PEEK);
  if (r == 0) {
    SET_NEXT_STATE (%CLOSED);
    return 0;
  }

  conn->rbuf = &conn->sbuf;
  conn->rlen = sizeof (conn->sbuf.simple_reply);
  SET_NEXT_STATE (%RECV_REPLY);
  return 0;

 RECV_REPLY:
  struct command_in_flight *cmd;

  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:
    conn->sbuf.simple_reply.magic = be32toh (conn->sbuf.simple_reply.magic);
    conn->sbuf.simple_reply.error = be32toh (conn->sbuf.simple_reply.error);
    conn->sbuf.simple_reply.handle = be64toh (conn->sbuf.simple_reply.handle);

    if (conn->sbuf.simple_reply.magic != NBD_SIMPLE_REPLY_MAGIC) {
      SET_NEXT_STATE (%DEAD); /* We've probably lost synchronization. */
      set_error (0, "handshake: invalid reply magic");
      return -1;
    }

    /* Find the command amongst the commands in flight. */
    for (cmd = conn->cmds_in_flight; cmd != NULL; cmd = cmd->next) {
      if (cmd->handle == conn->sbuf.simple_reply.handle)
        break;
    }
    if (cmd == NULL) {
      SET_NEXT_STATE (%READY);
      set_error (0, "handshake: no matching handle found for server reply, this is probably a bug in the server");
      return -1;
    }

    cmd->error = conn->sbuf.simple_reply.error;
    if (cmd->error == 0 && cmd->type == NBD_CMD_READ) {
      conn->rbuf = cmd->data;
      conn->rlen = cmd->count;
      SET_NEXT_STATE (%RECV_READ_PAYLOAD);
    }
    else {
      SET_NEXT_STATE (%FINISH_COMMAND);
    }
  }
  return 0;

 RECV_READ_PAYLOAD:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:  SET_NEXT_STATE (%FINISH_COMMAND);
  }
  return 0;

 FINISH_COMMAND:
  struct command_in_flight *prev_cmd, *cmd;

  conn->sbuf.simple_reply.handle = conn->sbuf.simple_reply.handle;
  /* Find the command amongst the commands in flight. */
  for (cmd = conn->cmds_in_flight, prev_cmd = NULL;
       cmd != NULL;
       prev_cmd = cmd, cmd = cmd->next) {
    if (cmd->handle == conn->sbuf.simple_reply.handle)
      break;
  }
  assert (cmd != NULL);

  /* Move it to the cmds_done list. */
  if (prev_cmd != NULL)
    prev_cmd->next = cmd->next;
  else
    conn->cmds_in_flight = cmd->next;
  cmd->next = conn->cmds_done;
  conn->cmds_done = cmd;

  SET_NEXT_STATE (%READY);
  return 0;

 DEAD:
  if (conn->fd >= 0) {
    close (conn->fd);
    conn->fd = -1;
  }
  return 0;

 CLOSED:
  if (conn->fd >= 0) {
    close (conn->fd);
    conn->fd = -1;
  }
  return 0;

} /* END STATE MACHINE */
