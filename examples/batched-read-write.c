/* Example usage with nbdkit:
 *
 * nbdkit -U - --filter=noparallel memory 2M \
 *   --run './batched-read-write $unixsocket'
 *
 * This will attempt to batch a large aio read request immediately
 * followed by a large aio write request, prior to waiting for any
 * command replies from the server. A naive client that does not check
 * for available read data related to the first command while trying
 * to write data for the second command, coupled with a server that
 * only processes commands serially, would cause deadlock (both
 * processes fill up their write buffers waiting for a reader); thus,
 * this tests that libnbd is smart enough to always respond to replies
 * for in-flight requests even when it has batched up other commands
 * to write.
 *
 * To run it against a remote server over TCP:
 *
 * ./batched-read-write hostname port
 *  or
 * ./batched-read-write nbd://hostname:port
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <assert.h>
#include <signal.h>

#include <libnbd.h>

/* The single NBD handle. */
static struct nbd_handle *nbd;

/* Buffers used for the test. */
static char *in, *out;
static int64_t packetsize;

static int
try_deadlock (void *arg)
{
  struct pollfd fds[1];
  size_t i;
  int64_t cookies[2], done;
  unsigned dir;
  int r;

  /* Issue commands. */
  cookies[0] = nbd_aio_pread (nbd, in, packetsize, 0,
                              NBD_NULL_COMPLETION, 0);
  if (cookies[0] == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    goto error;
  }
  cookies[1] = nbd_aio_pwrite (nbd, out, packetsize, packetsize,
                               NBD_NULL_COMPLETION, 0);
  if (cookies[1] == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    goto error;
  }

  /* Now wait for commands to retire, or for deadlock to occur */
  while (nbd_aio_in_flight (nbd) > 0) {
    if (nbd_aio_is_dead (nbd) || nbd_aio_is_closed (nbd)) {
      fprintf (stderr, "connection is dead or closed\n");
      goto error;
    }

    fds[0].fd = nbd_aio_get_fd (nbd);
    fds[0].events = 0;
    fds[0].revents = 0;
    dir = nbd_aio_get_direction (nbd);
    if ((dir & LIBNBD_AIO_DIRECTION_READ) != 0)
      fds[0].events |= POLLIN;
    if ((dir & LIBNBD_AIO_DIRECTION_WRITE) != 0)
      fds[0].events |= POLLOUT;

    if (poll (fds, 1, -1) == -1) {
      perror ("poll");
      goto error;
    }

    if ((dir & LIBNBD_AIO_DIRECTION_READ) != 0 &&
        (fds[0].revents & POLLIN) != 0)
      nbd_aio_notify_read (nbd);
    else if ((dir & LIBNBD_AIO_DIRECTION_WRITE) != 0 &&
             (fds[0].revents & POLLOUT) != 0)
      nbd_aio_notify_write (nbd);

    /* If a command is ready to retire, retire it. */
    while ((done = nbd_aio_peek_command_completed (nbd)) > 0) {
      for (i = 0; i < sizeof cookies / sizeof cookies[0]; ++i) {
        if (cookies[i] == done) {
          r = nbd_aio_command_completed (nbd, cookies[i]);
          if (r == -1) {
            fprintf (stderr, "%s\n", nbd_get_error ());
            goto error;
          }
          assert (r == 1);
          cookies[i] = 0;
        }
      }
    }
  }
  assert (nbd_aio_in_flight (nbd) == 0);

  printf ("finished OK\n");

  return 0;

 error:
  fprintf (stderr, "failed\n");
  return -1;
}

static void
alarm_handler (int sig)
{
  fprintf (stderr, "alarm fired; deadlock probably occurred\n");
  _exit (EXIT_FAILURE);
}

int
main (int argc, char *argv[])
{
  int64_t exportsize;

  if (argc < 2 || argc > 3) {
    fprintf (stderr, "%s uri | socket | hostname port\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Connect synchronously as this is simpler. */
  if (argc == 2) {
    if (strstr (argv[1], "://")) {
      if (nbd_connect_uri (nbd, argv[1]) == -1) {
        fprintf (stderr, "%s\n", nbd_get_error ());
        exit (EXIT_FAILURE);
      }
    }
    else if (nbd_connect_unix (nbd, argv[1]) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }
  else {
    if (nbd_connect_tcp (nbd, argv[1], argv[2]) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  if (nbd_is_read_only (nbd) == 1) {
    fprintf (stderr, "%s: error: this NBD export is read-only\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  exportsize = nbd_get_size (nbd);
  if (exportsize == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  packetsize = exportsize / 2;
  if (packetsize > 2 * 1024 * 1024)
    packetsize = 2 * 1024 * 1024;

  in = malloc (packetsize);
  out = malloc (packetsize);
  if (!in || !out) {
    fprintf (stderr, "insufficient memory\n");
    exit (EXIT_FAILURE);
  }

  /* Attempt to be non-destructive, by writing what file already contains */
  if (nbd_pread (nbd, out, packetsize, packetsize, 0) == -1) {
    fprintf (stderr, "sync read failed: %s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* When not debugging, set an alarm, in case this test deadlocks
   * instead of succeeding
   */
  if (nbd_get_debug (nbd) < 1) {
    signal (SIGALRM, alarm_handler);
    alarm (10);
  }

  if (try_deadlock (NULL) == -1)
    exit (EXIT_FAILURE);

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);

  return EXIT_SUCCESS;
}
