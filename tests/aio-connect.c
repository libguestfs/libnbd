/* NBD client library in userspace
 * Copyright (C) 2013-2021 Red Hat Inc.
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

/* Test connecting to an IPv4 TCP port using nbd_aio_connect. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libnbd.h>

#define PIDFILE "aio-connect.pid"

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int port;
  char port_str[16];
  pid_t pid;
  size_t i;
  struct sockaddr_in addr;

  unlink (PIDFILE);

  /* Pick a port at random, hope it's free. */
  srand (time (NULL) + getpid ());
  port = 32768 + (rand () & 32767);

  snprintf (port_str, sizeof port_str, "%d", port);

  pid = fork ();
  if (pid == -1) {
    perror ("fork");
    exit (EXIT_FAILURE);
  }
  if (pid == 0) {
    execlp ("nbdkit",
            "nbdkit", "-f", "-p", port_str, "-P", PIDFILE,
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

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  addr.sin_port = htons (port);

  if (nbd_aio_connect (nbd, (struct sockaddr *) &addr, sizeof addr) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Wait until we have connected. */
  while (!nbd_aio_is_ready (nbd)) {
    if (nbd_poll (nbd, -1) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
