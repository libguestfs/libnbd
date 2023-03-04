/* NBD client library in userspace
 * Copyright Red Hat
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

/* Test an asynchronous random load. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <poll.h>
#include <assert.h>

#include <pthread.h>

#include <libnbd.h>

/* This is also defined in aio-parallel-load.sh and checked here. */
#define EXPORTSIZE (64*1024*1024)

/* How long (seconds) that the test will run for. */
#define RUN_TIME 10

/* Number of threads and connections. */
#define NR_MULTI_CONN 8

/* Number of commands in flight per connection. */
#define MAX_IN_FLIGHT 64

/* Unix socket or uri. */
static const char *connection;

struct thread_status {
  size_t i;                     /* Thread index, 0 .. NR_MULTI_CONN-1 */
  time_t end_time;              /* Threads run until this end time. */
  size_t buf_size;              /* Size of requests. */
  int status;                   /* Return status. */
  unsigned requests;            /* Total number of requests made. */
  unsigned most_in_flight;      /* Most requests seen in flight. */
  uint64_t bytes_sent, bytes_received; /* Bytes sent and received by thread. */
};

/* There's a single shared buffer for all requests, which is not
 * realistic, but this test is not about data integrity but the
 * protocol handling under load.
 */
static char *buf;

static void *start_thread (void *arg);

int
main (int argc, char *argv[])
{
  pthread_t threads[NR_MULTI_CONN];
  struct thread_status status[NR_MULTI_CONN];
  size_t i;
  time_t t;
  int err;
  unsigned requests, most_in_flight, errors;
  uint64_t bytes_sent, bytes_received;
  size_t buf_size;

  if (argc < 2 || argc > 3) {
    fprintf (stderr, "%s socket [buf-size]\n", argv[0]);
    exit (EXIT_FAILURE);
  }
  connection = argv[1];
  if (argc == 3) {
    char *end;

    errno = 0;
    buf_size = strtoul (argv[2], &end, 0);
    if (errno || argv[2] == end || buf_size == 0 || buf_size >= EXPORTSIZE) {
      fprintf (stderr, "invalid buf-size %s, must be positive integer < %d\n",
               argv[2], EXPORTSIZE);
      exit (EXIT_FAILURE);
    }
  }
  else
    buf_size = 64 * 1024;

  buf = malloc (buf_size);
  if (!buf) {
    perror ("malloc");
    exit (EXIT_FAILURE);
  }

  /* Get the current time and the end time. */
  time (&t);
  t += RUN_TIME;

  /* Initialize the buffer with random data. */
  srand (t + getpid ());
  for (i = 0; i < buf_size; ++i)
    buf[i] = rand ();

  /* Start the worker threads, one per connection. */
  for (i = 0; i < NR_MULTI_CONN; ++i) {
    status[i].i = i;
    status[i].end_time = t;
    status[i].buf_size = buf_size;
    status[i].status = 0;
    status[i].requests = 0;
    status[i].most_in_flight = 0;
    status[i].bytes_sent = status[i].bytes_received = 0;
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
  bytes_sent = bytes_received = 0;
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
    bytes_sent += status[i].bytes_sent;
    bytes_received += status[i].bytes_received;
  }

  /* Print some stats. */
  printf ("TLS: %s\n",
#ifdef TLS
          "enabled"
#else
          "disabled"
#endif
          );
  printf ("multi-conn: %d\n", NR_MULTI_CONN);
  printf ("max in flight permitted (per connection): %d\n", MAX_IN_FLIGHT);

  printf ("bytes sent: %" PRIu64 " (%g Mbytes/s)\n",
          bytes_sent, (double) bytes_sent / RUN_TIME / 1000000);
  printf ("bytes received: %" PRIu64 " (%g Mbytes/s)\n",
          bytes_received, (double) bytes_received / RUN_TIME / 1000000);

  printf ("I/O requests: %u (%g IOPS)\n",
          requests, (double) requests / RUN_TIME);

  printf ("max requests in flight: %u\n",
          most_in_flight);

  exit (errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void *
start_thread (void *arg)
{
  struct pollfd fds[1];
  struct thread_status *status = arg;
  size_t buf_size = status->buf_size;
  struct nbd_handle *nbd;
  size_t i;
  uint64_t offset;
  int64_t cookie;
  int64_t cookies[MAX_IN_FLIGHT] = { 0 };
  unsigned dir;
  int r, cmd;
  time_t t;
  bool expired = false;

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

#ifdef TLS
  /* Require TLS on the handle and fail if not available or if the
   * handshake fails.
   */
  if (nbd_set_tls (nbd, LIBNBD_TLS_REQUIRE) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_set_tls_username (nbd, "alice") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_set_tls_psk_file (nbd, "keys.psk") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
#endif

  /* Connect to nbdkit. */
  if (strstr (connection, "://")) {
    if (nbd_connect_uri (nbd, connection) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  } else if (nbd_connect_unix (nbd, connection) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  assert (nbd_get_size (nbd) == EXPORTSIZE);
  assert (nbd_can_multi_conn (nbd) > 0);
  assert (nbd_is_read_only (nbd) == 0);

  /* Issue commands. */
  assert (nbd_aio_in_flight (nbd) == 0);
  while (!expired || nbd_aio_in_flight (nbd) > 0) {
    if (nbd_aio_is_dead (nbd) || nbd_aio_is_closed (nbd)) {
      fprintf (stderr, "thread %zu: connection is dead or closed\n",
               status->i);
      goto error;
    }

    /* Run until the timer expires. */
    time (&t);
    if (t > status->end_time) {
      expired = true;
      if (nbd_aio_in_flight (nbd) <= 0)
        break;
    }

    /* If we can issue another request, do so. */
    while (!expired && nbd_aio_in_flight (nbd) < MAX_IN_FLIGHT) {
      offset = rand () % (EXPORTSIZE - buf_size);
      cmd = rand () & 1;
      if (cmd == 0) {
        cookie = nbd_aio_pwrite (nbd, buf, buf_size, offset,
                                 NBD_NULL_COMPLETION, 0);
        status->bytes_sent += buf_size;
      }
      else {
        cookie = nbd_aio_pread (nbd, buf, buf_size, offset,
                                NBD_NULL_COMPLETION, 0);
        status->bytes_received += buf_size;
      }
      if (cookie == -1) {
        fprintf (stderr, "%s\n", nbd_get_error ());
        goto error;
      }
      for (i = 0; i < MAX_IN_FLIGHT; i++) {
        if (cookies[i] == 0) {
          cookies[i] = cookie;
          break;
        }
      }
      if (nbd_aio_in_flight (nbd)  > status->most_in_flight)
        status->most_in_flight = nbd_aio_in_flight (nbd);
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
    for (i = 0; i < MAX_IN_FLIGHT; ++i) {
      if (cookies[i] == 0)
        continue;
      r = nbd_aio_command_completed (nbd, cookies[i]);
      if (r == -1) {
        fprintf (stderr, "%s\n", nbd_get_error ());
        goto error;
      }
      if (r) {
        cookies[i] = 0;
        status->requests++;
      }
    }
  }

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    goto error;
  }

  nbd_close (nbd);

  printf ("thread %zu: finished OK\n", status->i);

  status->status = 0;
  pthread_exit (status);

 error:
  fprintf (stderr, "thread %zu: failed\n", status->i);
  status->status = -1;
  pthread_exit (status);
}
