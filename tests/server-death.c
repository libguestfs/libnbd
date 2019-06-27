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

/* Deliberately disconnect while stranding commands, to check their status.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <libnbd.h>

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  char buf[512];
  int64_t handle;
  char pidfile[] = "/tmp/libnbd-test-disconnectXXXXXX";
  int fd;
  pid_t pid;
  int r;
  int delay = 0;
  const char *cmd[] = { "nbdkit", "--pidfile", pidfile, "-s",
                        "--exit-with-parent", "--filter=delay", "memory",
                        "size=1m", "delay-reads=5", NULL };

  /* We're going to kill the child, but don't want to wait for a zombie */
  if (signal (SIGCHLD, SIG_IGN) == SIG_ERR) {
    fprintf (stderr, "%s: signal: %s\n", argv[0], strerror (errno));
    exit (EXIT_FAILURE);
  }

  fd = mkstemp (pidfile);
  if (fd < 0) {
    fprintf (stderr, "%s: mkstemp: %s\n", argv[0], strerror (errno));
    exit (EXIT_FAILURE);
  }

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    goto fail;
  }

  /* Connect to a slow server. */
  if (nbd_connect_command (nbd, (char **) cmd) == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    goto fail;
  }
  if (read (fd, buf, sizeof buf) == -1) {
    fprintf (stderr, "%s: read: %s\n", argv[0], strerror (errno));
    goto fail;
  }
  pid = atoi (buf);
  if (pid <= 0) {
    fprintf (stderr, "%s: unable to parse server's pid\n", argv[0]);
    goto fail;
  }

  /* Issue a read that should not complete yet. */
  if ((handle = nbd_aio_pread (nbd, buf, sizeof buf, 0, 0)) == -1) {
    fprintf (stderr, "%s: test failed: nbd_aio_pread\n", argv[0]);
    goto fail;
  }
  if (nbd_aio_peek_command_completed (nbd) != 0) {
    fprintf (stderr, "%s: test failed: nbd_aio_peek_command_completed\n",
             argv[0]);
    goto fail;
  }
  if (nbd_aio_command_completed (nbd, handle) != 0) {
    fprintf (stderr, "%s: test failed: nbd_aio_command_completed\n", argv[0]);
    goto fail;
  }

  /* Kill the server forcefully (SIGINT is not always strong enough,
   * as nbdkit waits for pending transactions to finish before
   * actually exiting), although it's a race whether our signal
   * arrives while nbdkit has a pending transaction.
   */
  if (kill (pid, SIGKILL) == -1) {
    fprintf (stderr, "%s: kill: %s\n", argv[0], strerror (errno));
    goto fail;
  }
  /* Wait up to 10 seconds, 100 ms at a time */
  while (kill (pid, 0) == 0) {
    if (delay++ > 100) {
      fprintf (stderr, "%s: kill taking too long\n", argv[0]);
      goto fail;
    }
    usleep (100 * 1000);
  }

  /* This part is somewhat racy if we don't wait for the process death
   * above - depending on load and timing, nbd_poll may succeed or
   * fail, and we may transition to either CLOSED (the state machine
   * saw a clean EOF) or DEAD (the state machine saw a stranded
   * transaction or POLLERR).
   */
  while ((r = nbd_poll (nbd, 1000)) == 1)
    if (nbd_aio_is_dead (nbd) || nbd_aio_is_closed (nbd))
      break;
  if (!(nbd_aio_is_dead (nbd) || nbd_aio_is_closed (nbd))) {
    fprintf (stderr, "%s: test failed: server death not detected\n", argv[0]);
    goto fail;
  }

  /* Proof that the read was stranded */
  if (nbd_aio_peek_command_completed (nbd) != 0) {
    fprintf (stderr, "%s: test failed: nbd_aio_peek_command_completed\n",
             argv[0]);
    goto fail;
  }
  if (nbd_aio_command_completed (nbd, handle) != 0) {
    fprintf (stderr, "%s: test failed: nbd_aio_command_completed\n", argv[0]);
    goto fail;
  }

  close (fd);
  unlink (pidfile);
  nbd_close (nbd);
  exit (EXIT_SUCCESS);

 fail:
  unlink (pidfile);
  exit (EXIT_FAILURE);
}
