/* NBD client library in userspace
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#ifdef HAVE_LINUX_VM_SOCKETS_H
#include <linux/vm_sockets.h>
#endif

#include "internal.h"

static int
error_unless_ready (struct nbd_handle *h)
{
  if (nbd_internal_is_state_ready (get_next_state (h)))
    return 0;

  /* Why did it fail? */
  if (nbd_internal_is_state_closed (get_next_state (h))) {
    set_error (0, "connection is closed");
    return -1;
  }

  if (nbd_internal_is_state_dead (get_next_state (h)))
    /* Don't set the error here, keep the error set when
     * the connection died.
     */
    return -1;

  /* Should probably never happen. */
  set_error (0, "connection in an unexpected state (%s)",
             nbd_internal_state_short_string (get_next_state (h)));
  return -1;
}

int
nbd_internal_wait_until_connected (struct nbd_handle *h)
{
  while (nbd_internal_is_state_connecting (get_next_state (h))) {
    if (nbd_unlocked_poll (h, -1) == -1)
      return -1;
  }

  return error_unless_ready (h);
}

/* Connect to a Unix domain socket. */
int
nbd_unlocked_connect_unix (struct nbd_handle *h, const char *unixsocket)
{
  if (nbd_unlocked_aio_connect_unix (h, unixsocket) == -1)
    return -1;

  return nbd_internal_wait_until_connected (h);
}

/* Connect to a vsock. */
int
nbd_unlocked_connect_vsock (struct nbd_handle *h, uint32_t cid, uint32_t port)
{
  if (nbd_unlocked_aio_connect_vsock (h, cid, port) == -1)
    return -1;

  return nbd_internal_wait_until_connected (h);
}

/* Connect to a TCP port. */
int
nbd_unlocked_connect_tcp (struct nbd_handle *h,
                          const char *hostname, const char *port)
{
  if (nbd_unlocked_aio_connect_tcp (h, hostname, port) == -1)
    return -1;

  return nbd_internal_wait_until_connected (h);
}

/* Connect to a connected socket. */
int
nbd_unlocked_connect_socket (struct nbd_handle *h, int sock)
{
  if (nbd_unlocked_aio_connect_socket (h, sock) == -1)
    return -1;

  return nbd_internal_wait_until_connected (h);
}

/* Connect to a local command. */
int
nbd_unlocked_connect_command (struct nbd_handle *h, char **argv)
{
  if (nbd_unlocked_aio_connect_command (h, argv) == -1)
    return -1;

  return nbd_internal_wait_until_connected (h);
}

/* Connect to a local command, use systemd socket activation. */
int
nbd_unlocked_connect_systemd_socket_activation (struct nbd_handle *h,
                                                char **argv)
{
  if (nbd_unlocked_aio_connect_systemd_socket_activation (h, argv) == -1)
    return -1;

  return nbd_internal_wait_until_connected (h);
}

int
nbd_unlocked_aio_connect (struct nbd_handle *h,
                          const struct sockaddr *addr, socklen_t len)
{
  memcpy (&h->connaddr, addr, len);
  h->connaddrlen = len;

  return nbd_internal_run (h, cmd_connect_sockaddr);
}

int
nbd_unlocked_aio_connect_unix (struct nbd_handle *h, const char *unixsocket)
{
  struct sockaddr_un sun = { .sun_family = AF_UNIX };
  socklen_t len;
  size_t namelen;

  namelen = strlen (unixsocket);
  if (namelen > sizeof sun.sun_path) {
    set_error (ENAMETOOLONG, "socket name too long: %s", unixsocket);
    return -1;
  }
  memcpy (sun.sun_path, unixsocket, namelen);
  len = sizeof sun;

  memcpy (&h->connaddr, &sun, len);
  h->connaddrlen = len;

  return nbd_internal_run (h, cmd_connect_sockaddr);
}

int
nbd_unlocked_aio_connect_vsock (struct nbd_handle *h,
                                uint32_t cid, uint32_t port)
{
#ifdef AF_VSOCK
  struct sockaddr_vm svm = {
    .svm_family = AF_VSOCK,
    .svm_cid = cid,
    .svm_port = port,
  };
  const socklen_t len = sizeof svm;

  memcpy (&h->connaddr, &svm, len);
  h->connaddrlen = len;

  return nbd_internal_run (h, cmd_connect_sockaddr);
#else
  set_error (ENOTSUP, "AF_VSOCK protocol is not supported");
  return -1;
#endif
}

int
nbd_unlocked_aio_connect_tcp (struct nbd_handle *h,
                              const char *hostname, const char *port)
{
  if (h->hostname)
    free (h->hostname);
  h->hostname = strdup (hostname);
  if (!h->hostname) {
    set_error (errno, "strdup");
    return -1;
  }
  if (h->port)
    free (h->port);
  h->port = strdup (port);
  if (!h->port) {
    set_error (errno, "strdup");
    return -1;
  }

  return nbd_internal_run (h, cmd_connect_tcp);
}

int
nbd_unlocked_aio_connect_socket (struct nbd_handle *h, int sock)
{
  int flags;

  /* Set O_NONBLOCK on the file and FD_CLOEXEC on the file descriptor.
   * We can't trust that the calling process did either of these.
   */
  flags = fcntl (sock, F_GETFL, 0);
  if (flags == -1 ||
      fcntl (sock, F_SETFL, flags|O_NONBLOCK) == -1) {
    set_error (errno, "fcntl: set O_NONBLOCK");
    close (sock);
    return -1;
  }

  flags = fcntl (sock, F_GETFD, 0);
  if (flags == -1 ||
      fcntl (sock, F_SETFD, flags|FD_CLOEXEC) == -1) {
    set_error (errno, "fcntl: set FD_CLOEXEC");
    close (sock);
    return -1;
  }

  h->sock = nbd_internal_socket_create (sock);
  if (!h->sock) {
    close (sock);
    return -1;
  }

  /* This jumps straight to %MAGIC.START (to start the handshake)
   * because the socket is already connected.
   */
  return nbd_internal_run (h, cmd_connect_socket);
}

int
nbd_unlocked_aio_connect_command (struct nbd_handle *h, char **argv)
{
  char **copy;

  copy = nbd_internal_copy_string_list (argv);
  if (!copy) {
    set_error (errno, "copy_string_list");
    return -1;
  }

  if (h->argv)
    nbd_internal_free_string_list (h->argv);
  h->argv = copy;

  return nbd_internal_run (h, cmd_connect_command);
}

int
nbd_unlocked_aio_connect_systemd_socket_activation (struct nbd_handle *h,
                                                    char **argv)
{
  char **copy;

  copy = nbd_internal_copy_string_list (argv);
  if (!copy) {
    set_error (errno, "copy_string_list");
    return -1;
  }

  if (h->argv)
    nbd_internal_free_string_list (h->argv);
  h->argv = copy;

  return nbd_internal_run (h, cmd_connect_sa);
}
