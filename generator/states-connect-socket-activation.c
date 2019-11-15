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

/* State machine related to connecting with systemd socket activation. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "internal.h"

/* This is baked into the systemd socket activation API. */
#define FIRST_SOCKET_ACTIVATION_FD 3

/* == strlen ("LISTEN_PID=") | strlen ("LISTEN_FDS=") */
#define PREFIX_LENGTH 11

extern char **environ;

/* Prepare environment for calling execvp when doing systemd socket
 * activation.  Takes the current environment and copies it.  Removes
 * any existing LISTEN_PID or LISTEN_FDS and replaces them with new
 * variables.  env[0] is "LISTEN_PID=..." which is filled in by
 * CONNECT_SA.START, and env[1] is "LISTEN_FDS=1".
 */
static char **
prepare_socket_activation_environment (void)
{
  char **env = NULL;
  char *p0 = NULL, *p1 = NULL;
  size_t i, len;
  void *vp;

  p0 = strdup ("LISTEN_PID=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
  if (p0 == NULL)
    goto err;
  p1 = strdup ("LISTEN_FDS=1");
  if (p1 == NULL)
    goto err;

  /* Copy the current environment. */
  env = nbd_internal_copy_string_list (environ);
  if (env == NULL)
    goto err;

  /* Reserve slots env[0] and env[1]. */
  len = nbd_internal_string_list_length (env);
  vp = realloc (env,
                sizeof (char *) * (len + 3)); /* include final NULL entry */
  if (vp == NULL)
    goto err;
  env = vp;
  memmove (&env[2], &env[0], sizeof (char *) * (len + 1));

  env[0] = p0;          /* Code below assumes env[0] is LISTEN_PID. */
  env[1] = p1;

  /* Remove any existing LISTEN_PID or LISTEN_FDS instances. */
  for (i = 2; env[i] != NULL; ++i) {
    if (strncmp (env[i], "LISTEN_PID=", PREFIX_LENGTH) == 0 ||
        strncmp (env[i], "LISTEN_FDS=", PREFIX_LENGTH) == 0) {
      memmove (&env[i], &env[i+1],
               sizeof (char *) * (nbd_internal_string_list_length (&env[i])));
      i--;
    }
  }

  return env;

 err:
  set_error (errno, "malloc");
  nbd_internal_free_string_list (env);
  free (p0);
  free (p1);
  return NULL;
}

STATE_MACHINE {
 CONNECT_SA.START:
  int s;
  struct sockaddr_un addr;
  char **env;
  pid_t pid;
  int flags;

  assert (!h->sock);
  assert (h->argv);
  assert (h->argv[0]);

  /* Use /tmp instead of TMPDIR because we must ensure the path is
   * short enough to store in the sockaddr_un.  On some platforms this
   * may cause problems so we may need to revisit it.  XXX
   */
  h->sa_tmpdir = strdup ("/tmp/libnbdXXXXXX");
  if (h->sa_tmpdir == NULL) {
    SET_NEXT_STATE (%.DEAD);
    set_error (errno, "strdup");
    return 0;
  }
  if (mkdtemp (h->sa_tmpdir) == NULL) {
    SET_NEXT_STATE (%.DEAD);
    set_error (errno, "mkdtemp");
    /* Avoid cleanup in nbd_close. */
    free (h->sa_tmpdir);
    h->sa_tmpdir = NULL;
    return 0;
  }

  if (asprintf (&h->sa_sockpath, "%s/sock", h->sa_tmpdir) == -1) {
    SET_NEXT_STATE (%.DEAD);
    set_error (errno, "strdup");
    return 0;
  }

  s = socket (AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0);
  if (s == -1) {
    SET_NEXT_STATE (%.DEAD);
    set_error (errno, "socket");
    return 0;
  }

  addr.sun_family = AF_UNIX;
  memcpy (addr.sun_path, h->sa_sockpath, strlen (h->sa_sockpath) + 1);
  if (bind (s, (struct sockaddr *) &addr, sizeof addr) == -1) {
    SET_NEXT_STATE (%.DEAD);
    set_error (errno, "bind: %s", h->sa_sockpath);
    close (s);
    return 0;
  }

  if (listen (s, 1) == -1) {
    SET_NEXT_STATE (%.DEAD);
    set_error (errno, "listen");
    close (s);
    return 0;
  }

  env = prepare_socket_activation_environment ();
  if (!env) {
    SET_NEXT_STATE (%.DEAD);
    close (s);
    return 0;
  }

  pid = fork ();
  if (pid == -1) {
    SET_NEXT_STATE (%.DEAD);
    set_error (errno, "fork");
    close (s);
    nbd_internal_free_string_list (env);
    return 0;
  }
  if (pid == 0) {         /* child - run command */
    if (s != FIRST_SOCKET_ACTIVATION_FD) {
      dup2 (s, FIRST_SOCKET_ACTIVATION_FD);
      close (s);
    }
    else {
      /* We must unset CLOEXEC on the fd.  (dup2 above does this
       * implicitly because CLOEXEC is set on the fd, not on the
       * socket).
       */
      flags = fcntl (s, F_GETFD, 0);
      if (flags == -1) {
        nbd_internal_fork_safe_perror ("fcntl: F_GETFD");
        _exit (126);
      }
      if (fcntl (s, F_SETFD, flags & ~FD_CLOEXEC) == -1) {
        nbd_internal_fork_safe_perror ("fcntl: F_SETFD");
        _exit (126);
      }
    }

    char buf[32];
    const char *v =
      nbd_internal_fork_safe_itoa ((long) getpid (), buf, sizeof buf);
    strcpy (&env[0][PREFIX_LENGTH], v);

    /* Restore SIGPIPE back to SIG_DFL. */
    signal (SIGPIPE, SIG_DFL);

    environ = env;
    execvp (h->argv[0], h->argv);
    nbd_internal_fork_safe_perror (h->argv[0]);
    if (errno == ENOENT)
      _exit (127);
    else
      _exit (126);
  }

  /* Parent. */
  close (s);
  nbd_internal_free_string_list (env);
  h->pid = pid;

  h->connaddrlen = sizeof addr;
  memcpy (&h->connaddr, &addr, h->connaddrlen);
  SET_NEXT_STATE (%^CONNECT.START);
  return 0;
} /* END STATE MACHINE */
