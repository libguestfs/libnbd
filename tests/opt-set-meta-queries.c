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

/* Test behavior of nbd_opt_set_meta_context_queries. */
/* See also unit test 255 in the various language ports. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include <libnbd.h>

struct progress {
  int count;
  bool seen;
};

static int
check (void *user_data, const char *name)
{
  struct progress *p = user_data;

  p->count++;
  if (strcmp (name, LIBNBD_CONTEXT_BASE_ALLOCATION) == 0)
    p->seen = true;
  return 0;
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int r;
  struct progress p;
  char *args[] = { "nbdkit", "-s", "--exit-with-parent", "-v",
                   "memory", "size=1M", NULL };
  nbd_context_callback ctx = { .callback = check,
                               .user_data = &p};

  /* Get into negotiating state. */
  nbd = nbd_create ();
  if (nbd == NULL ||
      nbd_set_opt_mode (nbd, true) == -1 ||
      nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* nbdkit does not match wildcard for SET, even though it does for LIST */
  p = (struct progress) { .count = 0 };
  {
    char *base[] = { "base:", NULL };
    r = nbd_opt_set_meta_context_queries (nbd, base, ctx);
  }
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != p.count || r != 0 || p.seen) {
    fprintf (stderr, "inconsistent return value %d, expected %d\n", r, p.count);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION)) != 0) {
    fprintf (stderr, "can_meta_context got %d, expected 0\n", r);
    exit (EXIT_FAILURE);
  }

  /* Negotiating with no contexts is not an error, but selects nothing.
   * An explicit empty list overrides a non-empty implicit list.
   */
  if (nbd_add_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  p = (struct progress) { .count = 0 };
  {
    char *empty[] = { NULL };
    r = nbd_opt_set_meta_context_queries (nbd, empty, ctx);
  }
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 0 || p.count || p.seen) {
    fprintf (stderr, "expecting set_meta to select nothing\n");
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION)) != 0) {
    fprintf (stderr, "can_meta_context got %d, expected 0\n", r);
    exit (EXIT_FAILURE);
  }

  /* Request 2 with expectation of 1. */
  p = (struct progress) { .count = 0 };
  {
    char *pair[] = { "x-nosuch:context", LIBNBD_CONTEXT_BASE_ALLOCATION, NULL };
    r = nbd_opt_set_meta_context_queries (nbd, pair, ctx);
  }
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 1 || p.count != 1 || !p.seen) {
    fprintf (stderr, "expecting one context, got %d\n", r);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION)) != 1) {
    fprintf (stderr, "can_meta_context got %d, expected 1\n", r);
    exit (EXIT_FAILURE);
  }

  /* Transition to transmission phase with with set_request_meta_context off,
   * our last set should remain active
   */
  if (nbd_set_request_meta_context (nbd, 0) == -1 ||
      nbd_opt_go (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION)) != 1) {
    fprintf (stderr, "can_meta_context got %d, expected 1\n", r);
    exit (EXIT_FAILURE);
  }

  nbd_shutdown (nbd, 0);
  nbd_close (nbd);

  exit (EXIT_SUCCESS);
}
