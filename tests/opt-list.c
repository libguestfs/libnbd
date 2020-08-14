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

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int64_t r;
  char *name;
  char *args[] = { "nbdkit", "-s", "--exit-with-parent", "-v",
                   "sh", SCRIPT, NULL };

  /* Quick check that nbdkit is new enough */
  if (system ("nbdkit sh --dump-plugin | grep -q has_list_exports=1")) {
    fprintf (stderr, "skipping: nbdkit too old for this test\n");
    exit (77);
  }

  /* Get into negotiating state. */
  nbd = nbd_create ();
  if (nbd == NULL ||
      nbd_set_opt_mode (nbd, true) == -1 ||
      nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* First pass: server fails NBD_OPT_LIST. */
  /* XXX We can't tell the difference */
  if (nbd_opt_list (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_get_nr_list_exports (nbd)) != 0) {
    fprintf (stderr, "wrong number of exports, got %" PRId64 " expecting 0\n",
             r);
    exit (EXIT_FAILURE);
  }

  /* Second pass: server advertises 'a' and 'b'. */
  if (nbd_opt_list (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_get_nr_list_exports (nbd)) != 2) {
    fprintf (stderr, "wrong number of exports, got %" PRId64 " expecting 2\n",
             r);
    exit (EXIT_FAILURE);
  }
  name = nbd_get_list_export_name (nbd, 0);
  if (!name || strcmp (name, "a") != 0) {
    fprintf (stderr, "wrong first export %s, expecting 'a'\n", name ?: "(nil)");
    exit (EXIT_FAILURE);
  }
  free (name);
  name = nbd_get_list_export_name (nbd, 1);
  if (!name || strcmp (name, "b") != 0) {
    fprintf (stderr, "wrong first export %s, expecting 'b'\n", name ?: "(nil)");
    exit (EXIT_FAILURE);
  }
  free (name);

  /* Third pass: server advertises empty list. */
  if (nbd_opt_list (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_get_nr_list_exports (nbd)) != 0) {
    fprintf (stderr, "wrong number of exports, got %" PRId64 " expecting 0\n",
             r);
    exit (EXIT_FAILURE);
  }
  name = nbd_get_list_export_name (nbd, 0);
  if (name) {
    fprintf (stderr, "expecting error for out of bounds request\n");
    exit (EXIT_FAILURE);
  }

  /* Final pass: server advertises 'a'. */
  if (nbd_opt_list (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_get_nr_list_exports (nbd)) != 1) {
    fprintf (stderr, "wrong number of exports, got %" PRId64 " expecting 1\n",
             r);
    exit (EXIT_FAILURE);
  }
  name = nbd_get_list_export_name (nbd, 0);
  if (!name || strcmp (name, "a") != 0) {
    fprintf (stderr, "wrong first export %s, expecting 'a'\n", name ?: "(nil)");
    exit (EXIT_FAILURE);
  }
  free (name);

  nbd_opt_abort (nbd);
  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
