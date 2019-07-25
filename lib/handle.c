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
#include <signal.h>
#include <assert.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "internal.h"

static void
free_cmd_list (struct command *list)
{
  struct command *cmd, *cmd_next;

  for (cmd = list; cmd != NULL; cmd = cmd_next) {
    cmd_next = cmd->next;
    nbd_internal_retire_and_free_command (cmd);
  }
}

struct nbd_handle *
nbd_create (void)
{
  struct nbd_handle *h;
  const char *s;

  nbd_internal_reset_error ("nbd_create");

  h = calloc (1, sizeof *h);
  if (h == NULL) {
    set_error (errno, "calloc");
    goto error1;
  }

  h->unique = 1;
  h->tls_verify_peer = true;

  s = getenv ("LIBNBD_DEBUG");
  h->debug = s && strcmp (s, "1") == 0;

  h->state = h->public_state = STATE_START;
  h->pid = -1;

  h->export_name = strdup ("");
  if (h->export_name == NULL) {
    set_error (errno, "strdup");
    goto error1;
  }

  errno = pthread_mutex_init (&h->lock, NULL);
  if (errno != 0) {
    set_error (errno, "pthread_mutex_init");
    goto error1;
  }

  if (nbd_internal_run (h, cmd_create) == -1)
    goto error2;

  debug (h, "opening handle");
  return h;

 error2:
  pthread_mutex_destroy (&h->lock);
 error1:
  if (h) {
    free (h->export_name);
    free (h);
  }
  return NULL;
}

void
nbd_close (struct nbd_handle *h)
{
  struct meta_context *m, *m_next;

  nbd_internal_reset_error ("nbd_close");

  debug (h, "closing handle");

  if (h == NULL)
    return;

  /* Free user callbacks first. */
  if (h->debug_fn)
    h->debug_fn (LIBNBD_CALLBACK_FREE, h->debug_data, NULL, NULL);
  h->debug_fn = NULL;

  free (h->bs_entries);
  for (m = h->meta_contexts; m != NULL; m = m_next) {
    m_next = m->next;
    free (m->name);
    free (m);
  }
  free_cmd_list (h->cmds_to_issue);
  free_cmd_list (h->cmds_in_flight);
  free_cmd_list (h->cmds_done);
  nbd_internal_free_string_list (h->argv);
  free (h->unixsocket);
  free (h->hostname);
  free (h->port);
  if (h->result)
    freeaddrinfo (h->result);
  if (h->sock)
    h->sock->ops->close (h->sock);
  if (h->pid > 0)
    waitpid (h->pid, NULL, 0);

  free (h->export_name);
  free (h->tls_certificates);
  free (h->tls_username);
  free (h->tls_psk_file);
  nbd_internal_free_string_list (h->request_meta_contexts);
  pthread_mutex_destroy (&h->lock);
  free (h);
}

int
nbd_unlocked_set_export_name (struct nbd_handle *h, const char *export_name)
{
  char *new_name;

  if (strnlen (export_name, NBD_MAX_STRING + 1) > NBD_MAX_STRING) {
    set_error (ENAMETOOLONG, "export name too long for NBD protocol");
    return -1;
  }

  new_name = strdup (export_name);
  if (!new_name) {
    set_error (errno, "strdup");
    return -1;
  }

  free (h->export_name);
  h->export_name = new_name;
  return 0;
}

char *
nbd_unlocked_get_export_name (struct nbd_handle *h)
{
  char *copy = strdup (h->export_name);

  if (!copy) {
    set_error (errno, "strdup");
    return NULL;
  }

  return copy;
}

int
nbd_unlocked_add_meta_context (struct nbd_handle *h, const char *name)
{
  char *copy;
  size_t len;
  char **list;

  if (strnlen (name, NBD_MAX_STRING + 1) > NBD_MAX_STRING) {
    set_error (ENAMETOOLONG, "meta context name too long for NBD protocol");
    return -1;
  }

  copy = strdup (name);
  if (!copy) {
    set_error (errno, "strdup");
    return -1;
  }
  len = h->request_meta_contexts == NULL ? 0
    : nbd_internal_string_list_length (h->request_meta_contexts);
  list = realloc (h->request_meta_contexts,
                  sizeof (char *) * (len+2 /* + new entry + NULL */));
  if (list == NULL) {
    free (copy);
    set_error (errno, "realloc");
    return -1;
  }
  h->request_meta_contexts = list;
  list[len] = copy;
  list[len+1] = NULL;

  return 0;
}

const char *
nbd_unlocked_get_package_name (struct nbd_handle *h)
{
  return PACKAGE_NAME;
}

const char *
nbd_unlocked_get_version (struct nbd_handle *h)
{
  return PACKAGE_VERSION;
}

int
nbd_unlocked_kill_command (struct nbd_handle *h, int signum)
{
  if (h->pid == -1) {
    set_error (ESRCH, "no subprocess exists");
    return -1;
  }
  assert (h->pid > 0);

  if (signum == 0)
    signum = SIGTERM;
  if (signum < 0) {
    set_error (EINVAL, "invalid signal number: %d", signum);
    return -1;
  }

  if (kill (h->pid, signum) == -1) {
    set_error (errno, "kill");
    return -1;
  }

  return 0;
}

/* NB: is_locked = false, may_set_error = false. */
int
nbd_unlocked_supports_tls (struct nbd_handle *h)
{
#ifdef HAVE_GNUTLS
  return 1;
#else
  return 0;
#endif
}

/* NB: is_locked = false, may_set_error = false. */
int
nbd_unlocked_supports_uri (struct nbd_handle *h)
{
#ifdef HAVE_LIBXML2
  return 1;
#else
  return 0;
#endif
}
