/* This example shows how to list NBD exports.
 *
 * To test this with qemu-nbd:
 *   $ qemu-nbd -x "hello" -t -k /tmp/sock disk.img
 *   $ ./run examples/list-exports /tmp/sock
 *   [0] hello
 *   Which export to connect to (-1 to quit)? 0
 *   Connecting to hello ...
 *   /tmp/sock: hello: size = 2048 bytes
 *
 * To test this with nbdkit (requires 1.22):
 *   $ nbdkit -U /tmp/sock sh - <<\EOF
 *   case $1 in
 *     list_exports) echo NAMES; echo foo; echo foobar ;;
 *     open) echo "$3" ;;
 *     get_size) echo "$2" | wc -c ;;
 *     pread) echo "$2" | dd bs=1 skip=$4 count=$3 ;;
 *     *) exit 2 ;;
 *   esac
 *   EOF
 *   $ ./run examples/list-exports /tmp/sock
 *   [0] foo
 *   [1] foobar
 *   Which export to connect to (-1 to quit)? 1
 *   Connecting to foobar ...
 *   /tmp/sock: foobar: size = 7 bytes
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include <libnbd.h>

struct export_list {
  int i;
  char **names;
};

/* Callback function for nbd_opt_list */
static int
list_one (void *opaque, const char *name,
          const char *description)
{
  struct export_list *l = opaque;
  char **names;

  printf ("[%d] %s\n", l->i, name);
  if (*description)
    printf("  (%s)\n", description);
  names = realloc (l->names,
                   (l->i + 1) * sizeof *names);
  if (!names) {
    perror ("realloc");
    exit (EXIT_FAILURE);
  }
  names[l->i] = strdup (name);
  if (!names[l->i]) {
    perror ("strdup");
    exit (EXIT_FAILURE);
  }
  l->names = names;
  l->i++;
  return 0;
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int i;
  const char *name;
  int64_t size;
  struct export_list list = { 0 };

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

  /* Set opt mode. */
  nbd_set_opt_mode (nbd, true);

  /* Connect to the NBD server over a
   * Unix domain socket.  If we did not
   * end up in option mode, then a
   * listing is not possible.
   */
  if (nbd_connect_unix (nbd, argv[1]) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (!nbd_aio_is_negotiating (nbd)) {
    fprintf (stderr, "Server does not support "
             "listing exports.\n");
    exit (EXIT_FAILURE);
  }

  /* Print the export list. */
  if (nbd_opt_list (nbd,
                    (nbd_list_callback) {
                      .callback = list_one,
                      .user_data = &list, }) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Display the list of exports. */
  printf ("Which export to connect to? ");
  if (scanf ("%d", &i) != 1) exit (EXIT_FAILURE);
  if (i == -1) {
    if (nbd_opt_abort (nbd) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    nbd_close (nbd);
    exit (EXIT_SUCCESS);
  }
  if (i < 0 || i >= list.i) {
    fprintf (stderr, "index %d out of range", i);
    exit (EXIT_FAILURE);
  }
  name = list.names[i];
  printf ("Connecting to %s ...\n", name);

  /* Resume connecting to the chosen export. */
  if (nbd_set_export_name (nbd, name) == -1 ||
      nbd_opt_go (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (!nbd_aio_is_ready (nbd)) {
    fprintf (stderr, "server closed early\n");
    exit (EXIT_FAILURE);
  }

  /* Read the size in bytes and print it. */
  size = nbd_get_size (nbd);
  if (size == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  printf ("%s: %s: size = %" PRIi64 " bytes\n",
          argv[1], name, size);

  /* Close the libnbd handle. */
  nbd_close (nbd);

  for (i = 0; i < list.i; i++)
    free (list.names[i]);
  free (list.names);

  exit (EXIT_SUCCESS);
}
