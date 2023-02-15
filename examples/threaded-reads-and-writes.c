/* Example usage with nbdkit:
 *
 * nbdkit -U - memory 100M --run './threaded-reads-and-writes $unixsocket'
 *
 * This will read and write randomly over the first megabyte of the
 * plugin using multi-conn, multiple threads and multiple requests in
 * flight on each thread.
 *
 * To run it against a remote server over TCP:
 *
 * ./threaded-reads-and-writes hostname port
 *  or
 * ./threaded-reads-and-writes nbd://hostname:port
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

#include <pthread.h>

#include <libnbd.h>

static int64_t exportsize;

/* Number of simultaneous connections to the NBD server.  This is also
 * the number of threads, because each thread manages one connection.
 * Note that some servers only support a limited number of
 * simultaneous connections, and/or have a configurable thread pool
 * internally, and if you exceed those limits then something will
 * break.
 */
#define NR_MULTI_CONN 8

/* Number of commands that can be "in flight" at the same time on each
 * connection.  (Therefore the total number of requests in flight may
 * be up to NR_MULTI_CONN * MAX_IN_FLIGHT).  See libnbd(3) section
 * "Issuing multiple in-flight requests".
 */
#define MAX_IN_FLIGHT 64

/* The size of large reads and writes, must be > 512. */
#define BUFFER_SIZE (1024*1024)

/* Number of commands we issue (per thread). */
#define NR_CYCLES 10000

struct thread_status {
  size_t i;                     /* Thread index, 0 .. NR_MULTI_CONN-1 */
  int argc;                     /* Command line parameters. */
  char **argv;
  int status;                   /* Return status. */
  unsigned requests;            /* Total number of requests made. */
  unsigned most_in_flight;      /* Most requests seen in flight. */
};

