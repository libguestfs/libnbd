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
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
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
/* We do use exec, but only when using --write, which is a maintainer
 * operation that always runs on Linux.
 */
#define SOCK_CLOEXEC 0
#endif

/* If defined, instead of creating the fuzzer wrapper, write a working
 * testcase to the named file.  This runs nbdkit and captures the
 * input to libnbd (output from nbdkit) in the file.  Use it like
 * this:
 *
 * Generate test case:
 * $ fuzzing/libnbd-fuzz-wrapper --write testcase
 * Test it:
 * $ LIBNBD_DEBUG=1 fuzzing/libnbd-fuzz-wrapper testcase
 * If it's good, copy it to the test case directory:
 * $ mv testcase fuzzing/testcase_dir/
 */
static const char *write_testcase;

static void client (int s);
static void server (int fd, int s);
static void nbdkit (int fd, int s);

int
main (int argc, char *argv[])
{
  int fd;
  pid_t pid;
  int sv[2], r, status;

  if (argc == 3 && strcmp (argv[1], "--write") == 0) {
    write_testcase = argv[2];

    fd = open (argv[2], O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd == -1) {
      perror (argv[1]);
      exit (EXIT_FAILURE);
    }
  }
  else if (argc == 2) {
    /* Open the test case before we fork so we know the file exists. */
    fd = open (argv[1], O_RDONLY);
    if (fd == -1) {
      perror (argv[1]);
      exit (EXIT_FAILURE);
    }
  }
  else {
    fprintf (stderr, "libnbd-fuzz-wrapper testcase\n");
    exit (EXIT_FAILURE);
  }

  /* Create a connected socket. */
  if (socketpair (AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0, sv) == -1) {
    perror ("socketpair");
    exit (EXIT_FAILURE);
  }

  /* Fork: The parent will be the libnbd process (client).  The child
   * will be the (usually phony) NBD server listening on the socket.
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

  /* Child: NBD server. */
  close (sv[0]);

  if (!write_testcase)
    server (fd, sv[1]);
  else
    nbdkit (fd, sv[1]);

  close (sv[1]);

  _exit (EXIT_SUCCESS);
}

/* Structured reads callback, does nothing. */
static int
chunk_callback (void *user_data, const void *subbuf,
                size_t count, uint64_t offset,
                unsigned status, int *error)
{
  //fprintf (stderr, "chunk called: %" PRIu64 " %zu %u\n",
  //         offset, count, status);
  return 0;
}

/* Block status (extents) callback, does nothing. */
static int
extent_callback (void *user_data,
                 const char *metacontext,
                 uint64_t offset, uint32_t *entries,
                 size_t nr_entries, int *error)
{
  //fprintf (stderr, "extent called, nr_entries = %zu\n", nr_entries);
  return 0;
}

/* This is the client (parent process) running libnbd. */
static char buf[512];
static char prbuf[65536];

static void
client (int sock)
{
  struct nbd_handle *nbd;
  int64_t length;

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

  length = nbd_get_size (nbd);

  /* Test common synchronous I/O calls. */
  nbd_pread (nbd, buf, sizeof buf, 0, 0);
  nbd_pwrite (nbd, buf, sizeof buf, 0, 0);
  nbd_flush (nbd, 0);
  nbd_trim (nbd, 8192, 8192, 0);
  nbd_zero (nbd, 8192, 65536, 0);
  nbd_cache (nbd, 8192, 0, 0);

  /* Test structured reads. */
  nbd_pread_structured (nbd, prbuf, sizeof prbuf, 8192,
                        (nbd_chunk_callback) {
                          .callback = chunk_callback,
                          .user_data = NULL,
                          .free = NULL
                        },
                        0);

  /* Test block status. */
  nbd_block_status (nbd, length, 0,
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

static void xwrite (int fd, const char *buf, size_t n);

/* This is used for --write mode where we capture nbdkit output into a
 * testcase file.
 */
static void
nbdkit (int fd, int sock)
{
  pid_t pid;
  int rfd[2], wfd[2];
  struct pollfd pfds[2];
  char buf[512];
  ssize_t r;
  bool parent_dead = false;

  if (pipe (rfd) == -1) {       /* Will be our input, nbdkit's stdout */
    perror ("pipe");
    _exit (EXIT_FAILURE);
  }

  if (pipe (wfd) == -1) {       /* Will be our output, nbdkit's stdin */
    perror ("pipe");
    _exit (EXIT_FAILURE);
  }

  /* Run nbdkit as another subprocess. */
  pid = fork ();
  if (pid == -1) {
    perror ("fork");
    _exit (EXIT_FAILURE);
  }

  if (pid == 0) {               /* Child - nbdkit */
    close (fd);
    close (sock);

    close (rfd[0]);
    dup2 (rfd[1], STDOUT_FILENO);

    close (wfd[1]);
    dup2 (wfd[0], STDIN_FILENO);

    execlp ("nbdkit",
            "nbdkit", "--exit-with-parent", "-s",
            "sparse-random",
            "--filter=cow",
            "size=1M",
            "runlength=8192", "percent=50", "random-content=true",
            NULL);
    perror ("execlp");
    _exit (EXIT_FAILURE);
  }

  /* Here we shuffle the data:
   *
   * Our parent         This process            Our child
   * (libnbd)                                   (nbdkit)
   *      ------------->   ------>  -----wfd[1]---->
   *          sock
   *      <-------------  <------  /<----rfd[0]-----
   *                              |
   *                              v
   *             writes from nbdkit tee'd to fd (testcase)
   *
   * We do everything blocking because that is easier and performance
   * doesn't matter since we're only capturing a test case.
   */
  close (rfd[1]);
  close (wfd[0]);

  while (!parent_dead && rfd[0] >= 0) {
    pfds[0].fd = parent_dead ? -1 : sock;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;

    pfds[1].fd = rfd[0];
    pfds[1].events = POLLIN;
    pfds[1].revents = 0;

    if (poll (pfds, 2, -1) == -1) {
      if (errno == EINTR)
        continue;
      perror ("poll");
      _exit (EXIT_FAILURE);
    }

    if (!parent_dead && (pfds[0].revents & POLLIN) != 0) {
      r = read (sock, buf, sizeof buf);
      if (r == -1 && errno != EINTR) {
        perror ("read (libnbd)");
        _exit (EXIT_FAILURE);
      }
      else if (r == 0) {
        parent_dead = true;
        continue;
      }
      else if (r > 0)
        xwrite (wfd[1], buf, r);
    }

    if (rfd[0] != -1 && (pfds[1].revents & POLLIN) != 0) {
      r = read (rfd[0], buf, sizeof buf);
      if (r == -1 && errno == EINTR) {
        perror ("read (nbdkit)");
        _exit (EXIT_FAILURE);
      }
      else if (r == 0) {
        close (rfd[0]);
        rfd[0] = -1;
        continue;
      }
      else if (r > 0) {
        xwrite (fd, buf, r);
        xwrite (sock, buf, r);
      }
    }
  }

  if (close (fd) == -1) {
    perror ("close");
    _exit (EXIT_FAILURE);
  }
}

static void
xwrite (int fd, const char *buf, size_t n)
{
  ssize_t r;

  while (n > 0) {
    r = write (fd, buf, n);
    if (r == -1) {
      perror ("write");
      _exit (EXIT_FAILURE);
    }
    buf += r;
    n -= r;
  }
}
