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

/* Test interoperability with oldstyle servers. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include <libnbd.h>

#define SIZE 65536
#define XSTR(s) #s
#define STR(s) XSTR(s)

static char wbuf[512] = { 1, 2, 3, 4 }, rbuf[512];
static const char *progname;

static int
pread_cb (void *data,
          const void *buf, size_t count, uint64_t offset,
          unsigned status, int *error)
{
  int *calls;

  calls = data;
  ++*calls;

  if (buf != rbuf || count != sizeof rbuf) {
    fprintf (stderr, "%s: callback called with wrong buffer\n", progname);
    exit (EXIT_FAILURE);
  }
  if (offset != 2 * sizeof rbuf) {
    fprintf (stderr, "%s: callback called with wrong offset\n", progname);
    exit (EXIT_FAILURE);
  }
  if (*error != 0) {
    fprintf (stderr, "%s: callback called with unexpected error\n", progname);
    exit (EXIT_FAILURE);
  }
  if (status != LIBNBD_READ_DATA) {
    fprintf (stderr, "%s: callback called with wrong status\n", progname);
    exit (EXIT_FAILURE);
  }

  if (memcmp (rbuf, wbuf, sizeof rbuf) != 0) {
    fprintf (stderr, "%s: DATA INTEGRITY ERROR!\n", progname);
    exit (EXIT_FAILURE);
  }

  if (*calls > 1) {
    *error = ECONNREFUSED; /* Something NBD servers can't send */
    return -1;
  }

  return 0;
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int64_t r;
  char *args[] = { "nbdkit", "-s", "-o", "--exit-with-parent", "-v",
                   "memory", "size=" STR(SIZE), NULL };
  int calls = 0;
  const char *s;

  progname = argv[0];

  /* Initial sanity check that we can't require TLS */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_supports_tls (nbd)) {
    if (nbd_set_tls (nbd, LIBNBD_TLS_REQUIRE) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    if (nbd_connect_command (nbd, args) != -1) {
      fprintf (stderr, "%s\n", "expected failure");
      exit (EXIT_FAILURE);
    }
  }
  nbd_close (nbd);

  /* Now for a working connection */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Protocol should be "oldstyle", with no structured replies. */
  s = nbd_get_protocol (nbd);
  if (strcmp (s, "oldstyle") != 0) {
    fprintf (stderr,
             "incorrect protocol \"%s\", expected \"oldstyle\"\n", s);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_get_structured_replies_negotiated (nbd)) != 0) {
    fprintf (stderr,
             "incorrect structured replies %" PRId64 ", expected 0\n", r);
    exit (EXIT_FAILURE);
  }

  if ((r = nbd_get_size (nbd)) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (r != SIZE) {
    fprintf (stderr, "%s: test failed: incorrect size, "
             "actual %" PRIi64 ", expected %d",
             argv[0], r, SIZE);
    exit (EXIT_FAILURE);
  }

  /* Plain I/O */
  if (nbd_pwrite (nbd, wbuf, sizeof wbuf, 2 * sizeof wbuf, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_pread (nbd, rbuf, sizeof rbuf, 2 * sizeof rbuf, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (memcmp (rbuf, wbuf, sizeof rbuf) != 0) {
    fprintf (stderr, "%s: DATA INTEGRITY ERROR!\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  /* Test again for callback operation. */
  memset (rbuf, 0, sizeof rbuf);
  if (nbd_pread_structured (nbd, rbuf, sizeof rbuf, 2 * sizeof rbuf,
                            (nbd_chunk_callback) { .callback = pread_cb, .user_data = &calls },
                            0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (calls != 1) {
    fprintf (stderr, "%s: callback called wrong number of times\n", argv[0]);
    exit (EXIT_FAILURE);
  }
  if (memcmp (rbuf, wbuf, sizeof rbuf) != 0) {
    fprintf (stderr, "%s: DATA INTEGRITY ERROR!\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  /* Also test that callback errors are reflected correctly. */
  if (nbd_pread_structured (nbd, rbuf, sizeof rbuf, 2 * sizeof rbuf,
                            (nbd_chunk_callback) { .callback = pread_cb, .user_data = &calls },
                            0) != -1) {
    fprintf (stderr, "%s: expected failure from callback\n", argv[0]);
    exit (EXIT_FAILURE);
  }
  if (nbd_get_errno () != ECONNREFUSED) {
    fprintf (stderr, "%s: wrong errno value after failed callback\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
