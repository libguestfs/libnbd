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

static const char *exports[] = { EXPORTS };
static const char *descriptions[] = { DESCRIPTIONS };
static const size_t nr_exports = sizeof exports / sizeof exports[0];

static char *progname;

static int
check (void *opaque, const char *name, const char *description)
{
  size_t *i = opaque;
  if (*i == nr_exports) {
    fprintf (stderr, "%s: server returned more exports than expected",
             progname);
    exit (EXIT_FAILURE);
  }
  if (strcmp (exports[*i], name) != 0) {
    fprintf (stderr, "%s: expected export \"%s\", but got \"%s\"\n",
             progname, exports[*i], name);
    exit (EXIT_FAILURE);
  }
  if (strcmp (descriptions[*i], description) != 0) {
    fprintf (stderr, "%s: expected description \"%s\", but got \"%s\"\n",
             progname, descriptions[*i], description);
    exit (EXIT_FAILURE);
  }
  (*i)++;
  return 0;
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  char tmpfile[] = "/tmp/nbdXXXXXX";
  int fd;
  size_t i = 0;

  progname = argv[0];

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

  /* Set option mode in the handle. */
  nbd_set_opt_mode (nbd, true);

  /* Run qemu-nbd. */
  char *args[] = { SERVER, SERVER_PARAMS, NULL };
#if SOCKET_ACTIVATION
#define NBD_CONNECT nbd_connect_systemd_socket_activation
#else
#define NBD_CONNECT nbd_connect_command
#endif
  if (NBD_CONNECT (nbd, args) == -1 ||
      nbd_opt_list (nbd, (nbd_list_callback) {
          .callback = check, .user_data = &i}) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    unlink (tmpfile);
    exit (EXIT_FAILURE);
  }

  /* We don't need the temporary file any longer. */
  unlink (tmpfile);

  /* Check for expected number of exports. */
  if (i != nr_exports) {
    fprintf (stderr, "%s: expected %zu export, but got %zu\n",
             argv[0], nr_exports, i);
    exit (EXIT_FAILURE);
  }

  nbd_opt_abort (nbd);
  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
