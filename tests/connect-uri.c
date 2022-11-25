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

/* Test connecting over an NBD URI. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include <libnbd.h>

#include "requires.h"

#ifdef NEEDS_UNIX_SOCKET
#define UNIX_SOCKET tmp
static char tmp[] = "/tmp/nbdXXXXXX";

static void
unlink_unix_socket (void)
{
  unlink (UNIX_SOCKET);
}
#endif /* NEEDS_UNIX_SOCKET */

static int compare_uris (const char *uri1, const char *uri2);

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  pid_t pid;
  size_t i;
  char *get_uri;
#ifdef NEEDS_UNIX_SOCKET
  char *uri;
#else
  const char *uri = URI;
#endif

  /* If SERVER_PARAMS contains --tls-verify-peer we must make sure
   * that nbdkit supports that option.
   */
#ifdef REQUIRES_NBDKIT_TLS_VERIFY_PEER
  requires ("nbdkit --tls-verify-peer -U - null --run 'exit 0'");
#endif

#ifdef NEEDS_UNIX_SOCKET
  int fd = mkstemp (UNIX_SOCKET);
  if (fd == -1 ||
      close (fd) == -1) {
    perror (UNIX_SOCKET);
    exit (EXIT_FAILURE);
  }
  /* We have to remove the temporary file first, since we will create
   * a socket in its place, and ensure the socket is removed on exit.
   */
  unlink_unix_socket ();
  atexit (unlink_unix_socket);

  /* uri = URI + UNIX_SOCKET */
  if (asprintf (&uri, "%s%s", URI, UNIX_SOCKET) == -1) {
    perror ("asprintf");
    exit (EXIT_FAILURE);
  }
#endif

  unlink (PIDFILE);

  pid = fork ();
  if (pid == -1) {
    perror ("fork");
    exit (EXIT_FAILURE);
  }
  if (pid == 0) {
    execlp ("nbdkit",
            "nbdkit", "-f", "-v", "--exit-with-parent",
//          "-D", "nbdkit.tls.log=99",
            "-P", PIDFILE,
            SERVER_PARAMS,
            "null", NULL);
    perror ("nbdkit");
    _exit (EXIT_FAILURE);
  }

  /* Wait for nbdkit to start listening. */
  for (i = 0; i < 60; ++i) {
    if (access (PIDFILE, F_OK) == 0)
      break;
    sleep (1);
  }
  unlink (PIDFILE);

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_supports_uri (nbd) != 1) {
    fprintf (stderr, "skip: compiled without URI support\n");
    exit (77);
  }

  nbd_set_uri_allow_local_file (nbd, true);

  if (nbd_connect_uri (nbd, uri) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Check we negotiated the right kind of connection. */
  if (strncmp (uri, "nbds", 4) == 0) {
    if (! nbd_get_tls_negotiated (nbd)) {
      fprintf (stderr, "%s: failed to negotiate a TLS connection\n",
               argv[0]);
      exit (EXIT_FAILURE);
    }
  }

  /* Usually the URI returned by nbd_get_uri should be the same as the
   * one passed to nbd_connect_uri, or at least it will be in our test
   * cases.
   */
  get_uri = nbd_get_uri (nbd);
  if (get_uri == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (compare_uris (uri, get_uri) != 0) {
    fprintf (stderr, "%s: connect URI %s != get URI %s\n",
             argv[0], uri, get_uri);
    exit (EXIT_FAILURE);
  }
  free (get_uri);

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
#ifdef NEEDS_UNIX_SOCKET
  free (uri);
#endif
  exit (EXIT_SUCCESS);
}

/* Naive comparison of two URIs, enough to get the tests to pass but
 * it does not take into account things like quoting.  The difference
 * between the URI we set and the one we read back is the order of
 * query fields.
 */
static int
compare_uris (const char *uri1, const char *uri2)
{
  size_t n;
  int r;

  /* Compare the parts before the query fields. */
  n = strcspn (uri1, "?");
  r = strncmp (uri1, uri2, n);
  if (r != 0) return r;

  if (strlen (uri1) == n)
    return 0;
  uri1 += n + 1;
  uri2 += n + 1;

  /* Compare each query field in the first URI and ensure it appears
   * in the second URI.  Note the first URI is the one we passed to
   * libnbd, we're not worried about extra fields in the second URI.
   */
  while (*uri1) {
    char *q;

    n = strcspn (uri1, "&");
    q = strndup (uri1, n);
    if (q == NULL) { perror ("strndup"); exit (EXIT_FAILURE); }
    if (strstr (uri2, q) != NULL)
      r = 0;
    else {
      fprintf (stderr, "error: compare_uris: query string '%s' does not appear in returned URI\n", q);
      r = 1;
    }
    free (q);
    if (r != 0)
      return r;
    if (strlen (uri1) == n)
      return 0;
    uri1 += n + 1;
  }

  return 0;
}
