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

/* Test synchronous parallel high level API requests on different
 * handles.  There should be no shared state between the handles so
 * this should run at full speed (albeit with us only having a single
 * command per thread in flight).
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <pthread.h>

#include <libnbd.h>

#include "byte-swapping.h"

/* We keep a shadow of the RAM disk so we can check integrity of the data. */
static char *ramdisk;

/* This is also defined in synch-parallel.sh and checked here. */
#define EXPORTSIZE (8*1024*1024)

/* How long (seconds) that the test will run for. */
#define RUN_TIME 10

/* Number of threads. */
#define NR_THREADS 8

/* Unix socket. */
static const char *unixsocket;

struct thread_status {
  size_t i;                     /* Thread index, 0 .. NR_THREADS-1 */
  time_t end_time;              /* Threads run until this end time. */
  uint64_t offset, length;      /* Area assigned to this thread. */
  int status;                   /* Return status. */
  unsigned requests;            /* Total number of requests made. */
  uint64_t bytes_sent, bytes_received; /* Bytes sent and received by thread. */
};

static void *start_thread (void *arg);

int
main (int argc, char *argv[])
{
  pthread_t threads[NR_THREADS];
  struct thread_status status[NR_THREADS];
  size_t i;
  time_t t;
  int err;
  unsigned requests, errors;
  uint64_t bytes_sent, bytes_received;

  if (argc != 2) {
    fprintf (stderr, "%s socket\n", argv[0]);
    exit (EXIT_FAILURE);
  }
  unixsocket = argv[1];

  /* Get the current time and the end time. */
  time (&t);
  t += RUN_TIME;

  srand (t + getpid ());

  /* Initialize the RAM disk with the initial data from
   * nbdkit-pattern-filter.
   */
  ramdisk = malloc (EXPORTSIZE);
  if (ramdisk == NULL) {
    perror ("calloc");
    exit (EXIT_FAILURE);
  }
  for (i = 0; i < EXPORTSIZE; i += 8) {
    uint64_t d = htobe64 (i);
    memcpy (&ramdisk[i], &d, sizeof d);
  }

  /* Start the worker threads. */
  for (i = 0; i < NR_THREADS; ++i) {
    status[i].i = i;
    status[i].end_time = t;
    status[i].offset = i * EXPORTSIZE / NR_THREADS;
    status[i].length = EXPORTSIZE / NR_THREADS;
    status[i].status = 0;
    status[i].requests = 0;
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
  bytes_sent = bytes_received = 0;
  for (i = 0; i < NR_THREADS; ++i) {
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
    bytes_sent += status[i].bytes_sent;
    bytes_received += status[i].bytes_received;
  }

  free (ramdisk);

  /* Print some stats. */
  printf ("TLS: %s\n",
#ifdef TLS
          "enabled"
#else
          "disabled"
#endif
          );

  printf ("bytes sent: %" PRIu64 " (%g Mbytes/s)\n",
          bytes_sent, (double) bytes_sent / RUN_TIME / 1000000);
  printf ("bytes received: %" PRIu64 " (%g Mbytes/s)\n",
          bytes_received, (double) bytes_received / RUN_TIME / 1000000);

  printf ("I/O requests: %u (%g IOPS)\n",
          requests, (double) requests / RUN_TIME);

  exit (errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

#define BUFFER_SIZE 16384

static void *
start_thread (void *arg)
{
  struct thread_status *status = arg;
  struct nbd_handle *nbd;
  char *buf;
  int cmd;
  uint64_t offset;
  time_t t;

  buf = calloc (BUFFER_SIZE, 1);
  if (buf == NULL) {
    perror ("calloc");
    exit (EXIT_FAILURE);
  }

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
  if (nbd_connect_unix (nbd, unixsocket) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  assert (nbd_get_size (nbd) == EXPORTSIZE);
  assert (nbd_can_multi_conn (nbd) > 0);
  assert (nbd_is_read_only (nbd) == 0);

  /* Issue commands. */
  while (1) {
    /* Run until the timer expires. */
    time (&t);
    if (t > status->end_time)
      break;

    /* Issue a synchronous read or write command. */
    offset = status->offset + (rand () % (status->length - BUFFER_SIZE));
    cmd = rand () & 1;
    if (cmd == 0) {
      if (nbd_pwrite (nbd, buf, BUFFER_SIZE, offset, 0) == -1) {
        fprintf (stderr, "%s\n", nbd_get_error ());
        goto error;
      }
      status->bytes_sent += BUFFER_SIZE;
      memcpy (&ramdisk[offset], buf, BUFFER_SIZE);
    }
    else {
      if (nbd_pread (nbd, buf, BUFFER_SIZE, offset, 0) == -1) {
        fprintf (stderr, "%s\n", nbd_get_error ());
        goto error;
      }
      status->bytes_received += BUFFER_SIZE;
      if (memcmp (&ramdisk[offset], buf, BUFFER_SIZE) != 0) {
        fprintf (stderr, "thread %zu: DATA INTEGRITY ERROR!\n", status->i);
        goto error;
      }
    }
    status->requests++;
  }

  printf ("thread %zu: finished OK\n", status->i);

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    goto error;
  }

  nbd_close (nbd);

  free (buf);

  status->status = 0;
  pthread_exit (status);

 error:
  free (buf);

  fprintf (stderr, "thread %zu: failed\n", status->i);
  status->status = -1;
  pthread_exit (status);
}
