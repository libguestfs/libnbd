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

/* Test connecting over a Unix domain socket. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <libnbd.h>

#define PIDFILE "connect-unix.pid"

int
main (int argc, char *argv[])
{
  char socket[] = "/tmp/connect-unix-sock-XXXXXX";
  struct nbd_handle *nbd;
  pid_t pid;
  size_t i;

  if (mkstemp (socket) == -1) {
    perror (socket);
    exit (EXIT_FAILURE);
  }

  unlink (socket);
  unlink (PIDFILE);

  pid = fork ();
  if (pid == -1) {
    perror ("fork");
    exit (EXIT_FAILURE);
  }
  if (pid == 0) {
    execlp ("nbdkit",
            "nbdkit", "-f", "-U", socket, "-P", PIDFILE,
            "--exit-with-parent", "null", NULL);
    perror ("nbdkit");
    _exit (EXIT_FAILURE);
  }

  /* Wait for nbdkit to start listening. */
  for (i = 0; i < 60; ++i) {
    if (access (PIDFILE, F_OK) == 0)
      break;
    sleep (1);
  }
  unlink (PIDFILE);

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_connect_unix (nbd, socket) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
  unlink (socket);
  exit (EXIT_SUCCESS);
}
