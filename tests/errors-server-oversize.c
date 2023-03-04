/* NBD client library in userspace
 * Copyright Red Hat
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
#include <inttypes.h>
#include <sys/stat.h>

#include <libnbd.h>
#include "requires.h"

#define MAXSIZE 68157440 /* 65M, oversize on purpose */

static char *progname;
static char buf[MAXSIZE];

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

static void
check_server_fail (struct nbd_handle *h, int64_t cookie,
                   const char *cmd, int experr)
{
  int r;

  if (cookie == -1) {
    fprintf (stderr, "%s: test failed: %s not sent to server\n",
             progname, cmd);
    exit (EXIT_FAILURE);
  }

  while ((r = nbd_aio_command_completed (h, cookie)) == 0) {
    if (nbd_poll (h, -1) == -1) {
      fprintf (stderr, "%s: test failed: poll failed while awaiting %s: %s\n",
               progname, cmd, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  if (r != -1) {
    fprintf (stderr, "%s: test failed: %s did not fail at server\n",
             progname, cmd);
    exit (EXIT_FAILURE);
  }
  check (experr, "nbd_aio_command_completed: ");
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  const char *cmd[] = {
    "nbdkit", "-s", "-v", "--exit-with-parent", "eval",
    "get_size=    echo 68157440",
    "block_size=  echo 1 512 16M",
    "pread=       echo EIO >&2; exit 1",
    "pwrite=      if test $3 -gt $((32*1024*1024)); then\n"
    "               exit 6\n" /* Hard disconnect */
    "             elif test $3 -gt $((16*1024*1024)); then\n"
    "               echo EOVERFLOW >&2; exit 1\n"
    "             fi\n"
    "             cat >/dev/null",
    NULL,
  };
  uint32_t strict;

  progname = argv[0];
  requires ("max=$(nbdkit --dump-plugin eval | "
            "sed -n '/^max_known_status=/ s///p') && test \"$max\" -ge 6");

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

  /* Check the advertised max sizes. */
  printf ("server block size maximum: %" PRId64 "\n",
          nbd_get_block_size (nbd, LIBNBD_SIZE_MAXIMUM));
  printf ("libnbd payload size maximum: %" PRId64 "\n",
          nbd_get_block_size (nbd, LIBNBD_SIZE_PAYLOAD));

  /* Disable client-side safety check */
  strict = nbd_get_strict_mode (nbd) & ~LIBNBD_STRICT_PAYLOAD;
  if (nbd_set_strict_mode (nbd, strict) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Handle graceful server rejection of oversize request */
  check_server_fail (nbd,
                     nbd_aio_pwrite (nbd, buf, 17*1024*1024, 0,
                                     NBD_NULL_COMPLETION, 0),
                     "17M nbd_aio_pwrite", EINVAL);

  /* Handle abrupt server rejection of oversize request */
  check_server_fail (nbd,
                     nbd_aio_pwrite (nbd, buf, 33*1024*1024, 0,
                                     NBD_NULL_COMPLETION, 0),
                     "33M nbd_aio_pwrite", ENOTCONN);

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
