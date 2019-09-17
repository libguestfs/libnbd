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

/* Test metadata context "base:allocation". */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>

#include <libnbd.h>

static int check_extent (void *data,
                         const char *metacontext,
                         uint64_t offset,
                         uint32_t *entries, size_t nr_entries, int *error);

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  char plugin_path[256];
  int id;
  int r;
  const char *s;

  snprintf (plugin_path, sizeof plugin_path, "%s/meta-base-allocation.sh",
            getenv ("srcdir") ? : ".");

  char *args[] =
    { "nbdkit", "-s", "--exit-with-parent", "-v",
      "sh", plugin_path,
      NULL };

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Negotiate metadata context "base:allocation" with the server.
   * This is supported in nbdkit >= 1.12.
   */
  if (nbd_add_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Also request negotiation of a bogus context, which should not
   * fail here nor affect block status later.
   */
  if (nbd_add_meta_context (nbd, "x-libnbd:nosuch") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Protocol should be "newstyle-fixed", with structured replies. */
  s = nbd_get_protocol (nbd);
  if (strcmp (s, "newstyle-fixed") != 0) {
    fprintf (stderr,
             "incorrect protocol \"%s\", expected \"newstyle-fixed\"\n", s);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_get_structured_replies_negotiated (nbd)) != 1) {
    fprintf (stderr,
             "incorrect structured replies %d, expected 1\n", r);
    exit (EXIT_FAILURE);
  }

  switch (nbd_can_meta_context (nbd, "x-libnbd:nosuch")) {
  case -1:
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  case 0:
    break;
  default:
    fprintf (stderr, "unexpected status for nbd_can_meta_context\n");
    exit (EXIT_FAILURE);
  }

  switch (nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION)) {
  case -1:
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  case 1:
    break;
  default:
    fprintf (stderr, "unexpected status for nbd_can_meta_context\n");
    exit (EXIT_FAILURE);
  }

  /* Read the block status. */
  id = 1;
  if (nbd_block_status (nbd, 65536, 0,
                        (nbd_extent_callback) { .callback = check_extent, .user_data = &id },
                        0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  id = 2;
  if (nbd_block_status (nbd, 1024, 32768-512,
                        (nbd_extent_callback) { .callback = check_extent, .user_data = &id },
                        0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  id = 3;
  if (nbd_block_status (nbd, 1024, 32768-512,
                        (nbd_extent_callback) { .callback = check_extent, .user_data = &id },
                        LIBNBD_CMD_FLAG_REQ_ONE) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}

static int
check_extent (void *data,
              const char *metacontext,
              uint64_t offset,
              uint32_t *entries, size_t nr_entries, int *error)
{
  size_t i;
  int id;

  id = * (int *)data;

  printf ("extent: id=%d, metacontext=%s, offset=%" PRIu64 ", "
          "nr_entries=%zu, error=%d\n",
          id, metacontext, offset, nr_entries, *error);

  assert (*error == 0);
  if (strcmp (metacontext, LIBNBD_CONTEXT_BASE_ALLOCATION) == 0) {
    for (i = 0; i < nr_entries; i += 2) {
      printf ("\t%zu\tlength=%" PRIu32 ", status=%" PRIu32 "\n",
              i, entries[i], entries[i+1]);
    }
    fflush (stdout);

    switch (id) {
    case 1:
      assert (nr_entries == 10);
      assert (entries[0] == 8192);  assert (entries[1] == 0);
      assert (entries[2] == 8192);  assert (entries[3] == LIBNBD_STATE_HOLE);
      assert (entries[4] == 16384); assert (entries[5] == (LIBNBD_STATE_HOLE|
                                                           LIBNBD_STATE_ZERO));
      assert (entries[6] == 16384); assert (entries[7] == LIBNBD_STATE_ZERO);
      assert (entries[8] == 16384); assert (entries[9] == 0);
      break;

    case 2:
      assert (nr_entries == 4);
      assert (entries[0] == 512);   assert (entries[1] == (LIBNBD_STATE_HOLE|
                                                           LIBNBD_STATE_ZERO));
      assert (entries[2] == 16384); assert (entries[3] == LIBNBD_STATE_ZERO);
      break;

    case 3:
      assert (nr_entries == 2);
      assert (entries[0] == 512);   assert (entries[1] == (LIBNBD_STATE_HOLE|
                                                           LIBNBD_STATE_ZERO));
      break;

    default:
      abort ();
    }

  }
  else
    fprintf (stderr, "warning: ignored unexpected meta context %s\n",
             metacontext);

  return 0;
}
