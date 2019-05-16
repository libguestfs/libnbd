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

/* Test synchronous parallel high level API requests.
 *
 * NB: These don't actually run in parallel because of the handle lock
 * held by nbd_poll (which cannot be removed).  However it's still a
 * nice test of data integrity and locking.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <pthread.h>

#include <libnbd.h>

/* Size of the RAM disk in bytes. */
static int64_t exportsize;

/* We keep a shadow of the RAM disk so we can check integrity of the data. */
static char *ramdisk;

/* How long (seconds) that the test will run for. */
#define RUN_TIME 10

/* Number of threads and connections.  These don't need to be related
 * because there is no real parallelism going on here.
 */
#define NR_THREADS 8
#define NR_MULTI_CONN 4

/* The single NBD handle.  This contains NR_MULTI_CONN connections. */
static struct nbd_handle *nbd;

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

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_set_multi_conn (nbd, NR_MULTI_CONN) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

#ifdef TLS
  /* Require TLS on the handle and fail if not available or if the
   * handshake fails.
   */
  if (nbd_set_tls (nbd, 2) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_set_tls_psk_file (nbd, "keys.psk") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
#endif

  /* Connect to nbdkit. */
  if (nbd_connect_unix (nbd, argv[1]) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Get the current time and the end time. */
  time (&t);
  t += RUN_TIME;

  srand (t + getpid ());

  exportsize = nbd_get_size (nbd);
  if (exportsize == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Initialize the RAM disk with the initial data from
   * nbdkit-pattern-filter.
   */
  ramdisk = malloc (exportsize);
  if (ramdisk == NULL) {
    perror ("calloc");
    exit (EXIT_FAILURE);
  }
  for (i = 0; i < exportsize; i += 8) {
    uint64_t d = htobe64 (i);
    memcpy (&ramdisk[i], &d, sizeof d);
  }

  if (nbd_read_only (nbd) == 1) {
    fprintf (stderr, "%s: error: this NBD export is read-only\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  if (nbd_can_multi_conn (nbd) == 0) {
    fprintf (stderr, "%s: error: "
             "this NBD export does not support multi-conn\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  /* Start the worker threads. */
  for (i = 0; i < NR_THREADS; ++i) {
    status[i].i = i;
    status[i].end_time = t;
    status[i].offset = i * exportsize / NR_THREADS;
    status[i].length = exportsize / NR_THREADS;
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

  if (nbd_shutdown (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);

  /* Print some stats. */
  printf ("TLS: %s\n",
#ifdef TLS
          "enabled"
#else
          "disabled"
#endif
          );
  printf ("multi-conn: %d\n", NR_MULTI_CONN);

  printf ("bytes sent: %" PRIu64 " (%g Mbytes/s)\n",
          bytes_sent, (double) bytes_sent / RUN_TIME / 1000000);
  printf ("bytes received: %" PRIu64 " (%g Mbytes/s)\n",
          bytes_received, (double) bytes_received / RUN_TIME / 1000000);

  printf ("I/O requests: %u (%g IOPS)\n",
          requests, (double) requests / RUN_TIME);

  exit (errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

static void *
start_thread (void *arg)
{
  struct thread_status *status = arg;
  char buf[16384];
  int cmd;
  uint64_t offset;
  time_t t;

  memset (buf, 0, sizeof buf);

  /* Issue commands. */
  while (1) {
    /* Run until the timer expires. */
    time (&t);
    if (t > status->end_time)
      break;

    /* Issue a synchronous read or write command. */
    offset = status->offset + (rand () % (status->length - sizeof buf));
    cmd = rand () & 1;
    if (cmd == 0) {
      if (nbd_pwrite (nbd, buf, sizeof buf, offset, 0) == -1) {
        fprintf (stderr, "%s\n", nbd_get_error ());
        goto error;
      }
      status->bytes_sent += sizeof buf;
      memcpy (&ramdisk[offset], buf, sizeof buf);
    }
    else {
      if (nbd_pread (nbd, buf, sizeof buf, offset) == -1) {
        fprintf (stderr, "%s\n", nbd_get_error ());
        goto error;
      }
      status->bytes_received += sizeof buf;
      if (memcmp (&ramdisk[offset], buf, sizeof buf) != 0) {
        fprintf (stderr, "thread %zu: DATA INTEGRITY ERROR!\n", status->i);
        goto error;
      }
    }
    status->requests++;
  }

  printf ("thread %zu: finished OK\n", status->i);

  status->status = 0;
  pthread_exit (status);

 error:
  fprintf (stderr, "thread %zu: failed\n", status->i);
  status->status = -1;
  pthread_exit (status);
}
