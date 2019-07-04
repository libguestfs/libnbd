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

/* Test interop with other servers. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>

#include <libnbd.h>

#define SIZE (1024*1024)

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  char tmpfile[] = "/tmp/nbdXXXXXX";
  int fd;
#ifdef SERVE_OVER_TCP
  int port;
  char port_str[16];
  pid_t pid = -1;
#endif
  int64_t actual_size;
  char buf[512];
  int r = -1;

  /* Create a large sparse temporary file. */
  fd = mkstemp (tmpfile);
  if (fd == -1 ||
      ftruncate (fd, SIZE) == -1 ||
      close (fd) == -1) {
    perror (tmpfile);
    goto out;
  }

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    goto out;
  }

  if (nbd_set_export_name (nbd, EXPORT_NAME) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    goto out;
  }

#if CERTS || PSK
  /* Require TLS on the handle and fail if not available or if the
   * handshake fails.
   */
  if (nbd_supports_tls (nbd) != 1) {
    fprintf (stderr, "skip: compiled without TLS support\n");
    exit (77);
  }
  if (nbd_set_tls (nbd, 2) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
#endif

#if CERTS
  if (nbd_set_tls_certificates (nbd, "../tests/pki") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
#elif PSK
  if (nbd_set_tls_psk_file (nbd, "../tests/keys.psk") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
#endif

#ifdef SERVE_OVER_TCP
  /* Pick a port at random, hope it's free. */
  srand (time (NULL) + getpid ());
  port = 32768 + (rand () & 32767);

  snprintf (port_str, sizeof port_str, "%d", port);

  pid = fork ();
  if (pid == -1) {
    perror ("fork");
    goto out;
  }
  if (pid == 0) {
    execlp (SERVER, SERVER, SERVER_PARAMS, NULL);
    perror (SERVER);
    _exit (EXIT_FAILURE);
  }

  /* Unfortunately there's no good way to wait for qemu-nbd to start
   * serving, so ...
   */
  sleep (5);

  if (nbd_connect_tcp (nbd, "localhost", port_str) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    goto out;
  }

#else /* !SERVE_OVER_TCP */

  char *args[] = { SERVER, SERVER_PARAMS, NULL };
  if (nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    goto out;
  }

#endif

  actual_size = nbd_get_size (nbd);
  if (actual_size == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    goto out;
  }
  if (actual_size != SIZE) {
    fprintf (stderr, "%s: actual size %" PRIi64 " <> expected size %d",
             argv[0], actual_size, SIZE);
    goto out;
  }

  if (nbd_pread (nbd, buf, sizeof buf, 0, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    goto out;
  }

  /* XXX In future test more operations here. */

#if !CERTS && !PSK
  /* XXX qemu doesn't shut down the connection nicely (using
   * gnutls_bye) and because of this the following call will fail
   * with:
   *
   * nbd_shutdown: gnutls_record_recv: The TLS connection was
   * non-properly terminated.
   */
  if (nbd_shutdown (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    goto out;
  }
#endif

  nbd_close (nbd);

  r = 0;
 out:
#ifdef SERVE_OVER_TCP
  if (pid > 0)
    kill (pid, SIGTERM);
#endif
  unlink (tmpfile);

  exit (r == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
