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

/* Check that LIBNBD_DEBUG=0|1 affects the debug flag. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <libnbd.h>

static int
get_debug_flag (void)
{
  struct nbd_handle *nbd;
  int r;

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  r = nbd_get_debug (nbd);
  if (r == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);

  return r;
}

int
main (int argc, char *argv[])
{
  setenv ("LIBNBD_DEBUG", "1", 1);
  assert (get_debug_flag () == 1);

  setenv ("LIBNBD_DEBUG", "0", 1);
  assert (get_debug_flag () == 0);

  setenv ("LIBNBD_DEBUG", "", 1);
  assert (get_debug_flag () == 0);

  unsetenv ("LIBNBD_DEBUG");
  assert (get_debug_flag () == 0);

  exit (EXIT_SUCCESS);
}
