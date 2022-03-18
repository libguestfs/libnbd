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

/* This is a wrapper allowing libnbd to be tested using common fuzzers
 * such as afl.  It takes the fuzzer test case as a filename on the
 * command line.  This is fed to the libnbd socket.  Any output to the
 * socket from libnbd is sent to /dev/null.  This is basically the
 * same way we fuzz nbdkit, but in reverse (see nbdkit.git/fuzzing).
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

#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0 /* This file doesn't use exec */
#endif

static void client (int s);
static void server (int fd, int s);

int
main (int argc, char *argv[])
{
  int fd;
  pid_t pid;
  int sv[2], r, status;

  if (argc != 2) {
    fprintf (stderr, "libnbd-fuzz-wrapper testcase\n");
    exit (EXIT_FAILURE);
  }

  /* Open the test case before we fork so we know the file exists. */
  fd = open (argv[1], O_RDONLY);
  if (fd == -1) {
    perror (argv[1]);
    exit (EXIT_FAILURE);
  }

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
    close (fd);

    client (sv[0]);

    close (sv[0]);

    r = wait (&status);
    if (r == -1) {
      perror ("wait");
      exit (EXIT_FAILURE);
    }
    if (!WIFEXITED (status) || WEXITSTATUS (status) != 0)
      exit (EXIT_FAILURE);
    else
      exit (EXIT_SUCCESS);
  }

  /* Child: phony NBD server. */
  close (sv[0]);

  server (fd, sv[1]);

  close (sv[1]);

  _exit (EXIT_SUCCESS);
}

/* Structured reads callback, does nothing. */
static int
chunk_callback (void *user_data, const void *subbuf,
                size_t count, uint64_t offset,
                unsigned status, int *error)
{
  return 0;
}

/* Block status (extents) callback, does nothing. */
static int
extent_callback (void *user_data,
                 const char *metacontext,
                 uint64_t offset, uint32_t *entries,
                 size_t nr_entries, int *error)
{
  return 0;
}

/* This is the client (parent process) running libnbd. */
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

  /* Enable a metadata context, for block status below. */
  nbd_add_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION);

  /* This tests the handshake phase. */
  nbd_connect_socket (nbd, sock);

  /* Test common synchronous I/O calls. */
  nbd_pread (nbd, buf, sizeof buf, 0, 0);
  nbd_pwrite (nbd, buf, sizeof buf, 0, 0);
  nbd_flush (nbd, 0);
  nbd_trim (nbd, 512, 0, 0);
  nbd_cache (nbd, 512, 0, 0);

  /* Test structured reads. */
  nbd_pread_structured (nbd, buf, sizeof buf, 0,
                        (nbd_chunk_callback) {
                          .callback = chunk_callback,
                          .user_data = NULL,
                          .free = NULL
                        },
                        0);

  /* Test block status. */
  nbd_block_status (nbd, sizeof buf, 0,
                    (nbd_extent_callback) {
                      .callback = extent_callback,
                      .user_data = NULL,
                      .free = NULL
                    },
                    0);

  nbd_shutdown (nbd, 0);
}

/* This is the server (child process) acting like an NBD server. */
static void
server (int fd, int sock)
{
  struct pollfd pfds[1];
  char rbuf[512], wbuf[512];
  size_t wsize = 0;
  ssize_t r;

  for (;;) {
    pfds[0].fd = sock;
    pfds[0].events = POLLIN;
    if (wsize > 0 || fd >= 0) pfds[0].events |= POLLOUT;
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
        perror ("read");
        return;
      }
      else if (r == 0)          /* end of input from the server */
        return;
    }

    /* We can write to the client socket. */
    if ((pfds[0].revents & POLLOUT) != 0) {
      /* Write more data from the wbuf. */
      if (wsize > 0) {
      morewrite:
        r = write (sock, wbuf, wsize);
        if (r == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
          perror ("write");
          return;
        }
        else if (r > 0) {
          memmove (wbuf, &wbuf[r], wsize-r);
          wsize -= r;
        }
      }
      /* Write more data from the file. */
      else if (fd >= 0) {
        r = read (fd, wbuf, sizeof wbuf);
        if (r == -1) {
          perror ("read");
          _exit (EXIT_FAILURE);
        }
        else if (r == 0) {
          fd = -1;              /* ignore the file from now on */
          shutdown (sock, SHUT_WR);
        }
        else {
          wsize = r;
          goto morewrite;
        }
      }
    }
  } /* for (;;) */
}
