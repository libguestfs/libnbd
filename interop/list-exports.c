/* NBD client library in userspace
 * Copyright (C) 2020 Red Hat Inc.
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

/* Test nbd_set_list_exports against qemu-nbd. */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>

#include <libnbd.h>

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  char tmpfile[] = "/tmp/nbdXXXXXX";
  int fd, r;
  size_t i;
  char *name, *desc;

  /* Create a sparse temporary file. */
  fd = mkstemp (tmpfile);
  if (fd == -1 ||
      ftruncate (fd, 1024) == -1 ||
      close (fd) == -1) {
    perror (tmpfile);
    exit (EXIT_FAILURE);
  }

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    unlink (tmpfile);
    exit (EXIT_FAILURE);
  }

  /* Set the list exports mode in the handle. */
  nbd_set_list_exports (nbd, true);

  /* Run qemu-nbd. */
  char *args[] = { SERVER, SERVER_PARAMS, NULL };
#if SOCKET_ACTIVATION
#define NBD_CONNECT nbd_connect_systemd_socket_activation
#else
#define NBD_CONNECT nbd_connect_command
#endif
  if (NBD_CONNECT (nbd, args) == -1)
    /* This is not an error so don't fail here. */
    fprintf (stderr, "%s\n", nbd_get_error ());

  /* We don't need the temporary file any longer. */
  unlink (tmpfile);

  /* Check for expected number of exports. */
  const char *exports[] = { EXPORTS };
  const char *descriptions[] = { DESCRIPTIONS };
  const size_t nr_exports = sizeof exports / sizeof exports[0];
  r = nbd_get_nr_list_exports (nbd);
  if (r != nr_exports) {
    fprintf (stderr, "%s: expected %zu export, but got %d\n",
             argv[0], nr_exports, r);
    exit (EXIT_FAILURE);
  }

  /* Check the export names and descriptions. */
  for (i = 0; i < nr_exports; ++i) {
    name = nbd_get_list_export_name (nbd, (int) i);
    if (strcmp (name, exports[i]) != 0) {
      fprintf (stderr, "%s: expected export \"%s\", but got \"%s\"\n",
               argv[0], exports[i], name);
      exit (EXIT_FAILURE);
    }
    free (name);
    desc = nbd_get_list_export_description (nbd, (int) i);
    if (strcmp (desc, descriptions[i]) != 0) {
      fprintf (stderr, "%s: expected description \"%s\", but got \"%s\"\n",
               argv[0], descriptions[i], desc);
      exit (EXIT_FAILURE);
    }
    free (desc);
  }

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
