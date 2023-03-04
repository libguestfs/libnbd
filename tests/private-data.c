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

/* Test the private data field. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libnbd.h>

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd1, *nbd2;

  nbd1 = nbd_create ();
  if (nbd1 == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd2 = nbd_create ();
  if (nbd2 == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Check the field initially reads as zero. */
  assert (nbd_get_private_data (nbd1) == 0);
  assert (nbd_get_private_data (nbd2) == 0);

  /* Set and read back the field. */
  assert (nbd_set_private_data (nbd1, 42) == 0);
  assert (nbd_get_private_data (nbd1) == 42);

  /* Setting the field in one handle shouldn't affect the other. */
  assert (nbd_set_private_data (nbd2, 999) == 0);
  assert (nbd_get_private_data (nbd1) == 42);
  assert (nbd_get_private_data (nbd2) == 999);
  assert (nbd_set_private_data (nbd1, 43) == 42);
  assert (nbd_get_private_data (nbd2) == 999);
  assert (nbd_set_private_data (nbd2, 998) == 999);
  assert (nbd_get_private_data (nbd1) == 43);

  /* Check that (in C) we can store and retrieve a pointer. */
  nbd_set_private_data (nbd1, (uintptr_t) &nbd_close);
  assert (nbd_get_private_data (nbd1) == (uintptr_t) &nbd_close);

  nbd_close (nbd2);
  nbd_close (nbd1);
  exit (EXIT_SUCCESS);
}
