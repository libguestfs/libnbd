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
#include <sys/un.h>

#include "internal.h"

static int
wait_all_connected (struct nbd_handle *h)
{
  size_t i;

  for (;;) {
    bool all_connected = true;

    /* Are any not yet connected? */
    for (i = 0; i < h->multi_conn; ++i) {
      if (nbd_unlocked_aio_is_closed (h->conns[i])) {
        set_error (0, "connection is closed");
        return -1;
      }
      if (nbd_unlocked_aio_is_dead (h->conns[i])) {
        /* Don't set the error here, keep the error set when
         * the connection died.
         */
        return -1;
      }
      if (!nbd_unlocked_aio_is_ready (h->conns[i])) {
        all_connected = false;
        break;
      }
    }

    if (all_connected)
      break;

    if (nbd_unlocked_poll (h, -1) == -1)
      return -1;
  }

  return 0;
}

/* For all connections in the CREATED state, start connecting them to
 * a Unix domain socket.  Wait until all connections are in the READY
 * state.
 */
int
nbd_unlocked_connect_unix (struct nbd_handle *h, const char *sockpath)
{
  size_t i;
  struct sockaddr_un sun;
  socklen_t len;
  bool started;

  sun.sun_family = AF_UNIX;
  memset (sun.sun_path, 0, sizeof (sun.sun_path));
  strncpy (sun.sun_path, sockpath, sizeof (sun.sun_path) - 1);
  len = sizeof (sun.sun_family) + strlen (sun.sun_path) + 1;

  started = false;
  for (i = 0; i < h->multi_conn; ++i) {
    if (h->conns[i]->state == STATE_START) {
      if (nbd_unlocked_aio_connect (h->conns[i],
                                    (struct sockaddr *) &sun, len) == -1)
        return -1;
      started = true;
    }
  }

  if (!started) {
    set_error (0, "no connections in this handle were in the created state, this is likely to be caused by a programming error in the calling program");
    return -1;
  }

  return wait_all_connected (h);
}

/* Connect to a TCP port. */
int
nbd_unlocked_connect_tcp (struct nbd_handle *h,
                          const char *hostname, const char *port)
{
  size_t i;
  bool started;

  started = false;
  for (i = 0; i < h->multi_conn; ++i) {
    if (h->conns[i]->state == STATE_START) {
      if (nbd_unlocked_aio_connect_tcp (h->conns[i], hostname, port) == -1)
        return -1;
      started = true;
    }
  }

  if (!started) {
    set_error (0, "no connections in this handle were in the created state, this is likely to be caused by a programming error in the calling program");
    return -1;
  }

  return wait_all_connected (h);
}

/* Connect to a local command.  Multi-conn doesn't make much sense
 * here, should it be an error?
 */
int
nbd_unlocked_connect_command (struct nbd_handle *h, char **argv)
{
  if (h->conns[0]->state != STATE_START) {
    set_error (0, "first connection in this handle is not in the created state, this is likely to be caused by a programming error in the calling program");
    return -1;
  }

  if (nbd_unlocked_aio_connect_command (h->conns[0], argv) == -1)
    return -1;

  for (;;) {
    if (nbd_unlocked_aio_is_closed (h->conns[0])) {
      set_error (0, "connection is closed");
      return -1;
    }
    if (nbd_unlocked_aio_is_dead (h->conns[0])) {
      /* Don't set the error here, keep the error set when
       * the connection died.
       */
      return -1;
    }
    if (nbd_unlocked_aio_is_ready (h->conns[0]))
      break;

    if (nbd_unlocked_poll (h, -1) == -1)
      return -1;
  }

  return 0;
}

int
nbd_unlocked_aio_connect (struct nbd_connection *conn,
                          const struct sockaddr *addr, socklen_t len)
{
  memcpy (&conn->connaddr, addr, len);
  conn->connaddrlen = len;

  return nbd_internal_run (conn->h, conn, cmd_connect_sockaddr);
}

int
nbd_unlocked_aio_connect_tcp (struct nbd_connection *conn,
                              const char *hostname, const char *port)
{
  if (conn->hostname)
    free (conn->hostname);
  conn->hostname = strdup (hostname);
  if (!conn->hostname) {
    set_error (errno, "strdup");
    return -1;
  }
  if (conn->port)
    free (conn->port);
  conn->port = strdup (port);
  if (!conn->port) {
    set_error (errno, "strdup");
    return -1;
  }

  return nbd_internal_run (conn->h, conn, cmd_connect_tcp);
}

int
nbd_unlocked_aio_connect_command (struct nbd_connection *conn,
                                  char **argv)
{
  char **copy;

  copy = nbd_internal_copy_string_list (argv);
  if (!copy) {
    set_error (errno, "copy_string_list");
    return -1;
  }

  if (conn->argv)
    nbd_internal_free_string_list (conn->argv);
  conn->argv = copy;

  return nbd_internal_run (conn->h, conn, cmd_connect_command);
}
