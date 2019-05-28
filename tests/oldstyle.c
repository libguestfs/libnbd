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

/* Test interoperability with oldstyle servers. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include <libnbd.h>

#define SIZE 65536
#define XSTR(s) #s
#define STR(s) XSTR(s)

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  char wbuf[512] = { 1, 2, 3, 4 }, rbuf[512];
  int64_t r;
  char *args[] = { "nbdkit", "-s", "-o", "--exit-with-parent", "-v",
                   "memory", "size=" STR(SIZE), NULL };

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
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

  if (nbd_pwrite (nbd, wbuf, sizeof wbuf, 0, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_pread (nbd, rbuf, sizeof rbuf, 0, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (memcmp (rbuf, wbuf, sizeof rbuf) != 0) {
    fprintf (stderr, "%s: DATA INTEGRITY ERROR!\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  if (nbd_shutdown (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
