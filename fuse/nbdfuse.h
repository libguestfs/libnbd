/* NBD client library in userspace
 * Copyright (C) 2013-2021 Red Hat Inc.
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

#ifndef LIBNBD_NBDFUSE_H
#define LIBNBD_NBDFUSE_H

#include <stdbool.h>
#include <time.h>

/* Define fuse API version and include the header file in one place so
 * we can be sure to get the same API in all source files.
 */
#define FUSE_USE_VERSION 35
#include <fuse.h>

#include "vector.h"

DEFINE_VECTOR_TYPE (handles, struct nbd_handle *)

extern handles nbd;
extern unsigned connections;
extern bool readonly;
extern bool file_mode;
extern struct timespec start_t;
extern char *filename;
extern uint64_t size;

extern struct fuse_operations nbdfuse_operations;
extern void start_operations_threads (void);

#endif /* LIBNBD_NBDFUSE_H */
