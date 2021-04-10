/* NBD client library in userspace
 * Copyright (C) 2013-2020 Red Hat Inc.
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

/* Test interoperability with newstyle (not newstyle-fixed) servers. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include <libnbd.h>

#include "requires.h"

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

static int
list_cb (void *opaque, const char *name, const char *description)
{
  /* This callback is unreachable; plain newstyle can't do OPT_LIST */
  fprintf (stderr, "%s: list callback mistakenly reached", progname);
  exit (EXIT_FAILURE);
}

static bool list_freed = false;
static void
free_list_cb (void *opaque)
{
  if (list_freed) {
    fprintf (stderr, "%s: list callback mistakenly freed twice", progname);
    exit (EXIT_FAILURE);
  }
  list_freed = true;
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int64_t r;
  char *args[] = { "nbdkit", "-s", "--mask-handshake", "0",
                   "--exit-with-parent", "-v", "eval",
                   "list_exports=printf a\\nb\\n",
                   "get_size=echo " STR(SIZE),
                   "open=if [ \"$3\" != a ]; then exit 1; fi\n"
                   " truncate --size=" STR(SIZE) " $tmpdir/a",
                   "pread=dd bs=1 skip=$4 count=$3 if=$tmpdir/a || exit 1",
                   "pwrite=dd bs=1 conv=notrunc seek=$4 of=$tmpdir/a || exit 1",
                   "can_write=exit 0",
                   NULL };
  int calls = 0;
  const char *s;

  progname = argv[0];

  /* Quick check that nbdkit is new enough */
  requires ("nbdkit eval --dump-plugin | grep -q has_list_exports=1");

  /* Initial sanity check that we can't require TLS */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  nbd_set_export_name (nbd, "a");
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

  /* Next try. Requesting opt_mode works, but opt_go is the only
   * option that can succeed (via NBD_OPT_EXPORT_NAME); opt_abort is
   * special-cased but moves to DEAD rather than CLOSED.
   */
  nbd = nbd_create ();
  if (nbd == NULL ||
      nbd_set_opt_mode (nbd, true) == -1 ||
      nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (!nbd_aio_is_negotiating (nbd)) {
    fprintf (stderr, "unexpected state after negotiating\n");
    exit (EXIT_FAILURE);
  }
  if (nbd_opt_list (nbd, (nbd_list_callback) { .callback = list_cb,
                                               .free = free_list_cb }) != -1) {
    fprintf (stderr, "nbd_opt_list: expected failure\n");
    exit (EXIT_FAILURE);
  }
  if (!list_freed) {
    fprintf (stderr, "nbd_opt_list: list closure memory leak\n");
    exit (EXIT_FAILURE);
  }
  if (nbd_get_errno () != ENOTSUP) {
    fprintf (stderr, "%s: wrong errno value after failed opt_list\n", argv[0]);
    exit (EXIT_FAILURE);
  }
  if (nbd_opt_info (nbd) != -1) {
    fprintf (stderr, "nbd_opt_info: expected failure\n");
    exit (EXIT_FAILURE);
  }
  if (nbd_get_errno () != ENOTSUP) {
    fprintf (stderr, "%s: wrong errno value after failed opt_info\n", argv[0]);
    exit (EXIT_FAILURE);
  }
  if (nbd_opt_abort (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (!nbd_aio_is_dead (nbd)) {
    fprintf (stderr, "unexpected state after opt_abort\n");
    exit (EXIT_FAILURE);
  }
  nbd_close (nbd);

  /* And another try: an incorrect export name kills the connection,
   * rather than allowing a second try.
   */
  nbd = nbd_create ();
  if (nbd == NULL ||
      nbd_set_opt_mode (nbd, true) == -1 ||
      nbd_connect_command (nbd, args) == -1 ||
      nbd_set_export_name (nbd, "b") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_opt_go (nbd) != -1) {
    fprintf (stderr, "%s\n", "expected failure");
    exit (EXIT_FAILURE);
  }
  if (!nbd_aio_is_dead (nbd)) {
    fprintf (stderr, "unexpected state after failed export\n");
    exit (EXIT_FAILURE);
  }
  nbd_close (nbd);

  /* Now for a working connection.  Protocol should be "newstyle",
   * with no structured replies and no meta contexts.
   */
  nbd = nbd_create ();
  if (nbd == NULL ||
      nbd_set_export_name (nbd, "a") == -1 ||
      nbd_add_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) == -1 ||
      nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_aio_is_ready (nbd) != true) {
    fprintf (stderr, "unexpected state after connection\n");
    exit (EXIT_FAILURE);
  }
  s = nbd_get_protocol (nbd);
  if (strcmp (s, "newstyle") != 0) {
    fprintf (stderr,
             "incorrect protocol \"%s\", expected \"newstyle\"\n", s);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_get_structured_replies_negotiated (nbd)) != 0) {
    fprintf (stderr,
             "incorrect structured replies %" PRId64 ", expected 0\n", r);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION)) != 0) {
    fprintf (stderr,
             "incorrect meta context %" PRId64 ", expected 0\n", r);
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
  nbd_close (nbd);

  /* Repeat a working connection, but with explicit nbd_opt_go. */
  nbd = nbd_create ();
  if (nbd == NULL ||
      nbd_set_export_name (nbd, "a") == -1 ||
      nbd_set_opt_mode (nbd, true) == -1 ||
      nbd_add_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) == -1 ||
      nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_aio_is_negotiating (nbd) != true) {
    fprintf (stderr, "unexpected state after connection\n");
    exit (EXIT_FAILURE);
  }
  if (nbd_opt_go (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_aio_is_ready (nbd) != true) {
    fprintf (stderr, "unexpected state after opt_go\n");
    exit (EXIT_FAILURE);
  }
  s = nbd_get_protocol (nbd);
  if (strcmp (s, "newstyle") != 0) {
    fprintf (stderr,
             "incorrect protocol \"%s\", expected \"newstyle\"\n", s);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_get_structured_replies_negotiated (nbd)) != 0) {
    fprintf (stderr,
             "incorrect structured replies %" PRId64 ", expected 0\n", r);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION)) != 0) {
    fprintf (stderr,
             "incorrect meta context %" PRId64 ", expected 0\n", r);
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
