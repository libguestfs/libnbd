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

#ifndef LIBNBD_NBDUBLK_H
#define LIBNBD_NBDUBLK_H

#include <stdbool.h>

#include <ublksrv.h>

#include "vector.h"

DEFINE_VECTOR_TYPE (handles, struct nbd_handle *);

#define UBLKSRV_TGT_TYPE_NBD 0

extern handles nbd;
extern unsigned connections;
extern bool readonly;
extern bool rotational;
extern bool can_fua;
extern char *filename;
extern uint64_t size;
extern uint64_t min_block_size;
extern uint64_t pref_block_size;
extern bool verbose;

extern struct ublksrv_tgt_type tgt_type;

extern int start_daemon (struct ublksrv_ctrl_dev *dev);

#endif /* LIBNBD_NBDUBLK_H */
