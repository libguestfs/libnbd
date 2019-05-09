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
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "internal.h"

static struct nbd_connection *
create_conn (struct nbd_handle *h)
{
  struct nbd_connection *conn;

  conn = calloc (1, sizeof *conn);
  if (conn == NULL)
    return NULL;

  conn->id = h->unique++;
  conn->state = STATE_CREATED;
  conn->fd = -1;
  conn->pid = -1;
  conn->h = h;

  if (nbd_internal_run (h, conn, cmd_create) == -1) {
    free (conn);
    return NULL;
  }

  return conn;
}

static void
free_cmd_list (struct command_in_flight *list)
{
  struct command_in_flight *cmd, *cmd_next;

  for (cmd = list; cmd != NULL; cmd = cmd_next) {
    cmd_next = cmd->next;
    free (cmd);
  }
}

static void
close_conn (struct nbd_connection *conn)
{
  free_cmd_list (conn->cmds_to_issue);
  free_cmd_list (conn->cmds_in_flight);
  free_cmd_list (conn->cmds_done);
  free (conn->command);
  free (conn->hostname);
  free (conn->port);
  if (conn->result)
    freeaddrinfo (conn->result);
  if (conn->fd >= 0)
    close (conn->fd);
  if (conn->pid >= 0) /* XXX kill it? */
    waitpid (conn->pid, NULL, 0);
  free (conn);
}

struct nbd_handle *
nbd_create (void)
{
  struct nbd_handle *h;
  struct nbd_connection *conn;

  h = calloc (1, sizeof *h);
  if (h == NULL) goto error1;

  errno = pthread_mutex_init (&h->lock, NULL);
  if (errno != 0)
    goto error1;

  h->export = strdup ("");
  if (h->export == NULL) goto error2;

  h->conns = malloc (sizeof (struct nbd_connection *));
  if (h->conns == NULL) goto error2;

  /* Handles are created with a single connection. */
  conn = create_conn (h);
  if (conn == NULL) goto error2;

  h->conns[0] = conn;
  h->multi_conn = 1;
  h->unique = 1;
  return h;

 error2:
  pthread_mutex_destroy (&h->lock);
 error1:
  if (h) {
    free (h->export);
    free (h->conns);
    free (h);
  }
  return NULL;
}

void
nbd_close (struct nbd_handle *h)
{
  size_t i;

  for (i = 0; i < h->multi_conn; ++i)
    close_conn (h->conns[i]);
  free (h->export);
  pthread_mutex_destroy (&h->lock);
  free (h);
}

struct nbd_connection *
nbd_get_connection (struct nbd_handle *h, unsigned i)
{
  struct nbd_connection *conn;

  pthread_mutex_lock (&h->lock);

  if (i >= h->multi_conn) {
    set_error (0, "nbd_get_connection: %u > number of connections", i);
    pthread_mutex_unlock (&h->lock);
    return NULL;
  }

  conn = h->conns[i];
  pthread_mutex_unlock (&h->lock);
  return conn;
}

int
nbd_connection_close (struct nbd_connection *conn)
{
  struct nbd_handle *h;
  size_t i;

  if (conn == NULL)
    return 0;

  h = conn->h;
  pthread_mutex_lock (&h->lock);

  /* We have to find the connection in the list of connections of the
   * parent handle, so we can drop the old pointer and create a new
   * connection.
   */
  for (i = 0; i < h->multi_conn; ++i)
    if (conn == h->conns[i])
      goto found;

  /* This should never happen (I think?) so it may be appropriate
   * to abort here.
   */
  set_error (EINVAL, "connection not found in parent handle");
  pthread_mutex_unlock (&h->lock);
  return -1;

 found:
  h->conns[i] = create_conn (h);
  if (h->conns[i] == NULL) {
    set_error (errno, "create_conn");
    pthread_mutex_unlock (&h->lock);
    return -1;
  }

  close_conn (conn);
  pthread_mutex_unlock (&h->lock);
  return 0;
}

int
nbd_unlocked_set_multi_conn (struct nbd_handle *h, unsigned multi_conn)
{
  struct nbd_connection **new_conns;
  size_t i;

  if (multi_conn < 1) {
    set_error (EINVAL, "multi_conn parameter must be >= 1");
    return -1;
  }

  /* We try to be careful to leave the handle in a valid state even if
   * a memory allocation fails.
   */
  new_conns = realloc (h->conns, multi_conn * sizeof (struct nbd_connection *));
  if (new_conns == NULL) {
    set_error (errno, "realloc");
    return -1;
  }

  /* If we're growing the array, allocate the new connections. */
  for (i = h->multi_conn; i < multi_conn; ++i) {
    new_conns[i] = create_conn (h);
    if (new_conns[i] == NULL) {
      set_error (errno, "create_conn");
      for (--i; i >= h->multi_conn; --i)
        close_conn (new_conns[i]);
      return -1;
    }
  }

  /* If we're reducing the array, close the extra connections. */
  for (i = multi_conn; i < h->multi_conn; ++i)
    close_conn (h->conns[i]);

  h->conns = new_conns;
  h->multi_conn = multi_conn;
  return 0;
}
