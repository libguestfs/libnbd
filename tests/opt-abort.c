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

/* Test behavior of nbd_opt_abort. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>

#include <libnbd.h>

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int64_t r;
  const char *s;
  char *args[] = { "nbdkit", "-s", "--exit-with-parent", "-v", "null", NULL };

  /* Get into negotiating state. */
  nbd = nbd_create ();
  if (nbd == NULL ||
      nbd_set_opt_mode (nbd, true) == -1 ||
      nbd_connect_command (nbd, args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Protocol should be "newstyle-fixed", with structured replies already
   * negotiated.
   */
  if (nbd_aio_is_negotiating (nbd) != true) {
    fprintf (stderr, "unexpected state after connection\n");
    exit (EXIT_FAILURE);
  }
  s = nbd_get_protocol (nbd);
  if (strcmp (s, "newstyle-fixed") != 0) {
    fprintf (stderr,
             "incorrect protocol \"%s\", expected \"newstyle-fixed\"\n", s);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_get_structured_replies_negotiated (nbd)) != 1) {
    fprintf (stderr,
             "incorrect structured replies %" PRId64 ", expected 1\n", r);
    exit (EXIT_FAILURE);
  }

  /* Quitting negotiation should be graceful. */
  if (nbd_opt_abort (nbd) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (nbd_aio_is_closed (nbd) != true) {
    fprintf (stderr, "unexpected state after abort\n");
    exit (EXIT_FAILURE);
  }

  /* As negotiation never finished, we have no size. */
  if ((r = nbd_get_size (nbd)) != -1) {
    fprintf (stderr, "%s: test failed: incorrect size, "
             "actual %" PRIi64 ", expected -1",
             argv[0], r);
    exit (EXIT_FAILURE);
  }
  if ((r = nbd_get_errno ()) != EINVAL) {
    fprintf (stderr, "%s: test failed: unexpected errno, "
             "actual %" PRIi64 ", expected %d EINVAL",
             argv[0], r, EINVAL);
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);
  exit (EXIT_SUCCESS);
}
