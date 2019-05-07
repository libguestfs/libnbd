/* This example can be copied, used and modified for any purpose
 * without restrictions.
 *
 * Example usage with nbdkit:
 *
 * nbdkit -U - pattern 1M --run './simple-fetch-first-sector $unixsocket sector'
 *
 * This will read the first sector (512 bytes) from the NBD server and
 * write it to the local file 'sector'.  nbdkit-pattern-plugin
 * contains a simple regular which you can use to check the data read
 * is correct.
 */

#include <stdio.h>
#include <stdlib.h>

#include <libnbd.h>

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  char buf[512];
  FILE *fp;

  if (argc != 3) {
    fprintf (stderr, "%s socket output\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  nbd = nbd_create ();
  if (nbd == NULL) {
    perror ("nbd_create");
    exit (EXIT_FAILURE);
  }
  if (nbd_connect_unix (nbd, argv[1]) == -1) {
    /* XXX PRINT ERROR */
    exit (EXIT_FAILURE);
  }

  if (nbd_pread (nbd, buf, sizeof buf, 0) == -1) {
    /* XXX PRINT ERROR */
    exit (EXIT_FAILURE);
  }

  if (nbd_shutdown (nbd) == -1) {
    /* XXX PRINT ERROR */
    exit (EXIT_FAILURE);
  }

  fp = fopen (argv[2], "w");
  if (fp == NULL) {
    perror (argv[2]);
    exit (EXIT_FAILURE);
  }
  fwrite (buf, sizeof buf, 1, fp);
  fclose (fp);

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
