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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "internal.h"

int
nbd_internal_errno_of_nbd_error (uint32_t error)
{
  switch (error) {
  case NBD_SUCCESS: return 0;
  case NBD_EPERM: return EPERM;
  case NBD_EIO: return EIO;
  case NBD_ENOMEM: return ENOMEM;
  case NBD_EINVAL: return EINVAL;
  case NBD_ENOSPC: return ENOSPC;
  case NBD_EOVERFLOW: return EOVERFLOW;
  case NBD_ENOTSUP: return ENOTSUP;
  case NBD_ESHUTDOWN: return ESHUTDOWN;
  default: return EINVAL;
  }
}

/* XXX In nbdkit this function is generated from the nbd-protocol.h
 * header file.  We should unify this across the two programs.
 */
const char *
nbd_internal_name_of_nbd_cmd (uint16_t type)
{
  switch (type) {
  case NBD_CMD_READ: return "read";
  case NBD_CMD_WRITE: return "write";
  case NBD_CMD_DISC: return "disconnect";
  case NBD_CMD_FLUSH: return "flush";
  case NBD_CMD_TRIM: return "trim";
  case NBD_CMD_CACHE: return "cache";
  case NBD_CMD_WRITE_ZEROES: return "write-zeroes";
  case NBD_CMD_BLOCK_STATUS: return "block-status";
  default: return "UNKNOWN!";
  }
}
