/* NBD client library in userspace
 * Copyright (C) 2013-2022 Red Hat Inc.
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

static char *progname;
static char buf[512];

static void
check (int experr, const char *prefix)
{
  const char *msg = nbd_get_error ();
  int errnum = nbd_get_errno ();

  fprintf (stderr, "error: \"%s\"\n", msg);
  fprintf (stderr, "errno: %d (%s)\n", errnum, strerror (errnum));
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

static bool chunk_clean;      /* whether check_chunk has been called */
static bool completion_clean; /* whether check_completion has been called */

static void
check_chunk (void *data) {
  if (chunk_clean) {
    fprintf (stderr, "%s: test failed: "
             "chunk callback invoked multiple times\n", progname);
    exit (EXIT_FAILURE);
  }
  chunk_clean = true;
}

static void
check_completion (void *data) {
  if (completion_clean) {
    fprintf (stderr, "%s: test failed: "
             "completion callback invoked multiple times\n", progname);
    exit (EXIT_FAILURE);
  }
  completion_clean = true;
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  const char *cmd[] = {
    "nbdkit", "-s", "-v", "--exit-with-parent", "memory", "1048576", NULL
  };

  progname = argv[0];

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Connect to the server. */
  if (nbd_connect_command (nbd, (char **) cmd) == -1) {
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* We guarantee callbacks will be freed even on all error paths. */
  if (nbd_aio_pread_structured (nbd, buf, 512, -1,
                                (nbd_chunk_callback) { .free = check_chunk, },
                                (nbd_completion_callback) {
                                  .free = check_completion, },
                                0) != -1) {
    fprintf (stderr, "%s: test failed: "
             "nbd_aio_pread_structured did not fail with bogus offset\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }
  check (EINVAL, "nbd_aio_pread_structured: ");
  if (!chunk_clean || !completion_clean) {
    fprintf (stderr, "%s: test failed: "
             "callbacks not freed on nbd_aio_pread_structured failure\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
