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

/* Test behavior of nbd_opt_list_meta_context_queries. */
/* See also unit test 245 in the various language ports. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

/* GCC will warn that we are passing NULL (or worse), so to do this
 * test we must remove the header file attribute.
 */
#define LIBNBD_ATTRIBUTE_NONNULL(...)
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

  /* C-only test: We document that a NULL list is undefined behavior, but
   * that we try to make it fail with EFAULT.  By disabling attributes
   * above, we are able to check that the generated EFAULT code works.
   */
  p = (struct progress) { .count = 0 };
  r = nbd_opt_list_meta_context_queries (nbd, NULL, ctx);
  if (r != -1 || nbd_get_errno () != EFAULT) {
    fprintf (stderr, "expected EFAULT for NULL query list\n");
    exit (EXIT_FAILURE);
  }
  if (p.count != 0 || p.seen) {
    fprintf (stderr, "unexpected use of callback on failure\n");
    exit (EXIT_FAILURE);
  }

  /* First pass: empty query should give at least "base:allocation".
   * The explicit query overrides a non-empty nbd_add_meta_context.
   */
  p = (struct progress) { .count = 0 };
  if (nbd_add_meta_context (nbd, "x-nosuch:") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  {
    char *empty[] = { NULL };
    r = nbd_opt_list_meta_context_queries (nbd, empty, ctx);
  }
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != p.count) {
    fprintf (stderr, "inconsistent return value %d, expected %d\n", r, p.count);
    exit (EXIT_FAILURE);
  }
  if (r < 1 || !p.seen) {
    fprintf (stderr, "server did not reply with base:allocation\n");
    exit (EXIT_FAILURE);
  }

  /* Second pass: bogus query has no response. */
  p = (struct progress) { .count = 0 };
  r = nbd_clear_meta_contexts (nbd);
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  {
    char *nosuch[] = { "x-nosuch:", NULL };
    r = nbd_opt_list_meta_context_queries (nbd, nosuch, ctx);
  }
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 0 || p.count != 0 || p.seen) {
    fprintf (stderr, "expecting no contexts, got %d\n", r);
    exit (EXIT_FAILURE);
  }

  /* Third pass: specific query should have one match. */
  p = (struct progress) { .count = 0 };
  {
    char *pair[] = { "x-nosuch:", LIBNBD_CONTEXT_BASE_ALLOCATION, NULL };
    r = nbd_opt_list_meta_context_queries (nbd, pair, ctx);
  }
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 1 || p.count != 1 || !p.seen) {
    fprintf (stderr, "expecting exactly one context, got %d\n", r);
    exit (EXIT_FAILURE);
  }

  nbd_opt_abort (nbd);
  nbd_close (nbd);

  exit (EXIT_SUCCESS);
}
