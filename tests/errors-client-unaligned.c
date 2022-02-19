/* NBD client library in userspace
 * Copyright (C) 2013-2022 Red Hat Inc.
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
#include <unistd.h>
#include <sys/stat.h>

#include <libnbd.h>

#include "requires.h"

static char *progname;
static char buf[512];

static void
check (int experr, const char *prefix)
{
  const char *msg = nbd_get_error ();
  int errnum = nbd_get_errno ();

  fprintf (stderr, "error: \"%s\"\n", msg);
  fprintf (stderr, "errno: %d (%s)\n", errnum, strerror (errnum));
  if (strncmp (msg, prefix, strlen (prefix)) != 0) {
    fprintf (stderr, "%s: test failed: missing context prefix: %s\n",
             progname, msg);
    exit (EXIT_FAILURE);
  }
  if (errnum != experr) {
    fprintf (stderr, "%s: test failed: "
             "expected errno = %d (%s), but got %d\n",
             progname, experr, strerror (experr), errnum);
    exit (EXIT_FAILURE);
  }
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  const char *cmd[] = {
    "nbdkit", "-s", "-v", "--exit-with-parent",
    "memory", "1024",
    "--filter=blocksize-policy", "blocksize-minimum=512",
    NULL
  };
  uint32_t strict;

  progname = argv[0];

  requires ("nbdkit --version --filter=blocksize-policy null");

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Connect to the server. */
  if (nbd_connect_command (nbd, (char **) cmd) == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Send an unaligned read, client-side */
  strict = nbd_get_strict_mode (nbd) | LIBNBD_STRICT_ALIGN;
  if (nbd_set_strict_mode (nbd, strict) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_aio_pread (nbd, buf, 1, 1, NBD_NULL_COMPLETION, 0) != -1) {
    fprintf (stderr, "%s: test failed: "
             "unaligned nbd_aio_pread did not fail\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (EINVAL, "nbd_aio_pread: ");

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
