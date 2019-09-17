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

/* Simple test which connects to nbdkit and reads the size.
 * Essentially testing the newstyle handshake.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include <libnbd.h>

#define SIZE 123456789
#define XSTR(s) #s
#define STR(s) XSTR(s)

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int64_t r;
  /* -n forces newstyle even if someone is still using nbdkit < 1.3 */
  char *args[] = { "nbdkit", "-s", "--exit-with-parent", "-n", "-v",
                   "null", "size=" STR(SIZE), NULL };
  const char *s;

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Even ancient versions of nbdkit only supported newstyle-fixed. */
  s = nbd_get_protocol (nbd);
  if (strcmp (s, "newstyle-fixed") != 0) {
    fprintf (stderr,
             "incorrect protocol \"%s\", expected \"newstyle-fixed\"\n", s);
    exit (EXIT_FAILURE);
  }

  if ((r = nbd_get_size (nbd)) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (r != SIZE) {
    fprintf (stderr, "%s: test failed: incorrect size, "
             "actual %" PRIi64 ", expected %d",
             argv[0], r, SIZE);
    exit (EXIT_FAILURE);
  }

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
