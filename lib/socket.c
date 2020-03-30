/* NBD client library in userspace
 * Copyright (C) 2013-2020 Red Hat Inc.
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

/* struct socket_ops for a plain ordinary Berkeley socket. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "internal.h"

static ssize_t
socket_recv (struct nbd_handle *h, struct socket *sock, void *buf, size_t len)
{
  ssize_t r;

  r = recv (sock->u.fd, buf, len, 0);
  if (r == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    set_error (errno, "recv");
  return r;
}

static ssize_t
socket_send (struct nbd_handle *h,
             struct socket *sock, const void *buf, size_t len, int flags)
{
  ssize_t r;

  /* We don't want to die from SIGPIPE, but also don't want to force a
   * changed signal handler on the rest of the application.
   */
  flags |= MSG_NOSIGNAL;

  r = send (sock->u.fd, buf, len, flags);
  if (r == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    set_error (errno, "send");
  return r;
}

static int
socket_get_fd (struct socket *sock)
{
  return sock->u.fd;
}

static bool
socket_shut_writes (struct nbd_handle *h, struct socket *sock)
{
  if (shutdown (sock->u.fd, SHUT_WR) == -1)
    debug (h, "ignoring shutdown failure: %s", strerror (errno));
  /* Regardless of any errors, we don't need to retry. */
  return true;
}

static int
socket_close (struct socket *sock)
{
  int r = close (sock->u.fd);

  free (sock);
  return r;
}

static struct socket_ops socket_ops = {
  .recv = socket_recv,
  .send = socket_send,
  .get_fd = socket_get_fd,
  .shut_writes = socket_shut_writes,
  .close = socket_close,
};

struct socket *
nbd_internal_socket_create (int fd)
{
  struct socket *sock;

  sock = malloc (sizeof *sock);
  if (sock == NULL) {
    set_error (errno, "malloc");
    return NULL;
  }
  sock->u.fd = fd;
  sock->ops = &socket_ops;
  return sock;
}
