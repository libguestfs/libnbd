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

/* State machines related to connecting to the server. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>

/* STATE MACHINE */ {
 CONNECT.START:
  int fd;

  assert (!h->sock);
  fd = socket (AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC, 0);
  if (fd == -1) {
    SET_NEXT_STATE (%.DEAD);
    set_error (errno, "socket");
    return -1;
  }
  h->sock = nbd_internal_socket_create (fd);
  if (!h->sock) {
    SET_NEXT_STATE (%.DEAD);
    return -1;
  }

  if (connect (fd, (struct sockaddr *) &h->connaddr,
               h->connaddrlen) == -1) {
    if (errno != EINPROGRESS) {
      SET_NEXT_STATE (%.DEAD);
      set_error (errno, "connect");
      return -1;
    }
  }
  return 0;

 CONNECT.CONNECTING:
  int status;
  socklen_t len = sizeof status;

  if (getsockopt (h->sock->ops->get_fd (h->sock),
                  SOL_SOCKET, SO_ERROR, &status, &len) == -1) {
    SET_NEXT_STATE (%.DEAD);
    set_error (errno, "getsockopt: SO_ERROR");
    return -1;
  }
  /* This checks the status of the original connect call. */
  if (status == 0) {
    SET_NEXT_STATE (%^MAGIC.START);
    return 0;
  }
  else {
    SET_NEXT_STATE (%.DEAD);
    set_error (status, "connect");
    return -1;
  }

 CONNECT_TCP.START:
  int r;

  assert (h->hostname != NULL);
  assert (h->port != NULL);

  if (h->result) {
    freeaddrinfo (h->result);
    h->result = NULL;
  }

  memset (&h->hints, 0, sizeof h->hints);
  h->hints.ai_family = AF_UNSPEC;
  h->hints.ai_socktype = SOCK_STREAM;
  h->hints.ai_flags = 0;
  h->hints.ai_protocol = 0;

  /* XXX Unfortunately getaddrinfo blocks.  getaddrinfo_a isn't
   * portable and in any case isn't an alternative because it can't be
   * integrated into a main loop.
   */
  r = getaddrinfo (h->hostname, h->port, &h->hints, &h->result);
  if (r != 0) {
    SET_NEXT_STATE (%^START);
    set_error (0, "getaddrinfo: %s:%s: %s",
               h->hostname, h->port, gai_strerror (r));
    return -1;
  }

  h->rp = h->result;
  SET_NEXT_STATE (%CONNECT);
  return 0;

 CONNECT_TCP.CONNECT:
  int fd;

  assert (!h->sock);

  if (h->rp == NULL) {
    /* We tried all the results from getaddrinfo without success.
     * Save errno from most recent connect(2) call. XXX
     */
    SET_NEXT_STATE (%^START);
    set_error (0, "connect: %s:%s: could not connect to remote host",
               h->hostname, h->port);
    return -1;
  }

  fd = socket (h->rp->ai_family,
               h->rp->ai_socktype|SOCK_NONBLOCK|SOCK_CLOEXEC,
               h->rp->ai_protocol);
  if (fd == -1) {
    SET_NEXT_STATE (%NEXT_ADDRESS);
    return 0;
  }
  h->sock = nbd_internal_socket_create (fd);
  if (!h->sock) {
    SET_NEXT_STATE (%.DEAD);
    return -1;
  }
  if (connect (fd, h->rp->ai_addr, h->rp->ai_addrlen) == -1) {
    if (errno != EINPROGRESS) {
      SET_NEXT_STATE (%NEXT_ADDRESS);
      return 0;
    }
  }

  SET_NEXT_STATE (%CONNECTING);
  return 0;

 CONNECT_TCP.CONNECTING:
  int status;
  socklen_t len = sizeof status;

  if (getsockopt (h->sock->ops->get_fd (h->sock),
                  SOL_SOCKET, SO_ERROR, &status, &len) == -1) {
    SET_NEXT_STATE (%.DEAD);
    set_error (errno, "getsockopt: SO_ERROR");
    return -1;
  }
  /* This checks the status of the original connect call. */
  if (status == 0)
    SET_NEXT_STATE (%^MAGIC.START);
  else
    SET_NEXT_STATE (%NEXT_ADDRESS);
  return 0;

 CONNECT_TCP.NEXT_ADDRESS:
  if (h->sock) {
    h->sock->ops->close (h->sock);
    h->sock = NULL;
  }
  if (h->rp)
    h->rp = h->rp->ai_next;
  SET_NEXT_STATE (%CONNECT);
  return 0;

 CONNECT_COMMAND.START:
  int sv[2];
  pid_t pid;

  assert (!h->sock);
  assert (h->argv);
  assert (h->argv[0]);
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

    execvp (h->argv[0], h->argv);
    perror (h->argv[0]);
    _exit (EXIT_FAILURE);
  }

  /* Parent. */
  h->pid = pid;
  h->sock = nbd_internal_socket_create (sv[0]);
  if (!h->sock) {
    SET_NEXT_STATE (%.DEAD);
    return -1;
  }
  close (sv[1]);

  /* The sockets are connected already, we can jump directly to
   * receiving the server magic.
   */
  SET_NEXT_STATE (%^MAGIC.START);
  return 0;

} /* END STATE MACHINE */
