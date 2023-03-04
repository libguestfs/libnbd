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

/* Test connecting using systemd socket activation. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <libnbd.h>

#include "requires.h"

int
main (int argc, char *argv[])
{
  char *args[] = {"nbdkit", "-f", "memory", "size=1m", NULL};
  struct nbd_handle *nbd;
  char *uri = NULL;
  int result = EXIT_FAILURE;

  requires ("nbdkit --version");
  requires ("nbdkit memory --version");

  printf ("Connecting via systemd socket activation...\n");

  nbd = nbd_create ();
  if (nbd == NULL)
    goto out;

  if (nbd_connect_systemd_socket_activation (nbd, args) == -1)
    goto out;

  /* Libnbd creates unix socket internally, but this is not documented yet. */
  uri = nbd_get_uri (nbd);

  printf ("Connected to %s\n", uri);
  result = EXIT_SUCCESS;

out:
  if (result == EXIT_FAILURE)
    fprintf (stderr, "%s\n", nbd_get_error ());

  free (uri);
  nbd_close (nbd);

  exit (result);
}
