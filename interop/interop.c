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
#include <errno.h>
#include <sys/types.h>

#include <libnbd.h>

#define SIZE (1024*1024)

#if CERTS || PSK
#define TLS 1
#ifndef TLS_MODE
#error "TLS_MODE must be defined when using CERTS || PSK"
#endif
#endif

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  char tmpfile[] = "/tmp/nbdXXXXXX";
  char *args[] = { SERVER, SERVER_PARAMS, NULL };
  int fd;
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

#ifdef EXPORT_NAME
  if (nbd_set_export_name (nbd, EXPORT_NAME) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    goto out;
  }
#endif

#if TLS
  if (nbd_supports_tls (nbd) != 1) {
    fprintf (stderr, "skip: compiled without TLS support\n");
    exit (77);
  }
  if (nbd_set_tls (nbd, TLS_MODE) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_set_tls_username (nbd, "alice") == -1) {
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

  /* Start the server. */
#if SOCKET_ACTIVATION
#define NBD_CONNECT nbd_connect_systemd_socket_activation
#else
#define NBD_CONNECT nbd_connect_command
#endif
  if (NBD_CONNECT (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    goto out;
  }

#if TLS
  if (TLS_MODE == LIBNBD_TLS_REQUIRE) {
    if (nbd_get_tls_negotiated (nbd) != 1) {
      fprintf (stderr,
               "%s: TLS required, but not negotiated on the connection\n",
               argv[0]);
      goto out;
    }
  }
  else if (TLS_MODE == LIBNBD_TLS_ALLOW) {
#if TLS_FALLBACK
    if (nbd_get_tls_negotiated (nbd) != 0) {
      fprintf (stderr,
               "%s: TLS disabled, but connection didn't fall back to plaintext\n",
               argv[0]);
      goto out;
    }
#else // !TLS_FALLBACK
    if (nbd_get_tls_negotiated (nbd) != 1) {
      fprintf (stderr,
               "%s: TLS allowed, but not negotiated on the connection\n",
               argv[0]);
      goto out;
    }
#endif
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

#if !TLS
  /* XXX qemu doesn't shut down the connection nicely (using
   * gnutls_bye) and because of this the following call will fail
   * with:
   *
   * nbd_shutdown: gnutls_record_recv: The TLS connection was
   * non-properly terminated.
   */
  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    goto out;
  }
#endif

  nbd_close (nbd);

  r = 0;
 out:
  unlink (tmpfile);

  exit (r == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
