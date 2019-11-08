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
  static _Atomic int hnums = 1;
  struct nbd_handle *h;
  const char *s;

  nbd_internal_set_error_context ("nbd_create");

  h = calloc (1, sizeof *h);
  if (h == NULL) {
    set_error (errno, "calloc");
    goto error1;
  }

  if (asprintf (&h->hname, "nbd%d", hnums++) == -1) {
    set_error (errno, "asprintf");
    goto error1;
  }

  h->unique = 1;
  h->tls_verify_peer = true;
  h->request_sr = true;

  h->uri_allow_transports = (uint32_t) -1;
  h->uri_allow_tls = LIBNBD_TLS_ALLOW;
  h->uri_allow_local_file = false;

  h->gflags = (LIBNBD_HANDSHAKE_FLAG_FIXED_NEWSTYLE |
               LIBNBD_HANDSHAKE_FLAG_NO_ZEROES);

  s = getenv ("LIBNBD_DEBUG");
  h->debug = s && strcmp (s, "1") == 0;

  h->public_state = STATE_START;
  h->state = STATE_START;
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
  /*debug (h, "sizeof *h = %zu", sizeof *h);*/
  return h;

 error2:
  pthread_mutex_destroy (&h->lock);
 error1:
  if (h) {
    free (h->export_name);
    free (h->hname);
    free (h);
  }
  return NULL;
}

void
nbd_close (struct nbd_handle *h)
{
  struct meta_context *m, *m_next;

  nbd_internal_set_error_context ("nbd_close");

  if (h == NULL)
    return;

  debug (h, "closing handle");

  /* Free user callbacks first. */
  nbd_unlocked_clear_debug_callback (h);

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
  if (h->sa_sockpath) {
    if (h->pid > 0)
      kill (h->pid, SIGTERM);
    unlink (h->sa_sockpath);
    free (h->sa_sockpath);
  }
  if (h->sa_tmpdir) {
    rmdir (h->sa_tmpdir);
    free (h->sa_tmpdir);
  }
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
  free (h->hname);
  pthread_mutex_destroy (&h->lock);
  free (h);
}

int
nbd_unlocked_set_handle_name (struct nbd_handle *h, const char *handle_name)
{
  char *new_name;

  new_name = strdup (handle_name);
  if (!new_name) {
    set_error (errno, "strdup");
    return -1;
  }

  free (h->hname);
  h->hname = new_name;
  return 0;
}

char *
nbd_unlocked_get_handle_name (struct nbd_handle *h)
{
  char *copy = strdup (h->hname);

  if (!copy) {
    set_error (errno, "strdup");
    return NULL;
  }

  return copy;
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

int
nbd_unlocked_set_request_structured_replies (struct nbd_handle *h,
                                             bool request)
{
  h->request_sr = request;
  return 0;
}

/* NB: may_set_error = false. */
int
nbd_unlocked_get_request_structured_replies (struct nbd_handle *h)
{
  return h->request_sr;
}

int
nbd_unlocked_get_structured_replies_negotiated (struct nbd_handle *h)
{
  return h->structured_replies;
}

int
nbd_unlocked_set_handshake_flags (struct nbd_handle *h,
                                  uint32_t flags)
{
  /* The generator already ensured flags was in range. */
  h->gflags = flags;
  return 0;
}

/* NB: may_set_error = false. */
uint32_t
nbd_unlocked_get_handshake_flags (struct nbd_handle *h)
{
  return h->gflags;
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
nbd_unlocked_kill_subprocess (struct nbd_handle *h, int signum)
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

const char *
nbd_unlocked_get_protocol (struct nbd_handle *h)
{
  /* I believe that if we reach the Connected or Closed permitted
   * states, then the state machine must have set h->protocol.  So if
   * this assertion is hit then it indicates a bug in libnbd.
   */
  assert (h->protocol);

  return h->protocol;
}

int
nbd_unlocked_set_uri_allow_transports (struct nbd_handle *h, uint32_t mask)
{
  h->uri_allow_transports = mask;
  return 0;
}

int
nbd_unlocked_set_uri_allow_tls (struct nbd_handle *h, int tls)
{
  h->uri_allow_tls = tls;
  return 0;
}

int
nbd_unlocked_set_uri_allow_local_file (struct nbd_handle *h, bool allow)
{
  h->uri_allow_local_file = allow;
  return 0;
}
