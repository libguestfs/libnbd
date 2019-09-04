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

/* Simple end-to-end test of flags. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <libnbd.h>

#ifndef value
#define value true
#endif

#define native 2
#define none 3

/* https://stackoverflow.com/a/1489985 */
#define XNBD_FLAG_FUNCTION(f) nbd_ ## f
#define NBD_FLAG_FUNCTION(f) XNBD_FLAG_FUNCTION(f)

#define XSTR(x) #x
#define STR(x) XSTR(x)

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int r, expected;
  char plugin_path[256];
  char key_param[32];

#ifdef require
  if (system ("nbdkit --dump-plugin sh | grep -q " require)) {
    fprintf (stderr, "skipping: nbdkit lacks support for '%s'\n", require);
    exit (77);
  }
#endif

  snprintf (plugin_path, sizeof plugin_path, "%s/eflags-plugin.sh",
            getenv ("srcdir") ? : ".");
  snprintf (key_param, sizeof key_param, "key=%s", STR (flag));

  char *args[] =
    { "nbdkit", "-s", "--exit-with-parent", "-v",
#ifdef filter
      filter,
#endif
      "sh", plugin_path,
      key_param,
#if value == true
      "print=", "rc=0",
#elif value == false
      "print=", "rc=3",
#elif value == native
      "print=native", "rc=0",
#elif value == none
      "print=none", "rc=0",
#else
#error "unknown value"
#endif
      NULL };

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
#ifdef no_sr
  if (nbd_set_request_structured_replies (nbd, false) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
#endif
  if (nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

#if value == true
  expected = 1;
#elif value == false
  expected = 0;
#elif value == native
  /* can_fua=native should return true */
  expected = 1;
#elif value == none
  /* can_fua=none should return false */
  expected = 0;
#else
#error "unknown value"
#endif

  if ((r = NBD_FLAG_FUNCTION(flag) (nbd)) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (r != expected) {
    fprintf (stderr, "%s: test failed: unexpected %s flag: "
             "actual=%d, expected=%d\n",
             argv[0], STR (flag), r, expected);
    exit (EXIT_FAILURE);
  }

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
