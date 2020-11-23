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
  size_t r;

  while ((r = src.ops->synch_read (&src, buf, sizeof buf, offset)) > 0) {
    dst.ops->synch_write (&dst, buf, r, offset);
    offset += r;
    if (progress)
      progress_bar (offset, dst.size);
  }
  if (progress)
    progress_bar (1, 1);
}
