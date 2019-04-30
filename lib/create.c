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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include "internal.h"

static struct nbd_connection *
create_conn (struct nbd_handle *h)
{
  struct nbd_connection *conn;

  conn = calloc (1, sizeof *conn);
  if (conn == NULL)
    return NULL;

  conn->state = STATE_CREATED;
  conn->fd = -1;
  conn->h = h;

  if (nbd_internal_run (h, conn, cmd_create) == -1) {
    free (conn);
    return NULL;
  }

  return conn;
}

struct nbd_handle *
nbd_create (void)
{
  struct nbd_handle *h;
  struct nbd_connection *conn;

  h = calloc (1, sizeof *h);
  if (h == NULL) goto error;

  h->export = strdup ("");
  if (h->export == NULL) goto error;

  h->conns = malloc (sizeof (struct nbd_connection *));
  if (h->conns == NULL) goto error;

  /* Handles are created with a single connection. */
  conn = create_conn (h);
  if (conn == NULL) goto error;

  h->conns[0] = conn;
  h->multi_conn = 1;
  return h;

 error:
  if (h) {
    free (h->export);
    free (h->conns);
    free (h);
  }
  return NULL;
}
