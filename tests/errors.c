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

/* Deliberately provoke some errors and check the error messages from
 * nbd_get_error etc look reasonable.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

#include <libnbd.h>

#define MAXSIZE (65 * 1024 * 1024) /* Oversize on purpose */

static char *progname;
static char buf[MAXSIZE];

static void
check (int experr, const char *prefix)
{
  const char *msg = nbd_get_error ();
  int errnum = nbd_get_errno ();

  printf ("error: \"%s\"\n", msg);
  printf ("errno: %d (%s)\n", errnum, strerror (errnum));
  if (strncmp (msg, prefix, strlen (prefix)) != 0) {
    fprintf (stderr, "%s: test failed: missing context prefix: %s\n",
             progname, msg);
    exit (EXIT_FAILURE);
  }
  if (errnum != experr) {
    fprintf (stderr, "%s: test failed: "
             "expected errno = %d (%s), but got %d\n",
             progname, experr, strerror (experr), errnum);
    exit (EXIT_FAILURE);
  }
}

static char script[] = "/tmp/libnbd-errors-scriptXXXXXX";
static char witness[] = "/tmp/libnbd-errors-witnessXXXXXX";
static int script_fd = -1, witness_fd = -1;

static void
cleanup (void)
{
  if (script_fd != -1) {
    if (script_fd >= 0)
      close (script_fd);
    unlink (script);
  }
  if (witness_fd >= 0) {
    close (witness_fd);
    unlink (witness);
  }
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  /* We will connect to a custom nbdkit sh plugin which always fails
   * on reads (with a precise spelling required for older nbdkit), and
   * which delays responding to writes until a witness file no longer
   * exists.
   */
  const char *cmd[] = { "nbdkit", "-s", "--exit-with-parent", "sh",
                        script, NULL };

  progname = argv[0];

  if (atexit (cleanup) != 0) {
    perror ("atexit");
    exit (EXIT_FAILURE);
  }
  if ((script_fd = mkstemp (script)) == -1) {
    perror ("mkstemp");
    exit (EXIT_FAILURE);
  }
  if ((witness_fd = mkstemp (witness)) == -1) {
    perror ("mkstemp");
    exit (EXIT_FAILURE);
  }

  if (dprintf (script_fd, "case $1 in\n"
               "  get_size) echo 128m || exit 1 ;;\n"
               "  thread_model) echo serialize_all_requests; exit 0 ;;\n"
               "  pread) printf 'ENOMEM ' >&2; exit 1 ;;\n"
               "  can_write) exit 0 ;;\n"
               "  pwrite)\n"
               "    while test -e %s; do sleep 1; done\n"
               "    exit 0;;\n"
               "  *) exit 2 ;;\n"
               "esac\n", witness) < 0) {
    perror ("dprintf");
    exit (EXIT_FAILURE);
  }
  if (fchmod (script_fd, 0700) == -1) {
    perror ("fchmod");
    exit (EXIT_FAILURE);
  }
  if (close (script_fd) == -1) {  /* Unlinked later during atexit */
    perror ("close");
    exit (EXIT_FAILURE);
  }
  script_fd = -2;

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Issue a connected command when not connected. */
  if (nbd_pread (nbd, NULL, 0, 0, 0) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_pread did not fail on non-connected handle\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (ENOTCONN, "nbd_pread: ");

  /* Request a name that is too long. */
  memset (buf, 'a', 4999);
  buf[4999] = '\0';
  if (nbd_set_export_name (nbd, buf) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_set_export_name did not reject large name\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (ENAMETOOLONG, "nbd_set_export_name: ");

  /* Poll while there is no fd. */
  if (nbd_aio_get_fd (nbd) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_aio_get_fd did not fail prior to connection\n",
             argv[0]);
  }
  check (EINVAL, "nbd_aio_get_fd: ");
  if (nbd_poll (nbd, 1000) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_poll did not fail prior to connection\n",
             argv[0]);
  }
  check (EINVAL, "nbd_poll: ");

  /* Connect to a working server, then try to connect again. */
  if (nbd_connect_command (nbd, (char **) cmd) == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_connect_command (nbd, (char **) cmd) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_connect_command did not reject repeat attempt\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (EINVAL, "nbd_connect_command: ");

  /* Try to notify that writes are ready when we aren't blocked on POLLOUT */
  if (nbd_aio_notify_write (nbd) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_aio_notify_write in wrong state did not fail\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (EINVAL, "nbd_aio_notify_write: ");

  /* Check for status of a bogus cookie */
  if (nbd_aio_command_completed (nbd, 0) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_aio_command_completed on bogus cookie did not fail\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (EINVAL, "nbd_aio_command_completed: ");

  /* Read from an invalid offset */
  if (nbd_pread (nbd, NULL, 0, -1, 0) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_pread did not fail with bogus offset\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (EINVAL, "nbd_pread: ");

  /* Use unknown command flags */
  if (nbd_pread (nbd, NULL, 0, 0, -1) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_pread did not fail with bogus flags\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (EINVAL, "nbd_pread: ");

  /* Check that oversized requests are rejected */
  if (nbd_pread (nbd, buf, MAXSIZE, 0, 0) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_pread did not fail with oversize request\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (ERANGE, "nbd_pread: ");
  if (nbd_aio_pwrite (nbd, buf, MAXSIZE, 0,
                      NBD_NULL_COMPLETION, 0) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_aio_pwrite did not fail with oversize request\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (ERANGE, "nbd_aio_pwrite: ");

  /* Send a read that the nbdkit sh plugin will fail. */
  if (nbd_pread (nbd, buf, 512, 0, 0) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_pread did not report server failure\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (ENOMEM, "nbd_pread: ");

  /* Queue up two write commands so large that we block on POLLIN (the
   * first might not block when under load, such as valgrind, but the
   * second definitely will, since the nbdkit sh plugin reads only one
   * command at a time and stalls on the first), then queue multiple
   * disconnects.
   */
  if (nbd_aio_pwrite (nbd, buf, 2 * 1024 * 1024, 0,
                      NBD_NULL_COMPLETION, 0) == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_aio_pwrite (nbd, buf, 2 * 1024 * 1024, 0,
                      NBD_NULL_COMPLETION, 0) == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if ((nbd_aio_get_direction (nbd) & LIBNBD_AIO_DIRECTION_WRITE) == 0) {
    fprintf (stderr, "%s: test failed: "
             "expect to be blocked on write\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  if (nbd_aio_disconnect (nbd, 0) == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_aio_disconnect (nbd, 0) != -1) {
    fprintf (stderr, "%s: test failed: "
             "no diagnosis that nbd_aio_disconnect prevents new commands\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (EINVAL, "nbd_aio_disconnect: ");

  /* Unblock the nbdkit sh plugin */
  if (close (witness_fd) == -1) {
    perror ("close");
    exit (EXIT_FAILURE);
  }
  witness_fd = -1;
  if (unlink (witness) == -1) {
    perror ("unlink");
    exit (EXIT_FAILURE);
  }

  /* Flush the queue (whether this one fails is a race with how fast
   * the server shuts down, so don't enforce status), then try to send
   * another command while CLOSED/DEAD
   */
  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s: ignoring %s\n", argv[0], nbd_get_error ());
  }
  else
    fprintf (stderr, "%s: shutdown completed successfully\n", argv[0]);
  if (nbd_pread (nbd, NULL, 0, 0, 0) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_pread did not fail on non-connected handle\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (EINVAL, "nbd_pread: ");

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
