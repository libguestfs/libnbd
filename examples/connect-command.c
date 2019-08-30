/* This example shows how to run an NBD server
 * (nbdkit) as a subprocess of libnbd.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libnbd.h>

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  char wbuf[512], rbuf[512];
  size_t i;

  /* Create the libnbd handle. */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Run nbdkit as a subprocess. */
  char *args[] = {
    "nbdkit",

    /* You must use ‘-s’ (which tells nbdkit to serve
     * a single connection on stdin/stdout).
     */
    "-s",

    /* It is recommended to use ‘--exit-with-parent’
     * to ensure nbdkit is always cleaned up even
     * if the main program crashes.
     */
    "--exit-with-parent",

    /* Use this to enable nbdkit debugging. */
    "-v",

    /* The nbdkit plugin name - this is a RAM disk. */
    "memory", "size=1M",

    /* Use NULL to terminate the arg list. */
    NULL
  };
  if (nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Write some random data to the first sector. */
  for (i = 0; i < sizeof wbuf; ++i)
    wbuf[i] = i % 13;
  if (nbd_pwrite (nbd, wbuf, sizeof wbuf, 0, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Read the first sector back. */
  if (nbd_pread (nbd, rbuf, sizeof rbuf, 0, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Close the libnbd handle. */
  nbd_close (nbd);

  /* What was read must be exactly the same as what
   * was written.
   */
  if (memcmp (rbuf, wbuf, sizeof rbuf) != 0) {
    fprintf (stderr, "FAILED: "
             "read data did not match written data\n");
    exit (EXIT_FAILURE);
  }

  exit (EXIT_SUCCESS);
}
