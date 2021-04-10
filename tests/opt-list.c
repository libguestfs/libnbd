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

/* Test behavior of nbd_opt_list. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include <libnbd.h>

#include "requires.h"

struct progress {
  int id;
  int visit;
};

static int
check (void *user_data, const char *name, const char *description)
{
  struct progress *p = user_data;

  if (*description) {
    fprintf (stderr, "unexpected description for id %d visit %d: %s\n",
             p->id, p->visit, description);
    exit (EXIT_FAILURE);
  }

  switch (p->id) {
  case 0:
    fprintf (stderr, "callback shouldn't be reached when server has error\n");
    exit (EXIT_FAILURE);
  case 1:
    switch (p->visit) {
    case 0:
      if (strcmp (name, "a") != 0) {
        fprintf (stderr, "unexpected name '%s', expecting 'a'\n", name);
        exit (EXIT_FAILURE);
      }
      break;
    case 1:
      if (strcmp (name, "b") != 0) {
        fprintf (stderr, "unexpected name '%s', expecting 'b'\n", name);
        exit (EXIT_FAILURE);
      }
      break;
    default:
      fprintf (stderr, "callback reached too many times\n");
      exit (EXIT_FAILURE);
    }
    break;
  case 2:
    fprintf (stderr, "callback shouldn't be reached when list is empty\n");
    exit (EXIT_FAILURE);
  case 3:
    if (p->visit != 0) {
      fprintf (stderr, "callback reached too many times\n");
      exit (EXIT_FAILURE);
    }
    if (strcmp (name, "a") != 0) {
      fprintf (stderr, "unexpected name '%s', expecting 'a'\n", name);
      exit (EXIT_FAILURE);
    }
    break;
  default:
    fprintf (stderr, "callback reached with unexpected id %d\n", p->id);
    exit (EXIT_FAILURE);
  }

  p->visit++;
  return 0;
}

static struct nbd_handle*
prepare (int i)
{
  char mode[] = "mode=X";
  char *args[] = { "nbdkit", "-s", "--exit-with-parent", "-v",
                   "sh", SCRIPT, mode, NULL };
  struct nbd_handle *nbd;

  /* Get into negotiating state. */
  mode[5] = '0' + i;
  nbd = nbd_create ();
  if (nbd == NULL ||
      nbd_set_opt_mode (nbd, true) == -1 ||
      nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  return nbd;
}

static void
cleanup (struct nbd_handle *nbd)
{
  nbd_opt_abort (nbd);
  nbd_close (nbd);
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int64_t r;
  struct progress p;

  /* Quick check that nbdkit is new enough */
  requires ("nbdkit sh --dump-plugin | grep -q has_list_exports=1");

  /* First pass: server fails NBD_OPT_LIST. */
  nbd = prepare (0);
  p = (struct progress) { .id = 0 };
  r = nbd_opt_list (nbd, (nbd_list_callback) { .callback = check,
                                               .user_data = &p});
  if (r != -1) {
    fprintf (stderr, "expected error after opt_list\n");
    exit (EXIT_FAILURE);
  }
  if (p.visit != 0) {
    fprintf (stderr, "callback called unexpectedly\n");
    exit (EXIT_FAILURE);
  }
  cleanup (nbd);

  /* Second pass: server advertises 'a' and 'b'. */
  nbd = prepare (1);
  p = (struct progress) { .id = 1 };
  r = nbd_opt_list (nbd, (nbd_list_callback) { .callback = check,
                                               .user_data = &p});
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  else if (r != 2 || p.visit != r) {
    fprintf (stderr, "wrong number of exports, got %" PRId64 " expecting 2\n",
             r);
    exit (EXIT_FAILURE);
  }
  cleanup (nbd);

  /* Third pass: server advertises empty list. */
  nbd = prepare (2);
  p = (struct progress) { .id = 2 };
  r = nbd_opt_list (nbd, (nbd_list_callback) { .callback = check,
                                               .user_data = &p});
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  else if (r != 0 || p.visit != r) {
    fprintf (stderr, "wrong number of exports, got %" PRId64 " expecting 0\n",
             r);
    exit (EXIT_FAILURE);
  }
  cleanup (nbd);

  /* Final pass: server advertises 'a'. */
  nbd = prepare (3);
  p = (struct progress) { .id = 3 };
  r = nbd_opt_list (nbd, (nbd_list_callback) { .callback = check,
                                               .user_data = &p});
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  else if (r != 1 || p.visit != r) {
    fprintf (stderr, "wrong number of exports, got %" PRId64 " expecting 1\n",
             r);
    exit (EXIT_FAILURE);
  }
  cleanup (nbd);

  exit (EXIT_SUCCESS);
}
