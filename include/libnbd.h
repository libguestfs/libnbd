/* nbd client library in userspace: public header
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

#ifndef LIBNBD_H
#define LIBNBD_H

#include <stdint.h>
#include <sys/socket.h>

struct nbd_handle;
struct nbd_connection;

extern struct nbd_handle *nbd_create (void);
extern int nbd_set_multi_conn (struct nbd_handle *h, unsigned multi_conn);

extern int nbd_connect_unix (struct nbd_handle *h, const char *sockpath);
extern int nbd_pread (struct nbd_handle *h, void *buf,
                      size_t count, uint64_t offset);
extern int nbd_pwrite (struct nbd_handle *h, const void *buf,
                       size_t count, uint64_t offset);

extern struct nbd_connection *nbd_aio_get_connection (struct nbd_handle *h,
                                                      unsigned int i);
extern int nbd_aio_connect (struct nbd_connection *conn,
                            const struct sockaddr *, socklen_t);
extern int64_t nbd_aio_pread (struct nbd_connection *conn, void *buf,
                              size_t count, uint64_t offset);
extern int64_t nbd_aio_pwrite (struct nbd_connection *conn, const void *buf,
                               size_t count, uint64_t offset);

extern int nbd_aio_get_fd (struct nbd_connection *conn);
#define LIBNBD_AIO_DIRECTION_READ  1
#define LIBNBD_AIO_DIRECTION_WRITE 2
#define LIBNBD_AIO_DIRECTION_BOTH  3
extern int nbd_aio_get_direction (struct nbd_connection *conn);
extern int nbd_aio_notify_read (struct nbd_connection *conn);
extern int nbd_aio_notify_write (struct nbd_connection *conn);
extern int nbd_aio_is_ready (struct nbd_connection *conn);
extern int nbd_aio_is_dead (struct nbd_connection *conn);
extern int nbd_aio_command_completed (struct nbd_connection *conn,
                                      int64_t handle);

extern int nbd_aio_poll (struct nbd_handle *h, int timeout);

extern const char *nbd_aio_connection_state (struct nbd_connection *conn);

#endif /* LIBNBD_H */
