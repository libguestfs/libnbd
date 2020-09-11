/* NBD client library in userspace
 * Copyright (C) 2013-2020 Red Hat Inc.
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

/* Deliberately shutdown while stranding commands, to check their status.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <libnbd.h>

static bool write_retired;
static const char *progname;

static int
callback (void *ignored, int *error)
{
  if (*error != ENOTCONN) {
    fprintf (stderr, "%s: unexpected error in pwrite callback: %s\n",
             progname, strerror (*error));
    return 0;
  }
  write_retired = 1;
  return 1;
}

static char buf[2 * 1024 * 1024];

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int err;
  const char *msg;
  int64_t cookie;
  const char *cmd[] = { "nbdkit", "-s", "--exit-with-parent",
                        "memory", "size=2m", NULL };

  progname = argv[0];

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Connect to a server. */
  if (nbd_connect_command (nbd, (char **) cmd) == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Pause the server from reading, so that our first request will
   * exceed the buffer and force the second request to be stuck client
   * side (without stopping the server, we would be racing on whether
   * we hit a block on writes based on whether the server can read
   * faster than we can fill the pipe).
   */
  if (nbd_kill_subprocess (nbd, SIGSTOP) == -1) {
    fprintf (stderr, "%s: test failed: nbd_kill_subprocess: %s\n", argv[0],
             nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Issue back-to-back write requests, both large enough to block.  Set up
   * the second to auto-retire via callback.
   */
  if ((cookie = nbd_aio_pwrite (nbd, buf, sizeof buf, 0,
                                NBD_NULL_COMPLETION, 0)) == -1) {
    fprintf (stderr, "%s: test failed: first nbd_aio_pwrite: %s\n", argv[0],
             nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_aio_pwrite (nbd, buf, sizeof buf, 0,
                      (nbd_completion_callback) { .callback = callback },
                      0) == -1) {
    fprintf (stderr, "%s: test failed: second nbd_aio_pwrite: %s\n", argv[0],
             nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Resume the server; but now our state machine remains blocked
   * until we notify or otherwise poll it.
   */
  if (nbd_kill_subprocess (nbd, SIGCONT) == -1) {
    fprintf (stderr, "%s: test failed: nbd_kill_subprocess: %s\n", argv[0],
             nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_aio_peek_command_completed (nbd) != 0) {
    fprintf (stderr, "%s: test failed: nbd_aio_peek_command_completed\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  if (nbd_aio_command_completed (nbd, cookie) != 0) {
    fprintf (stderr, "%s: test failed: nbd_aio_command_completed\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  /* Send an immediate shutdown.  This will abort the second write, as
   * well as kick the state machine to finish the first.
   */
  if (nbd_shutdown (nbd, LIBNBD_SHUTDOWN_ABANDON_PENDING) == -1) {
    fprintf (stderr, "%s: test failed: nbd_shutdown\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  /* All in-flight commands should now be completed.  But whether the
   * first write succeeded or failed depends on the server, so we
   * merely retire it without checking status.
   */
  if (nbd_aio_in_flight (nbd) != 0) {
    fprintf (stderr, "%s: test failed: nbd_aio_in_flight\n", argv[0]);
    exit (EXIT_FAILURE);
  }
  if (nbd_aio_peek_command_completed (nbd) != cookie) {
    fprintf (stderr, "%s: test failed: nbd_aio_peek_command_completed\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  nbd_aio_command_completed (nbd, cookie);

  /* With all commands retired, no further command should be pending */
  if (!write_retired) {
    fprintf (stderr, "%s: test failed: second nbd_aio_pwrite not retired\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  if (nbd_aio_peek_command_completed (nbd) != -1) {
    fprintf (stderr, "%s: test failed: nbd_aio_peek_command_completed\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  msg = nbd_get_error ();
  err = nbd_get_errno ();
  printf ("error: \"%s\"\n", msg);
  printf ("errno: %d (%s)\n", err, strerror (err));
  if (err != EINVAL) {
    fprintf (stderr, "%s: test failed: unexpected errno %d (%s)\n", argv[0],
             err, strerror (err));
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
