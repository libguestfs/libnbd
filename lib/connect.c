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
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "internal.h"

static int
error_unless_ready (struct nbd_handle *h)
{
  if (nbd_unlocked_aio_is_ready (h))
    return 0;

  /* Why did it fail? */
  if (nbd_unlocked_aio_is_closed (h)) {
    set_error (0, "connection is closed");
    return -1;
  }

  if (nbd_unlocked_aio_is_dead (h))
    /* Don't set the error here, keep the error set when
     * the connection died.
     */
    return -1;

  /* Should probably never happen. */
  set_error (0, "connection in an unexpected state (%s)",
             nbd_internal_state_short_string (h->state));
  return -1;
}

static int
wait_until_connected (struct nbd_handle *h)
{
  while (nbd_unlocked_aio_is_connecting (h)) {
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

  return wait_until_connected (h);
}

/* Connect to a TCP port. */
int
nbd_unlocked_connect_tcp (struct nbd_handle *h,
                          const char *hostname, const char *port)
{
  if (nbd_unlocked_aio_connect_tcp (h, hostname, port) == -1)
    return -1;

  return wait_until_connected (h);
}

/* Connect to a local command. */
int
nbd_unlocked_connect_command (struct nbd_handle *h, char **argv)
{
  if (nbd_unlocked_aio_connect_command (h, argv) == -1)
    return -1;

  return wait_until_connected (h);
}

static int
error_unless_start (struct nbd_handle *h)
{
  if (!nbd_unlocked_aio_is_created (h)) {
    set_error (EINVAL, "connection is not in the initially created state, "
               "this is likely to be caused by a programming error "
               "in the calling program");
    return -1;
  }

  return 0;
}

int
nbd_unlocked_aio_connect (struct nbd_handle *h,
                          const struct sockaddr *addr, socklen_t len)
{
  if (error_unless_start (h) == -1)
    return -1;

  memcpy (&h->connaddr, addr, len);
  h->connaddrlen = len;

  return nbd_internal_run (h, cmd_connect_sockaddr);
}

int
nbd_unlocked_aio_connect_unix (struct nbd_handle *h, const char *unixsocket)
{
  if (error_unless_start (h) == -1)
    return -1;

  if (h->unixsocket)
    free (h->unixsocket);
  h->unixsocket = strdup (unixsocket);
  if (!h->unixsocket) {
    set_error (errno, "strdup");
    return -1;
  }

  return nbd_internal_run (h, cmd_connect_unix);
}

int
nbd_unlocked_aio_connect_tcp (struct nbd_handle *h,
                              const char *hostname, const char *port)
{
  if (error_unless_start (h) == -1)
    return -1;

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
nbd_unlocked_aio_connect_command (struct nbd_handle *h, char **argv)
{
  char **copy;

  if (error_unless_start (h) == -1)
    return -1;

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
