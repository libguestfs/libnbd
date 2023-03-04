/* NBD client library in userspace
 * Copyright Red Hat
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libnbd.h>

static char *progname;

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;

  progname = argv[0];

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Attempt to set an enum to garbage. */
  if (nbd_set_tls (nbd, LIBNBD_TLS_REQUIRE + 1) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_set_tls did not reject invalid enum\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  if (nbd_get_tls (nbd) != LIBNBD_TLS_DISABLE) {
    fprintf (stderr, "%s: test failed: "
             "nbd_get_tls not left at default value\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
