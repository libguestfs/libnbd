/* This example shows how to connect to an NBD server
 * and fetch and print the first sector (usually the
 * boot sector or partition table or filesystem
 * superblock).
 *
 * You can test it with nbdkit like this:
 *
 * nbdkit -U - floppy . \
 *   --run './fetch-first-sector $unixsocket'
 *
 * The nbdkit floppy plugin creates an MBR disk so the
 * first sector is the partition table.
 */

#include <stdio.h>
#include <stdlib.h>

#include <libnbd.h>

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  char buf[512];
  FILE *pp;

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

  /* Read the first sector synchronously. */
  if (nbd_pread (nbd, buf, sizeof buf, 0, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Close the libnbd handle. */
  nbd_close (nbd);

  /* Print the first sector. */
  pp = popen ("hexdump -C", "w");
  if (pp == NULL) {
    perror ("popen: hexdump");
    exit (EXIT_FAILURE);
  }
  fwrite (buf, sizeof buf, 1, pp);
  pclose (pp);

  exit (EXIT_SUCCESS);
}
