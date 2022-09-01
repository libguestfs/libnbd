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

/* Test behavior of nbd_opt_info. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include <libnbd.h>

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int64_t r;
  char *s;
  char *args[] = { "nbdkit", "-s", "--exit-with-parent", "-v",
                   "sh", SCRIPT, NULL };

  /* Get into negotiating state. */
  nbd = nbd_create ();
  if (nbd == NULL ||
      nbd_set_opt_mode (nbd, true) == -1 ||
      nbd_connect_command (nbd, args) == -1 ||
      nbd_add_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* No size, flags, or meta-contexts yet */
  if (nbd_get_size (nbd) != -1) {
    fprintf (stderr, "expecting error for get_size\n");
    exit (EXIT_FAILURE);
  }
  if (nbd_is_read_only (nbd) != -1) {
    fprintf (stderr, "expecting error for is_read_only\n");
    exit (EXIT_FAILURE);
  }
  if (nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) != -1) {
    fprintf (stderr, "expecting error for can_meta_context\n");
    exit (EXIT_FAILURE);
  }

  /* info with no prior name gets info on "" */
  if (nbd_opt_info (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_get_size (nbd)) != 0) {
    fprintf (stderr, "expecting size of 0, got %" PRId64 "\n", r);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_is_read_only (nbd)) != 1) {
    fprintf (stderr, "expecting read-only export, got %" PRId64 "\n", r);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION)) != 1) {
    fprintf (stderr, "expecting can_meta_context true, got %" PRId64 "\n", r);
    exit (EXIT_FAILURE);
  }

  /* info on something not present fails, wipes out prior info */
  if (nbd_set_export_name (nbd, "a") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_opt_info (nbd) != -1) {
    fprintf (stderr, "expecting error for opt_info\n");
    exit (EXIT_FAILURE);
  }
  if (nbd_get_size (nbd) != -1) {
    fprintf (stderr, "expecting error for get_size\n");
    exit (EXIT_FAILURE);
  }
  if (nbd_is_read_only (nbd) != -1) {
    fprintf (stderr, "expecting error for is_read_only\n");
    exit (EXIT_FAILURE);
  }
  if (nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) != -1) {
    fprintf (stderr, "expecting error for can_meta_context\n");
    exit (EXIT_FAILURE);
  }

  /* info for a different export, with automatic meta_context disabled */
  if (nbd_set_export_name (nbd, "b") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_set_request_meta_context (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_opt_info (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_get_size (nbd)) != 1) {
    fprintf (stderr, "expecting size of 1, got %" PRId64 "\n", r);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_is_read_only (nbd)) != 0) {
    fprintf (stderr, "expecting read-write export, got %" PRId64 "\n", r);
    exit (EXIT_FAILURE);
  }
  if (nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) != -1) {
    fprintf (stderr, "expecting error for can_meta_context\n");
    exit (EXIT_FAILURE);
  }
  if (nbd_set_request_meta_context (nbd, 1) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* go on something not present */
  if (nbd_set_export_name (nbd, "a") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_opt_go (nbd) != -1) {
    fprintf (stderr, "expecting error for opt_go\n");
    exit (EXIT_FAILURE);
  }
  if (nbd_get_size (nbd) != -1) {
    fprintf (stderr, "expecting error for get_size\n");
    exit (EXIT_FAILURE);
  }
  if (nbd_is_read_only (nbd) != -1) {
    fprintf (stderr, "expecting error for is_read_only\n");
    exit (EXIT_FAILURE);
  }
  if (nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) != -1) {
    fprintf (stderr, "expecting error for can_meta_context\n");
    exit (EXIT_FAILURE);
  }

  /* go on a valid export */
  if (nbd_set_export_name (nbd, "good") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_opt_go (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_get_size (nbd)) != 4) {
    fprintf (stderr, "expecting size of 4, got %" PRId64 "\n", r);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_is_read_only (nbd)) != 1) {
    fprintf (stderr, "expecting read-only export, got %" PRId64 "\n", r);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION)) != 1) {
    fprintf (stderr, "expecting can_meta_context true, got %" PRId64 "\n", r);
    exit (EXIT_FAILURE);
  }

  /* now info is no longer valid, but does not wipe data */
  if (nbd_set_export_name (nbd, "a") != -1) {
    fprintf (stderr, "expecting error for set_export_name\n");
    exit (EXIT_FAILURE);
  }
  if ((s = nbd_get_export_name (nbd)) == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (strcmp (s, "good") != 0) {
    fprintf (stderr, "expecting export name 'good', got '%s'\n", s);
    exit (EXIT_FAILURE);
  }
  if (nbd_opt_info (nbd) != -1) {
    fprintf (stderr, "expecting error for opt_info\n");
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_get_size (nbd)) != 4) {
    fprintf (stderr, "expecting size of 4, got %" PRId64 "\n", r);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION)) != 1) {
    fprintf (stderr, "expecting can_meta_context true, got %" PRId64 "\n", r);
    exit (EXIT_FAILURE);
  }

  nbd_shutdown (nbd, 0);
  nbd_close (nbd);

  /* Another connection. This time, check that SET_META triggered by opt_info
   * persists through nbd_opt_go with set_request_meta_context disabled.
   */
  nbd = nbd_create ();
  if (nbd == NULL ||
      nbd_set_opt_mode (nbd, true) == -1 ||
      nbd_connect_command (nbd, args) == -1 ||
      nbd_add_meta_context (nbd, "x-unexpected:bogus") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) != -1) {
    fprintf (stderr, "expecting error for can_meta_context\n");
    exit (EXIT_FAILURE);
  }
  if (nbd_opt_info (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION)) != 0) {
    fprintf (stderr, "expecting can_meta_context false, got %" PRId64 "\n", r);

    exit (EXIT_FAILURE);
  }
  if (nbd_set_request_meta_context (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  /* Adding to the request list now won't matter */
  if (nbd_add_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) != 0) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_opt_go (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION)) != 0) {
    fprintf (stderr, "expecting can_meta_context false, got %" PRId64 "\n", r);

    exit (EXIT_FAILURE);
  }

  nbd_shutdown (nbd, 0);
  nbd_close (nbd);

  exit (EXIT_SUCCESS);
}
