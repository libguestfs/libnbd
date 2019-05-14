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
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "internal.h"

/* Uncomment this to dump received protocol packets to stderr. */
/*#define DUMP_PACKETS 1*/

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

  r = conn->sock->ops->recv (conn->sock, rbuf, rlen);
  if (r == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return 1;                 /* more data */
    /* sock->ops->recv called set_error already. */
    return -1;
  }
  if (r == 0) {
    set_error (0, "recv: server disconnected unexpectedly");
    return -1;
  }
#ifdef DUMP_PACKETS
  if (conn->rbuf != NULL)
    nbd_internal_hexdump (conn->rbuf, r, stderr);
#endif
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
  r = conn->sock->ops->send (conn->sock, conn->wbuf, conn->wlen);
  if (r == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return 0;                 /* more data */
    /* sock->ops->send called set_error already. */
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
  int fd;

  assert (!conn->sock);
  fd = socket (AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
  if (fd == -1) {
    SET_NEXT_STATE (%DEAD);
    set_error (errno, "socket");
    return -1;
  }
  conn->sock = nbd_internal_socket_create (fd);
  if (!conn->sock) {
    SET_NEXT_STATE (%DEAD);
    return -1;
  }

  if (connect (fd, (struct sockaddr *) &conn->connaddr,
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

  if (getsockopt (conn->sock->ops->get_fd (conn->sock),
                  SOL_SOCKET, SO_ERROR, &status, &len) == -1) {
    SET_NEXT_STATE (%DEAD);
    set_error (errno, "getsockopt: SO_ERROR");
    return -1;
  }
  /* This checks the status of the original connect call. */
  if (status == 0) {
    SET_NEXT_STATE (%PREPARE_FOR_MAGIC);
    return 0;
  }
  else {
    SET_NEXT_STATE (%DEAD);
    set_error (status, "connect");
    return -1;
  }

 CONNECT_TCP:
  int r;

  assert (conn->hostname != NULL);
  assert (conn->port != NULL);

  if (conn->result) {
    freeaddrinfo (conn->result);
    conn->result = NULL;
  }

  memset (&conn->hints, 0, sizeof conn->hints);
  conn->hints.ai_family = AF_UNSPEC;
  conn->hints.ai_socktype = SOCK_STREAM;
  conn->hints.ai_flags = 0;
  conn->hints.ai_protocol = 0;

  /* XXX Unfortunately getaddrinfo blocks.  getaddrinfo_a isn't
   * portable and in any case isn't an alternative because it can't be
   * integrated into a main loop.
   */
  r = getaddrinfo (conn->hostname, conn->port, &conn->hints, &conn->result);
  if (r != 0) {
    SET_NEXT_STATE (%CREATED);
    set_error (0, "getaddrinfo: %s:%s: %s",
               conn->hostname, conn->port, gai_strerror (r));
    return -1;
  }

  conn->rp = conn->result;
  SET_NEXT_STATE (%CONNECT_TCP_CONNECT);
  return 0;

 CONNECT_TCP_CONNECT:
  int fd;

  assert (!conn->sock);

  if (conn->rp == NULL) {
    /* We tried all the results from getaddrinfo without success.
     * Save errno from most recent connect(2) call. XXX
     */
    SET_NEXT_STATE (%CREATED);
    set_error (0, "connect: %s:%s: could not connect to remote host",
               conn->hostname, conn->port);
    return -1;
  }

  fd = socket (conn->rp->ai_family,
               conn->rp->ai_socktype|SOCK_NONBLOCK|SOCK_CLOEXEC,
               conn->rp->ai_protocol);
  if (fd == -1) {
    SET_NEXT_STATE (%CONNECT_TCP_NEXT);
    return 0;
  }
  conn->sock = nbd_internal_socket_create (fd);
  if (!conn->sock) {
    SET_NEXT_STATE (%DEAD);
    return -1;
  }
  if (connect (fd, conn->rp->ai_addr, conn->rp->ai_addrlen) == -1) {
    if (errno != EINPROGRESS) {
      SET_NEXT_STATE (%CONNECT_TCP_NEXT);
      return 0;
    }
  }

  SET_NEXT_STATE (%CONNECT_TCP_CONNECTING);
  return 0;

 CONNECT_TCP_CONNECTING:
  int status;
  socklen_t len = sizeof status;

  if (getsockopt (conn->sock->ops->get_fd (conn->sock),
                  SOL_SOCKET, SO_ERROR, &status, &len) == -1) {
    SET_NEXT_STATE (%DEAD);
    set_error (errno, "getsockopt: SO_ERROR");
    return -1;
  }
  /* This checks the status of the original connect call. */
  if (status == 0)
    SET_NEXT_STATE (%PREPARE_FOR_MAGIC);
  else
    SET_NEXT_STATE (%CONNECT_TCP_NEXT);
  return 0;

 CONNECT_TCP_NEXT:
  if (conn->sock) {
    conn->sock->ops->close (conn->sock);
    conn->sock = NULL;
  }
  if (conn->rp)
    conn->rp = conn->rp->ai_next;
  SET_NEXT_STATE (%CONNECT_TCP_CONNECT);
  return 0;

 CONNECT_COMMAND:
  int sv[2];
  pid_t pid;

  assert (!conn->sock);
  assert (conn->command);
  if (socketpair (AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0,
                  sv) == -1) {
    SET_NEXT_STATE (%DEAD);
    set_error (errno, "socketpair");
    return -1;
  }

  pid = fork ();
  if (pid == -1) {
    SET_NEXT_STATE (%DEAD);
    set_error (errno, "fork");
    close (sv[0]);
    close (sv[1]);
    return -1;
  }
  if (pid == 0) {         /* child - run command */
    close (0);
    close (1);
    close (sv[0]);
    dup2 (sv[1], 0);
    dup2 (sv[1], 1);
    close (sv[1]);

    /* Restore SIGPIPE back to SIG_DFL, since shell can't undo SIG_IGN */
    signal (SIGPIPE, SIG_DFL);

    if (system (conn->command) == 0)
      _exit (EXIT_SUCCESS);
    perror (conn->command);
    _exit (EXIT_FAILURE);
  }

  /* Parent. */
  conn->pid = pid;
  conn->sock = nbd_internal_socket_create (sv[0]);
  if (!conn->sock) {
    SET_NEXT_STATE (%DEAD);
    return -1;
  }
  close (sv[1]);

  /* The sockets are connected already, we can jump directly to
   * receiving the server magic.
   */
  SET_NEXT_STATE (%PREPARE_FOR_MAGIC);
  return 0;

 PREPARE_FOR_MAGIC:
  conn->rbuf = &conn->sbuf;
  conn->rlen = 16;
  SET_NEXT_STATE (%RECV_MAGIC);
  return 0;

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
    /* Start sending options.  NBD_OPT_STARTTLS must be sent first,
     * NBD_OPT_GO must be sent last.
     */
    if (h->tls) {
      SET_NEXT_STATE (%TRY_NEWSTYLE_OPT_STARTTLS);
    }
    else {
      // SET_NEXT_STATE (%-TRY_NEWSTYLE_OPT_STRUCTURED_REPLY);
      SET_NEXT_STATE (%TRY_NEWSTYLE_OPT_GO);
    }
  }
  return 0;

  // TRY_NEWSTYLE_OPT_STRUCTURED_REPLY:

 TRY_NEWSTYLE_OPT_STARTTLS:
  conn->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  conn->sbuf.option.option = htobe32 (NBD_OPT_STARTTLS);
  conn->sbuf.option.optlen = 0;
  conn->wbuf = &conn->sbuf;
  conn->wlen = sizeof (conn->sbuf.option);
  SET_NEXT_STATE (%SEND_NEWSTYLE_OPT_STARTTLS);
  return 0;

 SEND_NEWSTYLE_OPT_STARTTLS:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:
    conn->rbuf = &conn->sbuf;
    conn->rlen = sizeof (conn->sbuf.or.option_reply);
    SET_NEXT_STATE (%RECV_NEWSTYLE_OPT_STARTTLS_REPLY);
  }
  return 0;

 RECV_NEWSTYLE_OPT_STARTTLS_REPLY:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0: SET_NEXT_STATE (%CHECK_NEWSTYLE_OPT_STARTTLS_REPLY);
  }
  return 0;

 CHECK_NEWSTYLE_OPT_STARTTLS_REPLY:
  uint64_t magic;
  uint32_t option;
  uint32_t reply;
  uint32_t len;
  struct socket *new_sock;

  magic = be64toh (conn->sbuf.or.option_reply.magic);
  option = be32toh (conn->sbuf.or.option_reply.option);
  reply = be32toh (conn->sbuf.or.option_reply.reply);
  len = be32toh (conn->sbuf.or.option_reply.replylen);
  if (magic != NBD_REP_MAGIC || option != NBD_OPT_STARTTLS || len != 0) {
    SET_NEXT_STATE (%DEAD);
    set_error (0, "handshake: invalid option reply magic, option or length");
    return -1;
  }
  switch (reply) {
  case NBD_REP_ACK:
    new_sock = nbd_internal_crypto_create_session (conn, conn->sock);
    if (new_sock == NULL) {
      SET_NEXT_STATE (%DEAD);
      return -1;
    }
    conn->sock = new_sock;
    if (nbd_internal_crypto_is_reading (conn))
      SET_NEXT_STATE (%TLS_HANDSHAKE_READ);
    else
      SET_NEXT_STATE (%TLS_HANDSHAKE_WRITE);
    return 0;

  default:
    if (!NBD_REP_IS_ERR (reply))
      debug (conn->h,
             "server is confused by NBD_OPT_STARTTLS, continuing anyway");

    /* Server refused to upgrade to TLS.  If h->tls is not require (2)
     * then we can continue unencrypted.
     */
    if (h->tls == 2) {
      SET_NEXT_STATE (%DEAD);
      set_error (ENOTSUP, "handshake: server refused TLS, "
                 "but handle TLS setting is require (2)");
      return -1;
    }

    debug (conn->h,
           "server refused TLS (%s), continuing with unencrypted connection",
           reply == NBD_REP_ERR_POLICY ? "policy" : "not supported");
    // SET_NEXT_STATE (%-TRY_NEWSTYLE_OPT_STRUCTURED_REPLY);
    SET_NEXT_STATE (%TRY_NEWSTYLE_OPT_GO);
    return 0;
  }
  return 0;

 TLS_HANDSHAKE_READ:
  int r;

  r = nbd_internal_crypto_handshake (conn);
  if (r == -1) {
    SET_NEXT_STATE (%DEAD);
    return -1;
  }
  if (r == 0) {
    /* Finished handshake. */
    debug (conn->h, "connection is using TLS");

    /* Continue with option negotiation. */
    // SET_NEXT_STATE (%-TRY_NEWSTYLE_OPT_STRUCTURED_REPLY);
    SET_NEXT_STATE (%TRY_NEWSTYLE_OPT_GO);
    return 0;
  }
  /* Continue handshake. */
  if (nbd_internal_crypto_is_reading (conn))
    SET_NEXT_STATE (%TLS_HANDSHAKE_READ);
  else
    SET_NEXT_STATE (%TLS_HANDSHAKE_WRITE);
  return 0;

 TLS_HANDSHAKE_WRITE:
  int r;

  r = nbd_internal_crypto_handshake (conn);
  if (r == -1) {
    SET_NEXT_STATE (%DEAD);
    return -1;
  }
  if (r == 0) {
    /* Finished handshake. */
    debug (conn->h, "connection is using TLS");

    /* Continue with option negotiation. */
    // SET_NEXT_STATE (%-TRY_NEWSTYLE_OPT_STRUCTURED_REPLY);
    SET_NEXT_STATE (%TRY_NEWSTYLE_OPT_GO);
    return 0;
  }
  /* Continue handshake. */
  if (nbd_internal_crypto_is_reading (conn))
    SET_NEXT_STATE (%TLS_HANDSHAKE_READ);
  else
    SET_NEXT_STATE (%TLS_HANDSHAKE_WRITE);
  return 0;

 TRY_NEWSTYLE_OPT_GO:
  conn->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  conn->sbuf.option.option = htobe32 (NBD_OPT_GO);
  conn->sbuf.option.optlen =
    htobe32 (/* exportnamelen */ 4 + strlen (h->export_name) + /* nrinfos */ 2);
  conn->wbuf = &conn->sbuf;
  conn->wlen = sizeof (conn->sbuf.option);
  SET_NEXT_STATE (%SEND_NEWSTYLE_OPT_GO);
  return 0;

 SEND_NEWSTYLE_OPT_GO:
  const uint32_t exportnamelen = strlen (h->export_name);

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
    conn->wbuf = h->export_name;
    conn->wlen = strlen (h->export_name);
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
    conn->rlen = sizeof (conn->sbuf.or.option_reply);
    SET_NEXT_STATE (%RECV_NEWSTYLE_OPT_GO_REPLY);
  }
  return 0;

 RECV_NEWSTYLE_OPT_GO_REPLY:
  uint32_t len;
  const size_t maxpayload = sizeof conn->sbuf.or.payload;

  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:
    /* Read the following payload if it is short enough to fit in the
     * static buffer.  If it's too long, skip it.
     */
    len = be32toh (conn->sbuf.or.option_reply.replylen);
    if (len <= maxpayload)
      conn->rbuf = &conn->sbuf.or.payload;
    else
      conn->rbuf = NULL;
    conn->rlen = len;
    SET_NEXT_STATE (%RECV_NEWSTYLE_OPT_GO_REPLY_PAYLOAD);
  }
  return 0;

 RECV_NEWSTYLE_OPT_GO_REPLY_PAYLOAD:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK_NEWSTYLE_OPT_GO_REPLY);
  }
  return 0;

 CHECK_NEWSTYLE_OPT_GO_REPLY:
  uint64_t magic;
  uint32_t option;
  uint32_t reply;
  uint32_t len;
  const size_t maxpayload = sizeof conn->sbuf.or.payload;

  magic = be64toh (conn->sbuf.or.option_reply.magic);
  option = be32toh (conn->sbuf.or.option_reply.option);
  reply = be32toh (conn->sbuf.or.option_reply.reply);
  len = be32toh (conn->sbuf.or.option_reply.replylen);
  if (magic != NBD_REP_MAGIC || option != NBD_OPT_GO) {
    SET_NEXT_STATE (%DEAD);
    set_error (0, "handshake: invalid option reply magic or option");
    return -1;
  }
  switch (reply) {
  case NBD_REP_ACK:
    SET_NEXT_STATE (%READY);
    return 0;
  case NBD_REP_INFO:
    if (len <= maxpayload /* see RECV_NEWSTYLE_OPT_GO_REPLY */) {
      if (len >= sizeof conn->sbuf.or.payload.export) {
        if (be16toh (conn->sbuf.or.payload.export.info) == NBD_INFO_EXPORT) {
          conn->h->exportsize =
            be64toh (conn->sbuf.or.payload.export.exportsize);
          conn->h->eflags = be16toh (conn->sbuf.or.payload.export.eflags);
          debug (conn->h, "exportsize: %" PRIu64 " eflags: 0x%" PRIx16,
                 conn->h->exportsize, conn->h->eflags);
          if (conn->h->eflags == 0) {
            SET_NEXT_STATE (%DEAD);
            set_error (EINVAL, "handshake: invalid eflags == 0 from server");
            return -1;
          }
        }
      }
    }
    /* ... else ignore the payload. */
    /* Server is allowed to send any number of NBD_REP_INFO, read next one. */
    conn->rbuf = &conn->sbuf;
    conn->rlen = sizeof (conn->sbuf.or.option_reply);
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
               reply);
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

  conn->rbuf = &conn->sbuf;
  conn->rlen = sizeof conn->sbuf.simple_reply;

  r = conn->sock->ops->recv (conn->sock, conn->rbuf, conn->rlen);
  if (r == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      SET_NEXT_STATE (%RECV_REPLY);
      return 0;
    }
    SET_NEXT_STATE (%DEAD);
    /* sock->ops->recv called set_error already. */
    return -1;
  }
  if (r == 0) {
    SET_NEXT_STATE (%CLOSED);
    return 0;
  }

  conn->rbuf += r;
  conn->rlen -= r;
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
  if (conn->sock) {
    conn->sock->ops->close (conn->sock);
    conn->sock = NULL;
  }
  return 0;

 CLOSED:
  if (conn->sock) {
    conn->sock->ops->close (conn->sock);
    conn->sock = NULL;
  }
  return 0;

} /* END STATE MACHINE */
