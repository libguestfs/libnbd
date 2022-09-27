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

/* GCC will warn that we are passing NULL (or worse), so to do this
 * test we must remove the header file attribute.
 */
#define LIBNBD_ATTRIBUTE_NONNULL(s)
#include <libnbd.h>

static char *progname;

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
  const char *cmd[] = { NULL };

  progname = argv[0];

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Connect to an invalid command. */
  if (nbd_connect_command (nbd, NULL) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_connect_command did not reject NULL argv\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (EFAULT, "nbd_connect_command: ");

  if (nbd_connect_command (nbd, (char **) cmd) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_connect_command did not reject empty argv\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (EINVAL, "nbd_connect_command: ");

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
