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

/* Test behavior of nbd_opt_list_meta_context. */
/* See also unit test 240 in the various language ports. */

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
  int max;
  char *tmp;
  uint64_t bytes;

  /* Get into negotiating state. */
  nbd = nbd_create ();
  if (nbd == NULL ||
      nbd_set_opt_mode (nbd, true) == -1 ||
      nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* First pass: empty query should give at least "base:allocation". */
  p = (struct progress) { .count = 0 };
  r = nbd_opt_list_meta_context (nbd,
                                 (nbd_context_callback) { .callback = check,
                                                          .user_data = &p});
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
  max = p.count;

  /* Second pass: bogus query has no response. */
  p = (struct progress) { .count = 0 };
  r = nbd_add_meta_context (nbd, "x-nosuch:");
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  r = nbd_opt_list_meta_context (nbd,
                                 (nbd_context_callback) { .callback = check,
                                                          .user_data = &p});
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
  r = nbd_add_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION);
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_get_nr_meta_contexts (nbd) != 2) {
    fprintf (stderr, "expecting 2 meta requests\n");
    exit (EXIT_FAILURE);
  }
  tmp = nbd_get_meta_context (nbd, 1);
  if (!tmp) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (strcmp (tmp, LIBNBD_CONTEXT_BASE_ALLOCATION) != 0) {
    fprintf (stderr, "expecting base:allocation, got %s\n", tmp);
    exit (EXIT_FAILURE);
  }
  free (tmp);
  r = nbd_opt_list_meta_context (nbd,
                                 (nbd_context_callback) { .callback = check,
                                                          .user_data = &p});
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 1 || p.count != 1 || !p.seen) {
    fprintf (stderr, "expecting exactly one context, got %d\n", r);
    exit (EXIT_FAILURE);
  }

  /* Fourth pass: nbd_opt_list_meta_context is stateless, so it should
   * not wipe status learned during nbd_opt_info().
   */
  r = nbd_get_size (nbd);
  if (r != -1) {
    fprintf (stderr, "expecting get_size to fail, got %d\n", r);
    exit (EXIT_FAILURE);
  }
  r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION);
  if (r != -1) {
    fprintf (stderr, "expecting can_meta_context to fail, got %d\n", r);
    exit (EXIT_FAILURE);
  }
  if (nbd_opt_info (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  r = nbd_get_size (nbd);
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 1024*1024) {
    fprintf (stderr, "expecting get_size of 1M, got %d\n", r);
    exit (EXIT_FAILURE);
  }
  r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION);
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 1) {
    fprintf (stderr, "expecting can_meta_context to succeed, got %d\n", r);
    exit (EXIT_FAILURE);
  }
  if (nbd_clear_meta_contexts (nbd) == -1 ||
      nbd_add_meta_context (nbd, "x-nosuch:") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  p = (struct progress) { .count = 0 };
  r = nbd_opt_list_meta_context (nbd,
                                 (nbd_context_callback) { .callback = check,
                                                          .user_data = &p});
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 0 || p.count != 0 || p.seen) {
    fprintf (stderr, "expecting no contexts, got %d\n", r);
    exit (EXIT_FAILURE);
  }
  r = nbd_get_size (nbd);
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 1048576) {
    fprintf (stderr, "expecting get_size of 1M, got %d\n", r);
    exit (EXIT_FAILURE);
  }
  r = nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION);
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 1) {
    fprintf (stderr, "expecting can_meta_context to succeed, got %d\n", r);
    exit (EXIT_FAILURE);
  }

  /* Final pass: "base:" query should get at least "base:allocation" */
  p = (struct progress) { .count = 0 };
  r = nbd_add_meta_context (nbd, "base:");
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  r = nbd_opt_list_meta_context (nbd,
                                 (nbd_context_callback) { .callback = check,
                                                          .user_data = &p});
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r < 1 || r > max || r != p.count || !p.seen) {
    fprintf (stderr, "expecting at least one context, got %d\n", r);
    exit (EXIT_FAILURE);
  }

  nbd_opt_abort (nbd);
  nbd_close (nbd);

  /* Repeat but this time without structured replies. Deal gracefully
   * with older servers that don't allow the attempt.
   */
  nbd = nbd_create ();
  if (nbd == NULL ||
      nbd_set_opt_mode (nbd, true) == -1 ||
      nbd_set_request_structured_replies (nbd, false) == -1 ||
      nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  bytes = nbd_stats_bytes_sent (nbd);

  p = (struct progress) { .count = 0 };
  r = nbd_opt_list_meta_context (nbd,
                                 (nbd_context_callback) { .callback = check,
                                                          .user_data = &p});
  if (r == -1) {
    if (nbd_stats_bytes_sent (nbd) == bytes) {
      fprintf (stderr, "bug: client failed to send request\n");
      exit (EXIT_FAILURE);
    }
    fprintf (stdout, "ignoring failure from old server: %s\n",
             nbd_get_error ());
  }
  else {
    if (r != p.count) {
      fprintf (stderr, "inconsistent return value %d, expected %d\n",
               r, p.count);
      exit (EXIT_FAILURE);
    }
    if (r < 1 || !p.seen) {
      fprintf (stderr, "server did not reply with base:allocation\n");
      exit (EXIT_FAILURE);
    }
  }

  /* Now enable structured replies, and a retry should pass. */
  r = nbd_opt_structured_reply (nbd);
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != 1) {
    fprintf (stderr, "expecting structured replies to be supported\n");
    exit (EXIT_FAILURE);
  }

  p = (struct progress) { .count = 0 };
  r = nbd_opt_list_meta_context (nbd,
                                 (nbd_context_callback) { .callback = check,
                                                          .user_data = &p});
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (r != p.count) {
    fprintf (stderr, "inconsistent return value %d, expected %d\n",
             r, p.count);
    exit (EXIT_FAILURE);
  }
  if (r < 1 || !p.seen) {
    fprintf (stderr, "server did not reply with base:allocation\n");
    exit (EXIT_FAILURE);
  }

  nbd_opt_abort (nbd);
  nbd_close (nbd);

  exit (EXIT_SUCCESS);
}
