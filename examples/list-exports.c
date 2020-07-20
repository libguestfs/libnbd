/* This example shows how to list NBD exports.
 *
 * To test this with qemu-nbd:
 *   $ qemu-nbd -x "hello" -t -k /tmp/sock disk.img
 *   $ ./run examples/list-exports /tmp/sock
 *   [0] hello
 *   Which export to connect to? 0
 *   Connecting to hello ...
 *   /tmp/sock: hello: size = 2048 bytes
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>

#include <libnbd.h>

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd, *nbd2;
  int r, i;
  char *name;
  int64_t size;

  if (argc != 2) {
    fprintf (stderr, "%s socket\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  /* Create the libnbd handle for querying exports. */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Set the list exports mode in the handle. */
  nbd_set_list_exports (nbd, true);

  /* Connect to the NBD server over a
   * Unix domain socket.  A side effect of
   * connecting is to list the exports.
   * This operation can fail normally, so
   * we need to check the return value and
   * error code.
   */
  r = nbd_connect_unix (nbd, argv[1]);
  if (r == -1 && nbd_get_errno () == ENOTSUP) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_get_nr_list_exports (nbd) == 0) {
    fprintf (stderr, "Server does not support "
             "listing exports.\n");
    exit (EXIT_FAILURE);
  }

  /* Display the list of exports. */
  for (i = 0;
       i < nbd_get_nr_list_exports (nbd);
       i++) {
    name = nbd_get_list_export_name (nbd, i);
    printf ("[%d] %s\n", i, name);
    free (name);
  }
  printf ("Which export to connect to? ");
  if (scanf ("%d", &i) != 1) exit (EXIT_FAILURE);
  name = nbd_get_list_export_name (nbd, i);
  printf ("Connecting to %s ...\n", name);
  nbd_close (nbd);

  /* Connect again to the chosen export. */
  nbd2 = nbd_create ();
  if (nbd2 == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_set_export_name (nbd2, name) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_connect_unix (nbd2, argv[1]) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Read the size in bytes and print it. */
  size = nbd_get_size (nbd2);
  if (size == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  printf ("%s: %s: size = %" PRIi64 " bytes\n",
          argv[1], name, size);

  /* Close the libnbd handle. */
  nbd_close (nbd2);

  free (name);

  exit (EXIT_SUCCESS);
}
