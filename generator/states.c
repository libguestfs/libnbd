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
    SET_NEXT_STATE (%.DEAD);
    set_error (errno, "socket");
    return -1;
  }
  conn->sock = nbd_internal_socket_create (fd);
  if (!conn->sock) {
    SET_NEXT_STATE (%.DEAD);
    return -1;
  }

  if (connect (fd, (struct sockaddr *) &conn->connaddr,
               conn->connaddrlen) == -1) {
    if (errno != EINPROGRESS) {
      SET_NEXT_STATE (%.DEAD);
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
    SET_NEXT_STATE (%.DEAD);
    set_error (errno, "getsockopt: SO_ERROR");
    return -1;
  }
  /* This checks the status of the original connect call. */
  if (status == 0) {
    SET_NEXT_STATE (%MAGIC.START);
    return 0;
  }
  else {
    SET_NEXT_STATE (%.DEAD);
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
    SET_NEXT_STATE (%START);
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
    SET_NEXT_STATE (%START);
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
    SET_NEXT_STATE (%.DEAD);
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
    SET_NEXT_STATE (%.DEAD);
    set_error (errno, "getsockopt: SO_ERROR");
    return -1;
  }
  /* This checks the status of the original connect call. */
  if (status == 0)
    SET_NEXT_STATE (%MAGIC.START);
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
  assert (conn->argv);
  assert (conn->argv[0]);
  if (socketpair (AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0,
                  sv) == -1) {
    SET_NEXT_STATE (%.DEAD);
    set_error (errno, "socketpair");
    return -1;
  }

  pid = fork ();
  if (pid == -1) {
    SET_NEXT_STATE (%.DEAD);
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

    /* Restore SIGPIPE back to SIG_DFL. */
    signal (SIGPIPE, SIG_DFL);

    execvp (conn->argv[0], conn->argv);
    perror (conn->argv[0]);
    _exit (EXIT_FAILURE);
  }

  /* Parent. */
  conn->pid = pid;
  conn->sock = nbd_internal_socket_create (sv[0]);
  if (!conn->sock) {
    SET_NEXT_STATE (%.DEAD);
    return -1;
  }
  close (sv[1]);

  /* The sockets are connected already, we can jump directly to
   * receiving the server magic.
   */
  SET_NEXT_STATE (%MAGIC.START);
  return 0;

 MAGIC.START:
  conn->rbuf = &conn->sbuf;
  conn->rlen = 16;
  SET_NEXT_STATE (%RECV_MAGIC);
  return 0;

 MAGIC.RECV_MAGIC:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK_MAGIC);
  }
  return 0;

 MAGIC.CHECK_MAGIC:
  if (strncmp (conn->sbuf.handshake.nbdmagic, "NBDMAGIC", 8) != 0) {
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: server did not send expected NBD magic");
    return -1;
  }
  /* XXX Only handle fixed newstyle servers for now. */
  conn->sbuf.handshake.version = be64toh (conn->sbuf.handshake.version);
  if (conn->sbuf.handshake.version != NBD_NEW_VERSION) {
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: server is not a fixed newstyle NBD server");
    return -1;
  }
  conn->rbuf = &conn->gflags;
  conn->rlen = 2;
  SET_NEXT_STATE (%.NEWSTYLE.START);
  return 0;

 NEWSTYLE.START:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK_GFLAGS);
  }
  return 0;

 NEWSTYLE.CHECK_GFLAGS:
  uint32_t cflags;

  conn->gflags = be16toh (conn->gflags);
  if ((conn->gflags & NBD_FLAG_FIXED_NEWSTYLE) == 0) {
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: server is not a fixed newstyle NBD server");
    return -1;
  }

  cflags = conn->gflags & (NBD_FLAG_FIXED_NEWSTYLE|NBD_FLAG_NO_ZEROES);
  conn->sbuf.cflags = htobe32 (cflags);
  conn->wbuf = &conn->sbuf;
  conn->wlen = 4;
  SET_NEXT_STATE (%SEND_CFLAGS);
  return 0;

 NEWSTYLE.SEND_CFLAGS:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    /* Start sending options.  NBD_OPT_STARTTLS must be sent first. */
    if (h->tls)
      SET_NEXT_STATE (%OPT_STARTTLS.START);
    else
      SET_NEXT_STATE (%OPT_STRUCTURED_REPLY.START);
  }
  return 0;

 NEWSTYLE.OPT_STARTTLS.START:
  conn->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  conn->sbuf.option.option = htobe32 (NBD_OPT_STARTTLS);
  conn->sbuf.option.optlen = 0;
  conn->wbuf = &conn->sbuf;
  conn->wlen = sizeof (conn->sbuf.option);
  SET_NEXT_STATE (%SEND);
  return 0;

 NEWSTYLE.OPT_STARTTLS.SEND:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    conn->rbuf = &conn->sbuf;
    conn->rlen = sizeof (conn->sbuf.or.option_reply);
    SET_NEXT_STATE (%RECV_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_STARTTLS.RECV_REPLY:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0: SET_NEXT_STATE (%CHECK_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_STARTTLS.CHECK_REPLY:
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
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: invalid option reply magic, option or length");
    return -1;
  }
  switch (reply) {
  case NBD_REP_ACK:
    new_sock = nbd_internal_crypto_create_session (conn, conn->sock);
    if (new_sock == NULL) {
      SET_NEXT_STATE (%.DEAD);
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
      SET_NEXT_STATE (%.DEAD);
      set_error (ENOTSUP, "handshake: server refused TLS, "
                 "but handle TLS setting is require (2)");
      return -1;
    }

    debug (conn->h,
           "server refused TLS (%s), continuing with unencrypted connection",
           reply == NBD_REP_ERR_POLICY ? "policy" : "not supported");
    SET_NEXT_STATE (%^OPT_STRUCTURED_REPLY.START);
    return 0;
  }
  return 0;

 NEWSTYLE.OPT_STARTTLS.TLS_HANDSHAKE_READ:
  int r;

  r = nbd_internal_crypto_handshake (conn);
  if (r == -1) {
    SET_NEXT_STATE (%.DEAD);
    return -1;
  }
  if (r == 0) {
    /* Finished handshake. */
    nbd_internal_crypto_debug_tls_enabled (conn);

    /* Continue with option negotiation. */
    SET_NEXT_STATE (%^OPT_STRUCTURED_REPLY.START);
    return 0;
  }
  /* Continue handshake. */
  if (nbd_internal_crypto_is_reading (conn))
    SET_NEXT_STATE (%TLS_HANDSHAKE_READ);
  else
    SET_NEXT_STATE (%TLS_HANDSHAKE_WRITE);
  return 0;

 NEWSTYLE.OPT_STARTTLS.TLS_HANDSHAKE_WRITE:
  int r;

  r = nbd_internal_crypto_handshake (conn);
  if (r == -1) {
    SET_NEXT_STATE (%.DEAD);
    return -1;
  }
  if (r == 0) {
    /* Finished handshake. */
    debug (conn->h, "connection is using TLS");

    /* Continue with option negotiation. */
    SET_NEXT_STATE (%^OPT_STRUCTURED_REPLY.START);
    return 0;
  }
  /* Continue handshake. */
  if (nbd_internal_crypto_is_reading (conn))
    SET_NEXT_STATE (%TLS_HANDSHAKE_READ);
  else
    SET_NEXT_STATE (%TLS_HANDSHAKE_WRITE);
  return 0;

 NEWSTYLE.OPT_STRUCTURED_REPLY.START:
  conn->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  conn->sbuf.option.option = htobe32 (NBD_OPT_STRUCTURED_REPLY);
  conn->sbuf.option.optlen = htobe32 (0);
  conn->wbuf = &conn->sbuf;
  conn->wlen = sizeof conn->sbuf.option;
  SET_NEXT_STATE (%SEND);
  return 0;

 NEWSTYLE.OPT_STRUCTURED_REPLY.SEND:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    conn->rbuf = &conn->sbuf;
    conn->rlen = sizeof conn->sbuf.or.option_reply;
    SET_NEXT_STATE (%RECV_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_STRUCTURED_REPLY.RECV_REPLY:
  uint32_t len;

  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    /* Discard the payload if there is one. */
    len = be32toh (conn->sbuf.or.option_reply.replylen);
    conn->rbuf = NULL;
    conn->rlen = len;
    SET_NEXT_STATE (%SKIP_REPLY_PAYLOAD);
  }
  return 0;

 NEWSTYLE.OPT_STRUCTURED_REPLY.SKIP_REPLY_PAYLOAD:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_STRUCTURED_REPLY.CHECK_REPLY:
  uint64_t magic;
  uint32_t option;
  uint32_t reply;

  magic = be64toh (conn->sbuf.or.option_reply.magic);
  option = be32toh (conn->sbuf.or.option_reply.option);
  reply = be32toh (conn->sbuf.or.option_reply.reply);
  if (magic != NBD_REP_MAGIC || option != NBD_OPT_STRUCTURED_REPLY) {
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: invalid option reply magic or option");
    return -1;
  }
  switch (reply) {
  case NBD_REP_ACK:
    debug (conn->h, "negotiated structured replies on this connection");
    conn->structured_replies = true;
    break;
  default:
    debug (conn->h, "structured replies are not supported by this server");
    conn->structured_replies = false;
    break;
  }

  /* Next option. */
  SET_NEXT_STATE (%^OPT_GO.START);
  return 0;

 NEWSTYLE.OPT_GO.START:
  conn->sbuf.option.version = htobe64 (NBD_NEW_VERSION);
  conn->sbuf.option.option = htobe32 (NBD_OPT_GO);
  conn->sbuf.option.optlen =
    htobe32 (/* exportnamelen */ 4 + strlen (h->export_name) + /* nrinfos */ 2);
  conn->wbuf = &conn->sbuf;
  conn->wlen = sizeof conn->sbuf.option;
  SET_NEXT_STATE (%SEND);
  return 0;

 NEWSTYLE.OPT_GO.SEND:
  const uint32_t exportnamelen = strlen (h->export_name);

  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    conn->sbuf.len = htobe32 (exportnamelen);
    conn->wbuf = &conn->sbuf;
    conn->wlen = 4;
    SET_NEXT_STATE (%SEND_EXPORTNAMELEN);
  }
  return 0;

 NEWSTYLE.OPT_GO.SEND_EXPORTNAMELEN:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    conn->wbuf = h->export_name;
    conn->wlen = strlen (h->export_name);
    SET_NEXT_STATE (%SEND_EXPORT);
  }
  return 0;

 NEWSTYLE.OPT_GO.SEND_EXPORT:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    conn->sbuf.nrinfos = 0;
    conn->wbuf = &conn->sbuf;
    conn->wlen = 2;
    SET_NEXT_STATE (%SEND_NRINFOS);
  }
  return 0;

 NEWSTYLE.OPT_GO.SEND_NRINFOS:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    conn->rbuf = &conn->sbuf;
    conn->rlen = sizeof conn->sbuf.or.option_reply;
    SET_NEXT_STATE (%RECV_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_GO.RECV_REPLY:
  uint32_t len;
  const size_t maxpayload = sizeof conn->sbuf.or.payload;

  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
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
    SET_NEXT_STATE (%RECV_REPLY_PAYLOAD);
  }
  return 0;

 NEWSTYLE.OPT_GO.RECV_REPLY_PAYLOAD:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK_REPLY);
  }
  return 0;

 NEWSTYLE.OPT_GO.CHECK_REPLY:
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
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: invalid option reply magic or option");
    return -1;
  }
  switch (reply) {
  case NBD_REP_ACK:
    SET_NEXT_STATE (%.READY);
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
            SET_NEXT_STATE (%.DEAD);
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
    SET_NEXT_STATE (%RECV_REPLY);
    return 0;
  case NBD_REP_ERR_UNSUP:
    /* XXX fall back to NBD_OPT_EXPORT_NAME */
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: server does not support NBD_OPT_GO");
    return -1;
  default:
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "handshake: unknown reply from NBD_OPT_GO: 0x%" PRIx32,
               reply);
    return -1;
  }

 ISSUE_COMMAND.START:
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

 ISSUE_COMMAND.SEND_REQUEST:
  struct command_in_flight *cmd;

  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
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
      SET_NEXT_STATE (%.READY);
  }
  return 0;

 ISSUE_COMMAND.SEND_WRITE_PAYLOAD:
  switch (send_from_wbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%.READY);
  }
  return 0;

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
    /* We only read the simple_reply.  The structured_reply is longer,
     * so read the remaining part.
     */
    conn->rbuf = &conn->sbuf;
    conn->rbuf += sizeof conn->sbuf.simple_reply;
    conn->rlen = sizeof conn->sbuf.sr.structured_reply;
    conn->rlen -= sizeof conn->sbuf.simple_reply;
    SET_NEXT_STATE (%STRUCTURED_REPLY.START);
    return 0;
  }
  else {
    SET_NEXT_STATE (%.DEAD); /* We've probably lost synchronization. */
    set_error (0, "invalid reply magic");
    return -1;
  }

 REPLY.SIMPLE_REPLY.START:
  struct command_in_flight *cmd;
  uint32_t error;
  uint64_t handle;

  error = be32toh (conn->sbuf.simple_reply.error);
  handle = be64toh (conn->sbuf.simple_reply.handle);

  /* Find the command amongst the commands in flight. */
  for (cmd = conn->cmds_in_flight; cmd != NULL; cmd = cmd->next) {
    if (cmd->handle == handle)
      break;
  }
  if (cmd == NULL) {
    SET_NEXT_STATE (%.READY);
    set_error (0, "no matching handle found for server reply, "
               "this is probably a bug in the server");
    return -1;
  }

  cmd->error = error;
  if (cmd->error == 0 && cmd->type == NBD_CMD_READ) {
    conn->rbuf = cmd->data;
    conn->rlen = cmd->count;
    SET_NEXT_STATE (%RECV_READ_PAYLOAD);
  }
  else {
    SET_NEXT_STATE (%^FINISH_COMMAND);
  }
  return 0;

 REPLY.SIMPLE_REPLY.RECV_READ_PAYLOAD:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%^FINISH_COMMAND);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.START:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:  SET_NEXT_STATE (%CHECK);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.CHECK:
  struct command_in_flight *cmd;
  uint16_t flags, type;
  uint64_t handle;
  uint32_t length;

  flags = be16toh (conn->sbuf.sr.structured_reply.flags);
  type = be16toh (conn->sbuf.sr.structured_reply.type);
  handle = be64toh (conn->sbuf.sr.structured_reply.handle);
  length = be32toh (conn->sbuf.sr.structured_reply.length);

  /* Find the command amongst the commands in flight. */
  for (cmd = conn->cmds_in_flight; cmd != NULL; cmd = cmd->next) {
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

  if (NBD_REPLY_TYPE_IS_ERR (type)) {
    if (length < sizeof conn->sbuf.sr.payload.error) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "too short length in structured reply error");
      return -1;
    }
    conn->rbuf = &conn->sbuf.sr.payload.error;
    conn->rlen = sizeof conn->sbuf.sr.payload.error;
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
    if (length < sizeof conn->sbuf.sr.payload.offset_data) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "too short length in NBD_REPLY_TYPE_OFFSET_DATA");
      return -1;
    }
    conn->rbuf = &conn->sbuf.sr.payload.offset_data;
    conn->rlen = sizeof conn->sbuf.sr.payload.offset_data;
    SET_NEXT_STATE (%RECV_OFFSET_DATA);
    return 0;
  }
  else if (type == NBD_REPLY_TYPE_OFFSET_HOLE) {
    if (length != 12) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "invalid length in NBD_REPLY_TYPE_NONE");
      return -1;
    }
    conn->rbuf = &conn->sbuf.sr.payload.offset_hole;
    conn->rlen = sizeof conn->sbuf.sr.payload.offset_hole;
    SET_NEXT_STATE (%RECV_OFFSET_HOLE);
    return 0;
  }
  else if (type == NBD_REPLY_TYPE_BLOCK_STATUS) {
    /* XXX Not implemented yet. */
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "NBD_REPLY_TYPE_BLOCK_STATUS not implemented");
    return -1;
  }
  else {
    SET_NEXT_STATE (%.DEAD);
    set_error (0, "unknown structured reply type (%" PRIu16 ")", type);
    return -1;
  }

 REPLY.STRUCTURED_REPLY.RECV_ERROR:
  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    /* We skip the human readable error for now. XXX */
    conn->rbuf = NULL;
    conn->rlen = be16toh (conn->sbuf.sr.payload.error.len);
    SET_NEXT_STATE (%RECV_ERROR_MESSAGE);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.RECV_ERROR_MESSAGE:
  struct command_in_flight *cmd;
  uint16_t flags;
  uint64_t handle;
  uint32_t error;

  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    flags = be16toh (conn->sbuf.sr.structured_reply.flags);
    handle = be64toh (conn->sbuf.sr.structured_reply.handle);
    error = be32toh (conn->sbuf.sr.payload.error.error);

    /* Find the command amongst the commands in flight. */
    for (cmd = conn->cmds_in_flight; cmd != NULL; cmd = cmd->next) {
      if (cmd->handle == handle)
        break;
    }
    assert (cmd); /* guaranteed by CHECK_STRUCTURED_REPLY */

    cmd->error = error;

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

  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    handle = be64toh (conn->sbuf.sr.structured_reply.handle);
    length = be32toh (conn->sbuf.sr.structured_reply.length);
    offset = be64toh (conn->sbuf.sr.payload.offset_data.offset);

    /* Find the command amongst the commands in flight. */
    for (cmd = conn->cmds_in_flight; cmd != NULL; cmd = cmd->next) {
      if (cmd->handle == handle)
        break;
    }
    assert (cmd); /* guaranteed by CHECK_STRUCTURED_REPLY */

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

    if (cmd->data == NULL) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "invalid command for receiving offset-data chunk, "
                 "cmd->type=%" PRIu16 ", "
                 "this is likely to be a bug in the server",
                 cmd->type);
      return -1;
    }

    /* Set up to receive the data directly to the user buffer. */
    conn->rbuf = cmd->data + offset;
    conn->rlen = length;
    SET_NEXT_STATE (%RECV_OFFSET_DATA_DATA);
  }
  return 0;

 REPLY.STRUCTURED_REPLY.RECV_OFFSET_DATA_DATA:
  uint16_t flags;

  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    flags = be16toh (conn->sbuf.sr.structured_reply.flags);

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

  switch (recv_into_rbuf (conn)) {
  case -1: SET_NEXT_STATE (%.DEAD); return -1;
  case 0:
    handle = be64toh (conn->sbuf.sr.structured_reply.handle);
    flags = be16toh (conn->sbuf.sr.structured_reply.flags);
    offset = be64toh (conn->sbuf.sr.payload.offset_hole.offset);
    length = be32toh (conn->sbuf.sr.payload.offset_hole.length);

    /* Find the command amongst the commands in flight. */
    for (cmd = conn->cmds_in_flight; cmd != NULL; cmd = cmd->next) {
      if (cmd->handle == handle)
        break;
    }
    assert (cmd); /* guaranteed by CHECK_STRUCTURED_REPLY */

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

    if (cmd->data == NULL) {
      SET_NEXT_STATE (%.DEAD);
      set_error (0, "invalid command for receiving offset-hole chunk, "
                 "cmd->type=%" PRIu16 ", "
                 "this is likely to be a bug in the server",
                 cmd->type);
      return -1;
    }

    memset (cmd->data + offset, 0, length);

    if (flags & NBD_REPLY_FLAG_DONE)
      SET_NEXT_STATE (%^FINISH_COMMAND);
    else
      SET_NEXT_STATE (%.READY);
  }
  return 0;

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

  /* Move it to the cmds_done list. */
  if (prev_cmd != NULL)
    prev_cmd->next = cmd->next;
  else
    conn->cmds_in_flight = cmd->next;
  cmd->next = conn->cmds_done;
  conn->cmds_done = cmd;

  SET_NEXT_STATE (%.READY);
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
