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

/* This is a libFuzzer test case for libnbd.  The way it works is
 * similar to the AFL wrapper (libnbd-fuzz-wrapper), feeding the
 * binary data into the libnbd socket from a forked process.  But the
 * mechanics are slightly different because libFuzzer calls a C
 * function with the test case instead of providing it in a file.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <libnbd.h>

static void client (int sock);
static void server (const uint8_t *data, size_t size, int sock);

/* This is the entry point called by libFuzzer. */
int
LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
  pid_t pid;
  int sv[2], r, status;

  /* Create a connected socket. */
  if (socketpair (AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0, sv) == -1) {
    perror ("socketpair");
    exit (EXIT_FAILURE);
  }

  /* Fork: The parent will be the libnbd process (client).  The child
   * will be the phony NBD server listening on the socket.
   */
  pid = fork ();
  if (pid == -1) {
    perror ("fork");
    exit (EXIT_FAILURE);
  }

  if (pid > 0) {
    /* Parent: libnbd client. */
    close (sv[1]);

    client (sv[0]);

    close (sv[0]);

  again:
    r = wait (&status);
    if (r == -1) {
      if (errno == EINTR)
        goto again;
      perror ("wait");
      exit (EXIT_FAILURE);
    }
    if (!WIFEXITED (status) || WEXITSTATUS (status) != 0)
      fprintf (stderr, "bad exit status %d\n", status);

    return 0;
  }

  /* Child: phony NBD server. */
  close (sv[0]);

  server (data, size, sv[1]);

  close (sv[1]);

  _exit (EXIT_SUCCESS);
}

static void
client (int sock)
{
  struct nbd_handle *nbd;
  char buf[512];

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Note we ignore errors in these calls because we are only
   * interested in whether the process crashes.
   */
  /* This tests the handshake phase. */
  nbd_connect_socket (nbd, sock);

  nbd_pread (nbd, buf, sizeof buf, 0, 0);
  nbd_pwrite (nbd, buf, sizeof buf, 0, 0);
  nbd_flush (nbd, 0);
  nbd_trim (nbd, 512, 0, 0);
  nbd_cache (nbd, 512, 0, 0);

  /* XXX Test structured reads and block status. */

  nbd_shutdown (nbd, 0);
  nbd_close (nbd);
}

static void
server (const uint8_t *data, size_t size, int sock)
{
  struct pollfd pfds[1];
  char rbuf[512];
  ssize_t r;

  if (size == 0)
    shutdown (sock, SHUT_WR);

  for (;;) {
    pfds[0].fd = sock;
    pfds[0].events = POLLIN;
    if (size > 0) pfds[0].events |= POLLOUT;
    pfds[0].revents = 0;

    if (poll (pfds, 1, -1) == -1) {
      if (errno == EINTR)
        continue;
      perror ("poll");
      /* This is not an error. */
      return;
    }

    /* We can read from the client socket.  Just throw away anything sent. */
    if ((pfds[0].revents & POLLIN) != 0) {
      r = read (sock, rbuf, sizeof rbuf);
      if (r == -1 && errno != EINTR) {
        //perror ("read");
        return;
      }
      else if (r == 0)          /* end of input from the server */
        return;
    }

    /* We can write to the client socket. */
    if ((pfds[0].revents & POLLOUT) != 0) {
      if (size > 0) {
        r = write (sock, data, size);
        if (r == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
          perror ("write");
          return;
        }
        else if (r > 0) {
          data += r;
          size -= r;

          if (size == 0)
            shutdown (sock, SHUT_WR);
        }
      }
    }
  } /* for (;;) */
}
