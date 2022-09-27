/* NBD client library in userspace
 * Copyright (C) 2013-2022 Red Hat Inc.
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

/* Demonstrate low-level use of nbd_opt_structured_reply(). */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include <libnbd.h>


static int
check_extent (void *data, const char *metacontext, uint64_t offset,
              uint32_t *entries, size_t nr_entries, int *error)
{
  /* If we got here, structured replies were negotiated. */
  bool *seen = data;

  *seen = true;
  return 0;
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  const char *cmd[] = {
    "nbdkit", "-s", "-v", "--exit-with-parent", "memory", "1048576", NULL
  };
  int r;
  bool extents_worked = false;

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Connect to the server in opt mode, without structured replies. */
  if (nbd_set_opt_mode (nbd, true) == -1 ||
      nbd_set_request_structured_replies (nbd, false) == -1 ||
      nbd_connect_command (nbd, (char **) cmd) == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  r = nbd_get_structured_replies_negotiated (nbd);
  if (r == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 0) {
    fprintf (stderr, "%s: not expecting structured replies yet\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  /* First request should succeed. */
  r = nbd_opt_structured_reply (nbd);
  if (r == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 1) {
    fprintf (stderr, "%s: expecting structured replies\n", argv[0]);
    exit (EXIT_FAILURE);
  }
  r = nbd_get_structured_replies_negotiated (nbd);
  if (r == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 1) {
    fprintf (stderr, "%s: expecting structured replies\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  /* nbdkit 1.32 allows a second request, nbdkit 1.34 diagnoses it. */
  r = nbd_opt_structured_reply (nbd);
  if (r == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  printf ("%s: server's response to second request: %s\n", argv[0],
          r ? "accepted" : "rejected");

  /* Regardless of whether second request passed, structured replies were
   * negotiated, so we should be able to do block status.
   */
  r = nbd_get_structured_replies_negotiated (nbd);
  if (r == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 1) {
    fprintf (stderr, "%s: expecting structured replies\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  if (nbd_add_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) == -1 ||
      nbd_opt_go (nbd) == -1 ||
      (r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION)) == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 1) {
    fprintf (stderr, "%s: expecting base:allocation support\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  if (nbd_block_status (nbd, 65536, 0,
                        (nbd_extent_callback) { .callback = check_extent,
                                                .user_data = &extents_worked },
                        0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (!extents_worked) {
    fprintf (stderr, "%s: expecting block_status success\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
