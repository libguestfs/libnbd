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
  struct pollfd fds[1];
  int r;

  /* fd might be negative, and poll will ignore it. */
  fds[0].fd = nbd_unlocked_aio_get_fd (h);
  switch (nbd_internal_aio_get_direction (get_next_state (h))) {
  case LIBNBD_AIO_DIRECTION_READ:
    fds[0].events = POLLIN;
    break;
  case LIBNBD_AIO_DIRECTION_WRITE:
    fds[0].events = POLLOUT;
    break;
  case LIBNBD_AIO_DIRECTION_BOTH:
    fds[0].events = POLLIN|POLLOUT;
    break;
  default:
    set_error (EINVAL, "nothing to poll for in state %s",
               nbd_internal_state_short_string (get_next_state (h)));
    return -1;
  }
  fds[0].revents = 0;

  /* Note that it's not safe to release the handle lock here, as it
   * would allow other threads to close file descriptors which we have
   * passed to poll.
   */
  r = poll (fds, 1, timeout);
  if (r == -1) {
    set_error (errno, "poll");
    return -1;
  }

  /* POLLIN and POLLOUT might both be set.  However we shouldn't call
   * both nbd_aio_notify_read and nbd_aio_notify_write at this time
   * since the first might change the handle state, making the second
   * notification invalid.  Nothing bad happens by ignoring one of the
   * notifications since if it's still valid it will be picked up by a
   * subsequent poll.
   */
  r = 0;
  if ((fds[0].revents & POLLIN) != 0)
    r = nbd_unlocked_aio_notify_read (h);
  else if ((fds[0].revents & POLLOUT) != 0)
    r = nbd_unlocked_aio_notify_write (h);
  if (r == -1)
    return -1;

  return 0;
}
