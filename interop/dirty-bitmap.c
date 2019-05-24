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

/* Test reading qemu dirty bitmap. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <libnbd.h>

static const char *unixsocket;
static const char *bitmap;

static void
cb (void *data, const char *metacontext, uint64_t offset,
    uint32_t *entries, size_t len)
{
  if (strcmp (metacontext, bitmap) != 0)
    return;

  assert (len == 10);

  assert (entries[0] ==  65536); assert (entries[1] == 0);
  /* dirty block offset 64K size 64K */
  assert (entries[2] ==  65536); assert (entries[3] == 1);
  assert (entries[4] == 393216); assert (entries[5] == 0);
  /* dirty block offset 512K size 64K */
  assert (entries[6] ==  65536); assert (entries[7] == 1);
  assert (entries[8] == 458752); assert (entries[9] == 0);
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int64_t exportsize;

  if (argc != 3) {
    fprintf (stderr, "%s unixsocket bitmap\n", argv[0]);
    exit (EXIT_FAILURE);
  }
  unixsocket = argv[1];
  bitmap = argv[2];

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_request_meta_context (nbd, bitmap);

  if (nbd_connect_unix (nbd, unixsocket) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  exportsize = nbd_get_size (nbd);
  if (exportsize == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_block_status (nbd, exportsize, 0, 0, NULL, cb) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_shutdown (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);

  exit (EXIT_SUCCESS);
}
