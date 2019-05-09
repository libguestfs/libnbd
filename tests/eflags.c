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
  char cmd[256];
  int r, expected;

  snprintf (cmd, sizeof cmd,
            "nbdkit -s --exit-with-parent -v "
            "sh %s/eflags-plugin.sh "
            "key=%s print=%s rc=%d",
            getenv ("srcdir") ? : ".",
            STR (flag),
#if value == true
            "", 0
#elif value == false
            "", 3
#elif value == native
            "native", 0
#elif value == none
            "none", 0
#else
#error "unknown value"
#endif
            );

  nbd = nbd_create ();
  if (nbd == NULL) {
    perror ("nbd_create");
    exit (EXIT_FAILURE);
  }
  if (nbd_connect_command (nbd, cmd) == -1) {
    /* XXX PRINT ERROR */
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
    /* XXX PRINT ERROR */
    exit (EXIT_FAILURE);
  }

  if (r != expected) {
    fprintf (stderr, "%s: test failed: unexpected %s flag: "
             "actual=%d, expected=%d\n",
             argv[0], STR (flag), r, expected);
    exit (EXIT_FAILURE);
  }

  if (nbd_shutdown (nbd) == -1) {
    /* XXX PRINT ERROR */
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
