/* This example shows how to do synchronous reads
 * and writes randomly over the first megabyte of an
 * NBD server.  Note this will destroy any existing
 * content on the NBD server.
 *
 * To test it with nbdkit and a RAM disk:
 *
 * nbdkit -U - memory 1M \
 *     --run './simple-reads-and-writes $unixsocket'
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <assert.h>
#include <time.h>

#include <libnbd.h>

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  char buf[512];
  size_t i;
  int64_t exportsize;
  uint64_t offset;

  srand (time (NULL));

  if (argc != 2) {
    fprintf (stderr, "%s socket\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  /* Create the libnbd handle. */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Connect to the NBD server over a
   * Unix domain socket.
   */
  if (nbd_connect_unix (nbd, argv[1]) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Get the size of the disk and check
   * it's large enough.
   */
  exportsize = nbd_get_size (nbd);
  if (exportsize == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  assert (exportsize >= sizeof buf);

  /* Check that the server is writable. */
  if (nbd_is_read_only (nbd) == 1) {
    fprintf (stderr, "%s: "
             "error: this NBD export is read-only\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }

  for (i = 0; i < sizeof buf; ++i)
    buf[i] = rand ();

  /* 1000 writes. */
  for (i = 0; i < 1000; ++i) {
    offset = rand () % (exportsize - sizeof buf);

    if (nbd_pwrite (nbd, buf, sizeof buf,
                    offset, 0) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  /* 1000 reads and writes. */
  for (i = 0; i < 1000; ++i) {
    offset = rand () % (exportsize - sizeof buf);
    if (nbd_pread (nbd, buf, sizeof buf,
                   offset, 0) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }

    offset = rand () % (exportsize - sizeof buf);
    if (nbd_pwrite (nbd, buf, sizeof buf,
                    offset, 0) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  /* Sends a graceful shutdown to the server. */
  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);

  exit (EXIT_SUCCESS);
}
