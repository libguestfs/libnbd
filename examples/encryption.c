/* An example showing how to connect to a server which is
 * using TLS encryption.
 *
 * This requires nbdkit, and psktool from gnutls.
 *
 * Both libnbd and nbdkit support TLS-PSK which is a
 * simpler-to-deploy form of encryption.  (Of course
 * certificate-based encryption is also supported, but
 * it’s harder to make a self-contained example).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libnbd.h>

#define TMPDIR "/tmp/XXXXXX"
#define KEYS "keys.psk"
#define USERNAME "alice"

static char dir[] = TMPDIR;
static char keys[] = TMPDIR "/" KEYS;
static char cmd[] =
  "psktool -u " USERNAME " -p " TMPDIR "/" KEYS;

/* Remove the temporary keys file when the program
 * exits.
 */
static void
cleanup_keys (void)
{
  unlink (keys);
  rmdir (dir);
}

/* Create the temporary keys file to share with the
 * server.
 */
static void
create_keys (void)
{
  size_t i;

  if (mkdtemp (dir) == NULL) {
    perror ("mkdtemp");
    exit (EXIT_FAILURE);
  }
  i = strlen (cmd) - strlen (TMPDIR) - strlen (KEYS) - 1;
  memcpy (&cmd[i], dir, strlen (TMPDIR));
  memcpy (keys, dir, strlen (TMPDIR));

  if (system (cmd) != 0) {
    fprintf (stderr, "psktool command failed\n");
    exit (EXIT_FAILURE);
  }

  atexit (cleanup_keys);
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  char buf[512];

  create_keys ();

  /* Create the libnbd handle. */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Enable TLS in the client. */
  if (nbd_set_tls (nbd, LIBNBD_TLS_REQUIRE) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Enable TLS-PSK and pass the keys filename. */
  if (nbd_set_tls_psk_file (nbd, keys) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Set the local username for authentication. */
  if (nbd_set_tls_username (nbd, USERNAME) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Run nbdkit as a subprocess, enabling and requiring
   * TLS-PSK encryption.
   */
  char *args[] = {
    "nbdkit", "-s", "--exit-with-parent",
    "--tls", "require", "--tls-psk", keys,
    "pattern", "size=1M", NULL
  };
  if (nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Read the first sector. */
  if (nbd_pread (nbd, buf, sizeof buf, 0, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* TLS connections must be shut down. */
  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Close the libnbd handle. */
  nbd_close (nbd);

  exit (EXIT_SUCCESS);
}
