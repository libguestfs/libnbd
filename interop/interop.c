/* NBD client library in userspace
 * Copyright (C) 2013-2021 Red Hat Inc.
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

#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#endif

#include <libnbd.h>

#include "../tests/requires.h"

#define SIZE (1024*1024)

#if CERTS || PSK
#define TLS 1
#ifndef TLS_MODE
#error "TLS_MODE must be defined when using CERTS || PSK"
#endif
#endif

#ifdef NEEDS_TMPFILE
#define TMPFILE tmp
static char tmp[] = "/tmp/nbdXXXXXX";

static void
unlink_tmpfile (void)
{
  unlink (TMPFILE);
}
#endif /* NEEDS_TMPFILE */

#ifdef HAVE_GNUTLS
#if TLS
static void
tls_log (int level, const char *msg)
{
  /* Messages from GnuTLS are always \n-terminated. */
  fprintf (stderr, "gnutls[%d]: %s", level, msg);
}
#endif
#endif

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  char *args[] = { SERVER, SERVER_PARAMS, NULL };
  int64_t actual_size;
  char buf[512];
  size_t i;

  /* Check requirements or skip the test. */
#ifdef REQUIRES
  REQUIRES
#endif

  /* Ignore SIGPIPE.  We only need this for GnuTLS that lacks the
   * GNUTLS_NO_SIGNAL flag, either because it predates GnuTLS 3.4.2 or
   * because the OS lacks MSG_NOSIGNAL support.
   */
#if TLS && !defined(HAVE_GNUTLS_NO_SIGNAL)
  signal (SIGPIPE, SIG_IGN);
#endif

  /* Create a large sparse temporary file. */
#ifdef NEEDS_TMPFILE
  int fd = mkstemp (TMPFILE);
  if (fd == -1 ||
      ftruncate (fd, SIZE) == -1 ||
      close (fd) == -1) {
    perror (TMPFILE);
    exit (EXIT_FAILURE);
  }
  atexit (unlink_tmpfile);
#endif

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

#ifdef EXPORT_NAME
  if (nbd_set_export_name (nbd, EXPORT_NAME) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
#endif

#if TLS
  if (nbd_supports_tls (nbd) != 1) {
    fprintf (stderr, "skip: compiled without TLS support\n");
    exit (77);
  }

#ifdef HAVE_GNUTLS
  /* This is kind of ugly but GnuTLS only allows us to set these
   * globally (so they are not appropriate for libnbd).
   *
   * Also by default GnuTLS throws away log messages even if you
   * called gnutls_global_set_log_level.  It doesn't install the
   * default log handler unless you set $GNUTLS_DEBUG_LEVEL.  So we
   * need to have our own log handler.
   *
   * Also the log levels are quite random.  Level 2 doesn't show the
   * negotiated cyphersuite, but level 3+ shows excessive detail.
   */
  gnutls_global_set_log_level (2);
  gnutls_global_set_log_function (tls_log);
#endif

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

  /* Print the server parameters. */
  fprintf (stderr, "server: %s", args[0]);
  for (i = 1; args[i] != NULL; ++i)
    fprintf (stderr, " %s", args[i]);
  fprintf (stderr, "\n");

  /* Start the server. */
#if SOCKET_ACTIVATION
#define NBD_CONNECT nbd_connect_systemd_socket_activation
#else
#define NBD_CONNECT nbd_connect_command
#endif
  if (NBD_CONNECT (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

#if TLS
  if (TLS_MODE == LIBNBD_TLS_REQUIRE) {
    if (nbd_get_tls_negotiated (nbd) != 1) {
      fprintf (stderr,
               "%s: TLS required, but not negotiated on the connection\n",
               argv[0]);
      exit (EXIT_FAILURE);
    }
  }
  else if (TLS_MODE == LIBNBD_TLS_ALLOW) {
#if TLS_FALLBACK
    if (nbd_get_tls_negotiated (nbd) != 0) {
      fprintf (stderr,
               "%s: TLS disabled, but connection didn't fall back to plaintext\n",
               argv[0]);
      exit (EXIT_FAILURE);
    }
#else // !TLS_FALLBACK
    if (nbd_get_tls_negotiated (nbd) != 1) {
      fprintf (stderr,
               "%s: TLS allowed, but not negotiated on the connection\n",
               argv[0]);
      exit (EXIT_FAILURE);
    }
#endif
  }
#endif

  actual_size = nbd_get_size (nbd);
  if (actual_size == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (actual_size != SIZE) {
    fprintf (stderr, "%s: actual size %" PRIi64 " <> expected size %d",
             argv[0], actual_size, SIZE);
    exit (EXIT_FAILURE);
  }

  if (nbd_pread (nbd, buf, sizeof buf, 0, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* XXX In future test more operations here. */

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);

  exit (EXIT_SUCCESS);
}
