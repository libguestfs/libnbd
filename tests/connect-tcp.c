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

/* Test connecting to a TCP port. */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include <libnbd.h>

#define PIDFILE "connect-tcp.pid"

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int port;
  char cmd[80];
  char port_str[16];
  size_t i;

  unlink (PIDFILE);

  /* Pick a port at random, hope it's free. */
  srand (time (NULL));
  port = 32768 + (rand () & 32767);

  snprintf (port_str, sizeof port_str, "%d", port);
  snprintf (cmd, sizeof cmd,
            "nbdkit -f -p %d -P %s --exit-with-parent null &",
            port, PIDFILE);
  if (system (cmd) != 0) {
    fprintf (stderr, "%s: could not run: %s", argv[0], cmd);
    exit (EXIT_FAILURE);
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
    perror ("nbd_create");
    exit (EXIT_FAILURE);
  }

  if (nbd_connect_tcp (nbd, "localhost", port_str) == -1) {
    /* XXX PRINT ERROR */
    exit (EXIT_FAILURE);
  }

  if (nbd_shutdown (nbd) == -1) {
    /* XXX PRINT ERROR */
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
