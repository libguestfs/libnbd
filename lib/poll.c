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
#include <poll.h>

#include "internal.h"

/* A simple main loop implementation using poll(2). */
int
nbd_unlocked_poll (struct nbd_handle *h, int timeout)
{
  struct pollfd fds[h->multi_conn];
  size_t i;
  int r;

  for (i = 0; i < h->multi_conn; ++i) {
    fds[i].fd = nbd_unlocked_aio_get_fd (h->conns[i]);
    switch (nbd_unlocked_aio_get_direction (h->conns[i])) {
    case LIBNBD_AIO_DIRECTION_READ:
      fds[i].events = POLLIN;
      break;
    case LIBNBD_AIO_DIRECTION_WRITE:
      fds[i].events = POLLOUT;
      break;
    case LIBNBD_AIO_DIRECTION_BOTH:
      fds[i].events = POLLIN|POLLOUT;
      break;
    }
    fds[i].revents = 0;
  }

  /* Note that it's not safe to release the handle lock here, as it
   * would allow other threads to close file descriptors which we have
   * passed to poll.
   */
  r = poll (fds, h->multi_conn, timeout);
  if (r == -1) {
    set_error (errno, "poll");
    return -1;
  }

  for (i = 0; i < h->multi_conn; ++i) {
    r = 0;
    /* POLLIN and POLLOUT might both be set.  However we shouldn't
     * call both nbd_aio_notify_read and nbd_aio_notify_write at this
     * time since the first might change the handle state, making the
     * second notification invalid.  Nothing bad happens by ignoring
     * one of the notifications since if it's still valid it will be
     * picked up by a subsequent poll.
     */
    if ((fds[i].revents & POLLIN) != 0)
      r = nbd_unlocked_aio_notify_read (h->conns[i]);
    else if ((fds[i].revents & POLLOUT) != 0)
      r = nbd_unlocked_aio_notify_write (h->conns[i]);
    if (r == -1)
      return -1;
  }

  return 0;
}
