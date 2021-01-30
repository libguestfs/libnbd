/* NBD client library in userspace.
 * Copyright (C) 2020 Red Hat Inc.
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
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>

#include <libnbd.h>

#include "nbdcopy.h"

static char buf[MAX_REQUEST_SIZE];

void
synch_copying (void)
{
  uint64_t offset = 0;

  /* If the source size is unknown then we copy data and cannot use
   * extent information.
   */
  if (src.size == -1) {
    size_t r;

    while ((r = src.ops->synch_read (&src, buf, sizeof buf, offset)) > 0) {
      dst.ops->synch_write (&dst, buf, r, offset);
      offset += r;
      progress_bar (offset, src.size);
    }
  }

  /* Otherwise we know how much we're copying, so we can copy in whole
   * blocks and use extent information to optimize the case.
   */
  else {
    while (offset < src.size) {
      extent_list exts = empty_vector;
      uint64_t count = src.size - offset;
      size_t i, r;

      if (count > sizeof buf)
        count = sizeof buf;

      if (extents)
        src.ops->get_extents (&src, 0, offset, count, &exts);
      else
        default_get_extents (&src, 0, offset, count, &exts);

      for (i = 0; i < exts.size; ++i) {
        assert (exts.ptr[i].length <= count);

        if (exts.ptr[i].hole) {
          if (!dst.ops->synch_trim (&dst, offset, exts.ptr[i].length) &&
              !dst.ops->synch_zero (&dst, offset, exts.ptr[i].length)) {
            /* If neither trimming nor efficient zeroing are possible,
             * write zeroes the hard way.
             */
            memset (buf, 0, exts.ptr[i].length);
            dst.ops->synch_write (&dst, buf, exts.ptr[i].length, offset);
          }
          offset += exts.ptr[i].length;
        }
        else /* data */ {
          r = src.ops->synch_read (&src, buf, exts.ptr[i].length, offset);

          /* These cases should never happen unless the file is
           * truncated underneath us.
           */
          if (r == 0 || r < exts.ptr[i].length) {
            fprintf (stderr, "%s: unexpected end of file\n", src.name);
            exit (EXIT_FAILURE);
          }

          dst.ops->synch_write (&dst, buf, r, offset);
          offset += r;
          progress_bar (offset, src.size);
        }
      }

      free (exts.ptr);
    } /* while */
  }
}
