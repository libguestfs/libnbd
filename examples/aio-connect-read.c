/* This example shows how to use the AIO (asynchronous) low
 * level API to connect to a server and read the disk.
 *
 * Here are a few ways to try this example:
 *
 * nbdkit -U - linuxdisk . \
 *   --run './aio-connect-read $unixsocket'
 *
 * nbdkit -U - floppy . \
 *   --run './aio-connect-read $unixsocket'
 *
 * nbdkit -U - pattern size=1M \
 *   --run './aio-connect-read $unixsocket'
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <assert.h>

#include <libnbd.h>

#define NR_SECTORS 32
#define SECTOR_SIZE 512

struct data {
  uint64_t offset;
  char sector[SECTOR_SIZE];
};

static int
hexdump (void *user_data, int *error)
{
  struct data *data = user_data;
  FILE *pp;

  if (*error) {
    errno = *error;
    perror ("failed to read");
    exit (EXIT_FAILURE);
  }

  printf ("sector at offset 0x%" PRIx64 ":\n",
          data->offset);
  pp = popen ("hexdump -C", "w");
  if (pp == NULL) {
    perror ("popen: hexdump");
    exit (EXIT_FAILURE);
  }
  fwrite (data->sector, SECTOR_SIZE, 1, pp);
  pclose (pp);
  printf ("\n");

  /* Returning 1 from the callback automatically retires
   * the command.
   */
  return 1;
}

static struct data data[NR_SECTORS];

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  size_t i;

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

  /* Connect to the NBD server over a Unix domain socket.
   * This only starts the connection.
   */
  if (nbd_aio_connect_unix (nbd, argv[1]) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Wait for the connection to complete.  The use of
   * nbd_poll here is only as an example.  You could also
   * integrate this with poll(2), glib or another main
   * loop.  Read libnbd(3) and the source file lib/poll.c.
   */
  while (!nbd_aio_is_ready (nbd)) {
    if (nbd_poll (nbd, -1) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  assert (nbd_get_size (nbd) >= NR_SECTORS * SECTOR_SIZE);

  /* Issue read commands for the first NR sectors. */
  for (i = 0; i < NR_SECTORS; ++i) {
    data[i].offset = i * SECTOR_SIZE;

    /* The callback (hexdump) is called when the command
     * completes.  The buffer must continue to exist while
     * the command is running.
     */
    if (nbd_aio_pread (nbd, data[i].sector, SECTOR_SIZE,
                       data[i].offset,
                       (nbd_completion_callback) {
                         .callback = hexdump,
                         .user_data = &data[i],
                       }, 0) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  /* Run the main loop until all the commands have
   * completed and retired.  Again the use of nbd_poll
   * here is only as an example.
   */
  while (nbd_aio_in_flight (nbd) > 0) {
    if (nbd_poll (nbd, -1) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  /* Close the libnbd handle. */
  nbd_close (nbd);

  exit (EXIT_SUCCESS);
}
