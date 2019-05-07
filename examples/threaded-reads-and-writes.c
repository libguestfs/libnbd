/* This example can be copied, used and modified for any purpose
 * without restrictions.
 *
 * Example usage with nbdkit:
 *
 * nbdkit -U - memory 1M --run './simple-reads-and-writes $unixsocket'
 *
 * This will read and write randomly over the first megabyte of the
 * plugin using multi-conn, multiple threads and multiple requests in
 * flight on each thread.
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

#define SIZE (1024*1024)

/* Number of simultaneous connections to the NBD server.  This is also
 * the number of threads, because each thread manages one connection.
 * Note that some servers only support a limited number of
 * simultaneous connections, and/or have a configurable thread pool
 * internally, and if you exceed those limits then something will
 * break.
 */
#define NR_MULTI_CONN 4

/* Number of commands that can be "in flight" at the same time on each
 * connection.  (Therefore the total number of requests in flight may
 * be up to NR_MULTI_CONN * MAX_IN_FLIGHT).  Some servers do not
 * support multiple requests in flight and may deadlock or even crash
 * if this is larger than 1, but common NBD servers should be OK.
 */
#define MAX_IN_FLIGHT 4

/* Number of commands we issue (per thread). */
#define NR_CYCLES 10000

/* The single NBD handle.  This contains NR_MULTI_CONN connections. */
static struct nbd_handle *nbd;

struct thread_status {
  size_t i;                     /* Thread index, 0 .. NR_MULTI_CONN-1 */
  int status;                   /* Return status. */
  unsigned requests;            /* Total number of requests made. */
  unsigned most_in_flight;      /* Most requests seen in flight. */
};

static void *start_thread (void *arg);

int
main (int argc, char *argv[])
{
  pthread_t threads[NR_MULTI_CONN];
  struct thread_status status[NR_MULTI_CONN];
  size_t i;
  int err;
  unsigned requests, most_in_flight, errors;

  srand (time (NULL));

  if (argc != 2) {
    fprintf (stderr, "%s socket\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  nbd = nbd_create ();
  if (nbd == NULL) {
    perror ("nbd_create");
    exit (EXIT_FAILURE);
  }
  if (nbd_set_multi_conn (nbd, NR_MULTI_CONN) == -1) {
    /* XXX PRINT ERROR */
    exit (EXIT_FAILURE);
  }

  /* Connect all connections synchronously as this is simpler. */
  if (nbd_connect_unix (nbd, argv[1]) == -1) {
    /* XXX PRINT ERROR */
    exit (EXIT_FAILURE);
  }

  /* Start the worker threads, one per connection. */
  for (i = 0; i < NR_MULTI_CONN; ++i) {
    status[i].i = i;
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

  if (nbd_shutdown (nbd) == -1) {
    /* XXX PRINT ERROR */
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);

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
  struct pollfd fds[1];
  struct thread_status *status = arg;
  struct nbd_connection *conn;
  char buf[512];
  size_t i;
  uint64_t offset, handle;
  uint64_t handles[MAX_IN_FLIGHT];
  size_t in_flight;        /* counts number of requests in flight */
  int dir, r, cmd;
  bool want_to_send;

  /* The single thread "owns" the connection. */
  conn = nbd_get_connection (nbd, status->i);

  for (i = 0; i < sizeof buf; ++i)
    buf[i] = rand ();

  /* Issue commands. */
  in_flight = 0;
  i = NR_CYCLES;
  while (i > 0 || in_flight > 0) {
    if (nbd_aio_is_dead (conn) || nbd_aio_is_closed (conn)) {
      fprintf (stderr, "thread %zu: connection is dead or closed\n",
               status->i);
      goto error;
    }

    /* Do we want to send another request and there's room to issue it? */
    want_to_send = i > 0 && in_flight < MAX_IN_FLIGHT;

    fds[0].fd = nbd_aio_get_fd (conn);
    fds[0].events = want_to_send ? POLLOUT : 0;
    fds[0].revents = 0;
    dir = nbd_aio_get_direction (conn);
    if ((dir & LIBNBD_AIO_DIRECTION_READ) != 0)
      fds[0].events |= POLLIN;
    if ((dir & LIBNBD_AIO_DIRECTION_WRITE) != 0)
      fds[0].events |= POLLOUT;

    if (poll (fds, 1, -1) == -1) {
      perror ("poll");
      goto error;
    }

    if ((fds[0].revents & POLLIN) != 0)
      nbd_aio_notify_read (conn);
    else if ((fds[0].revents & POLLOUT) != 0)
      nbd_aio_notify_write (conn);

    /* If we can issue another request, do so.  Note that we reuse the
     * same buffer for multiple in-flight requests.  It doesn't matter
     * here because we're just trying to write random stuff, but that
     * would be Very Bad in a real application.
     */
    if (want_to_send && (fds[0].revents & POLLOUT) != 0 &&
        nbd_aio_is_ready (conn)) {
      offset = rand () % (SIZE - sizeof buf);
      cmd = rand () & 1;
      if (cmd == 0)
        handle = nbd_aio_pwrite (conn, buf, sizeof buf, offset, 0);
      else
        handle = nbd_aio_pread (conn, buf, sizeof buf, offset);
      if (handle == -1) {
        /* XXX PRINT ERROR */
        goto error;
      }
      handles[in_flight] = handle;
      i--;
      in_flight++;
      if (in_flight > status->most_in_flight)
        status->most_in_flight = in_flight;
    }

    /* If a command is ready to retire, retire it. */
    if (in_flight > 0) {
      r = nbd_aio_command_completed (conn, handles[0]);
      if (r == -1) {
        /* XXX PRINT ERROR */
        goto error;
      }
      if (r) {
        in_flight--;
        memmove (&handles[0], &handles[1],
                 sizeof (handles[0]) * (MAX_IN_FLIGHT-1));
        status->requests++;
      }
    }
  }

  status->status = 0;
  pthread_exit (status);

 error:
  status->status = -1;
  pthread_exit (status);
}
