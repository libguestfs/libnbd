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

/* Deliberately provoke some errors and check the error messages from
 * nbd_get_error etc look reasonable.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libnbd.h>

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  const char *msg;
  int errnum;

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Issue a connected command when not connected. */
  if (nbd_pread (nbd, NULL, 0, 0) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_pread did not fail on non-connected handle\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  msg = nbd_get_error ();
  errnum = nbd_get_errno ();
  printf ("error: \"%s\"\n", msg);
  printf ("errno: %d (%s)\n", errnum, strerror (errnum));
  if (strncmp (msg, "nbd_pread: ", strlen ("nbd_pread: ")) != 0) {
    fprintf (stderr, "%s: test failed: missing context prefix: %s\n",
             argv[0], msg);
    exit (EXIT_FAILURE);
  }
  if (errnum != ENOTCONN) {
    fprintf (stderr, "%s: test failed: "
             "expected errno = ENOTCONN, but got %d\n",
             argv[0], errnum);
    exit (EXIT_FAILURE);
  }

  /* XXX Test some more stuff here. */

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
