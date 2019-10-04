/* This example shows how to use qemu-nbd
 * to open a local qcow2 file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libnbd.h>

int
main (int argc, char *argv[])
{
  const char *filename;
  struct nbd_handle *nbd;
  char buf[512];
  FILE *fp;

  if (argc != 2) {
    fprintf (stderr, "open-qcow2 file.qcow2\n");
    exit (EXIT_FAILURE);
  }
  filename = argv[1];

  /* Create the libnbd handle. */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Run qemu-nbd as a subprocess using
   * systemd socket activation.
   */
  char *args[] = {
    "qemu-nbd", "-f", "qcow2",
    (char *) filename,
    NULL
  };
  if (nbd_connect_systemd_socket_activation (nbd,
                                             args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Read the first sector and print it. */
  if (nbd_pread (nbd, buf, sizeof buf, 0, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  fp = popen ("hexdump -C", "w");
  if (fp == NULL) {
    perror ("popen: hexdump");
    exit (EXIT_FAILURE);
  }
  fwrite (buf, sizeof buf, 1, fp);
  pclose (fp);

  /* Close the libnbd handle. */
  nbd_close (nbd);

  exit (EXIT_SUCCESS);
}