static void *start_thread (void *arg);

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  pthread_t threads[NR_MULTI_CONN];
  struct thread_status status[NR_MULTI_CONN];
  size_t i;
  int err;
  unsigned requests, most_in_flight, errors;

  srand (time (NULL));

  if (argc < 2 || argc > 3) {
    fprintf (stderr, "%s uri | socket | hostname port\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Connect first to check if the server supports writes and multi-conn. */
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

  exportsize = nbd_get_size (nbd);
  if (exportsize == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  else if (exportsize <= BUFFER_SIZE) {
    fprintf (stderr, "export too small, must be larger than %d\n", BUFFER_SIZE);
    exit (EXIT_FAILURE);
  }

  if (nbd_is_read_only (nbd) == 1) {
    fprintf (stderr, "%s: error: this NBD export is read-only\n", argv[0]);
    exit (EXIT_FAILURE);
  }

#if NR_MULTI_CONN > 1
  if (nbd_can_multi_conn (nbd) == 0) {
    fprintf (stderr, "%s: error: "
             "this NBD export does not support multi-conn\n", argv[0]);
    exit (EXIT_FAILURE);
  }
#endif

  nbd_close (nbd);

  /* Start the worker threads, one per connection. */
  for (i = 0; i < NR_MULTI_CONN; ++i) {
    status[i].i = i;
    status[i].argc = argc;
    status[i].argv = argv;
    status[i].status = 0;
    status[i].requests = 0;
    status[i].most_in_flight = 0;
    err = pthread_create (&threads[i], NULL, start_thread, &status[i]);
    if (err != 0) {
      errno = err;
      perror ("pthread_create");
      exit (EXIT_FAILURE);
    }
  }

  /* Wait for the threads to exit. */
  errors = 0;
  requests = 0;
  most_in_flight = 0;
  for (i = 0; i < NR_MULTI_CONN; ++i) {
    err = pthread_join (threads[i], NULL);
    if (err != 0) {
      errno = err;
      perror ("pthread_join");
      exit (EXIT_FAILURE);
    }
    if (status[i].status != 0) {
      fprintf (stderr, "thread %zu failed with status %d\n",
               i, status[i].status);
      errors++;
    }
    requests += status[i].requests;
    if (status[i].most_in_flight > most_in_flight)
      most_in_flight = status[i].most_in_flight;
  }

  /* Make sure the number of requests that were required matches what
   * we expect.
   */
  assert (requests == NR_MULTI_CONN * NR_CYCLES);

  printf ("most requests seen in flight = %u (per thread) "
          "vs MAX_IN_FLIGHT = %d\n",
          most_in_flight, MAX_IN_FLIGHT);

  exit (errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void *
start_thread (void *arg)
{
  struct nbd_handle *nbd;
  struct pollfd fds[1];
  struct thread_status *status = arg;
  char *buf;
  size_t i, j;
  uint64_t offset;
  int64_t cookie;
  int64_t cookies[MAX_IN_FLIGHT];
  size_t in_flight;        /* counts number of requests in flight */
  unsigned dir;
  int r, cmd;
  size_t size;

  assert (512 < BUFFER_SIZE);
  buf = malloc (BUFFER_SIZE);
  if (buf == NULL) {
    perror ("malloc");
    exit (EXIT_FAILURE);
  }

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (status->argc == 2) {
    if (strstr (status->argv[1], "://")) {
      if (nbd_connect_uri (nbd, status->argv[1]) == -1) {
        fprintf (stderr, "%s\n", nbd_get_error ());
        exit (EXIT_FAILURE);
      }
    }
    else if (nbd_connect_unix (nbd, status->argv[1]) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }
  else {
    if (nbd_connect_tcp (nbd, status->argv[1], status->argv[2]) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  for (i = 0; i < BUFFER_SIZE; ++i)
    buf[i] = rand ();

  /* Issue commands. */
  in_flight = 0;
  i = NR_CYCLES;
  while (i > 0 || in_flight > 0) {
    if (nbd_aio_is_dead (nbd) || nbd_aio_is_closed (nbd)) {
      fprintf (stderr, "thread %zu: connection is dead or closed\n",
               status->i);
      goto error;
    }

    /* If we want to issue another request, do so.  Note that we reuse
     * the same buffer for multiple in-flight requests.  It doesn't
     * matter here because we're just trying to write random stuff,
     * but that would be Very Bad in a real application.
     * Simulate a mix of large and small requests.
     */
    while (i > 0 && in_flight < MAX_IN_FLIGHT) {
      size = (rand () & 1) ? BUFFER_SIZE : 512;
      offset = rand () % (exportsize - size);
      cmd = rand () & 1;
      if (cmd == 0)
        cookie = nbd_aio_pwrite (nbd, buf, size, offset,
                                 NBD_NULL_COMPLETION, 0);
      else
        cookie = nbd_aio_pread (nbd, buf, size, offset,
                                NBD_NULL_COMPLETION, 0);
      if (cookie == -1) {
        fprintf (stderr, "%s\n", nbd_get_error ());
        goto error;
      }
      cookies[in_flight] = cookie;
      i--;
      in_flight++;
      if (in_flight > status->most_in_flight)
        status->most_in_flight = in_flight;
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
    for (j = 0; j < in_flight; ++j) {
      r = nbd_aio_command_completed (nbd, cookies[j]);
      if (r == -1) {
        fprintf (stderr, "%s\n", nbd_get_error ());
        goto error;
      }
      if (r) {
        memmove (&cookies[j], &cookies[j+1],
                 sizeof (cookies[0]) * (in_flight - j - 1));
        j--;
        in_flight--;
        status->requests++;
      }
    }
  }

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);

  printf ("thread %zu: finished OK\n", status->i);

  free (buf);
  status->status = 0;
  pthread_exit (status);

 error:
  free (buf);
  fprintf (stderr, "thread %zu: failed\n", status->i);
  status->status = -1;
  pthread_exit (status);
}
