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

static size_t read_src (void *buf, size_t len, off_t pos);
static void write_dst (const void *buf, size_t len, off_t pos);

void
synch_copying (void)
{
  off_t pos = 0;
  size_t r;

  while ((r = read_src (buf, sizeof buf, pos)) > 0) {
    write_dst (buf, r, pos);
    pos += r;
    if (progress)
      progress_bar (pos, dst.size);
  }
  if (progress)
    progress_bar (1, 1);
}

/* Read from src up to len bytes into buf.  Returns 0 if we've reached
 * the end of the input.  This exits on failure.
 */
static size_t
read_src (void *buf, size_t len, off_t pos)
{
  ssize_t r;

  switch (src.t) {
  case LOCAL:
    r = read (src.u.local.fd, buf, len);
    if (r == -1) {
      perror (src.name);
      exit (EXIT_FAILURE);
    }
    return r;

  case NBD:
    if (len > src.size - pos) len = src.size - pos;
    if (len == 0)
      return 0;
    if (nbd_pread (src.u.nbd.ptr[0], buf, len, pos, 0) == -1) {
      fprintf (stderr, "%s: %s\n", src.name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    return len;

  default: abort ();
  }
}

/* Write buf to dst.  This exits on failure. */
static void
write_dst (const void *buf, size_t len, off_t pos)
{
  ssize_t r;

  switch (dst.t) {
  case LOCAL:
    while (len > 0) {
      r = write (dst.u.local.fd, buf, len);
      if (r == -1) {
        perror (dst.name);
        exit (EXIT_FAILURE);
      }
      buf += r;
      len -= r;
      pos += r;
      if (progress)
        progress_bar (pos, dst.size);
    }
    break;

  case NBD:
    if (nbd_pwrite (dst.u.nbd.ptr[0], buf, len, pos, 0) == -1) {
      fprintf (stderr, "%s: %s\n", dst.name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    break;

  default: abort ();
  }
}
