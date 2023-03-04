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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libnbd.h>

#include "nbdinfo.h"

int can_exit_code;

void
do_can (void)
{
  int feature;

  if (strcasecmp (can, "connect") == 0 ||
      strcasecmp (can, "read") == 0)
    feature = 1;

  else if (strcasecmp (can, "tls") == 0)
    feature = nbd_get_tls_negotiated (nbd);

  else if (strcasecmp (can, "sr") == 0 ||
           strcasecmp (can, "structured") == 0 ||
           strcasecmp (can, "structured reply") == 0 ||
           strcasecmp (can, "structured-reply") == 0 ||
           strcasecmp (can, "structured_reply") == 0 ||
           strcasecmp (can, "structured replies") == 0 ||
           strcasecmp (can, "structured-replies") == 0 ||
           strcasecmp (can, "structured_replies") == 0)
    feature = nbd_get_structured_replies_negotiated (nbd);

  else if (strcasecmp (can, "readonly") == 0 ||
           strcasecmp (can, "read-only") == 0 ||
           strcasecmp (can, "read_only") == 0)
    feature = nbd_is_read_only (nbd);

  else if (strcasecmp (can, "write") == 0) {
    feature = nbd_is_read_only (nbd);
    if (feature >= 0) feature = !feature;
  }

  else if (strcasecmp (can, "rotational") == 0)
    feature = nbd_is_rotational (nbd);

  else if (strcasecmp (can, "cache") == 0)
    feature = nbd_can_cache (nbd);

  else if (strcasecmp (can, "df") == 0)
    feature = nbd_can_df (nbd);

  else if (strcasecmp (can, "fastzero") == 0 ||
           strcasecmp (can, "fast-zero") == 0 ||
           strcasecmp (can, "fast_zero") == 0)
    feature = nbd_can_fast_zero (nbd);

  else if (strcasecmp (can, "flush") == 0)
    feature = nbd_can_flush (nbd);

  else if (strcasecmp (can, "fua") == 0)
    feature = nbd_can_fua (nbd);

  else if (strcasecmp (can, "multiconn") == 0 ||
           strcasecmp (can, "multi-conn") == 0 ||
           strcasecmp (can, "multi_conn") == 0)
    feature = nbd_can_multi_conn (nbd);

  else if (strcasecmp (can, "trim") == 0)
    feature = nbd_can_trim (nbd);

  else if (strcasecmp (can, "zero") == 0)
    feature = nbd_can_zero (nbd);

  else {
    fprintf (stderr, "%s: unknown --can or --is option: %s\n",
             progname, can);
    exit (EXIT_FAILURE);
  }

  if (feature == -1) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Translate the feature bool into an exit code.  This is used in main(). */
  can_exit_code = feature ? EXIT_SUCCESS : 2;
}
