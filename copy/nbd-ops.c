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
#include <assert.h>

#include <libnbd.h>

#include "nbdcopy.h"

static void
nbd_ops_close (struct rw *rw)
{
  size_t i;

  for (i = 0; i < rw->u.nbd.size; ++i) {
    if (nbd_shutdown (rw->u.nbd.ptr[i], 0) == -1) {
      fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    nbd_close (rw->u.nbd.ptr[i]);
  }

  handles_reset (&rw->u.nbd);
}

static void
nbd_ops_flush (struct rw *rw)
{
  size_t i;

  for (i = 0; i < rw->u.nbd.size; ++i) {
    if (nbd_flush (rw->u.nbd.ptr[i], 0) == -1) {
      fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }
}

static size_t
nbd_ops_synch_read (struct rw *rw,
                void *data, size_t len, uint64_t offset)
{
  if (len > rw->size - offset)
    len = rw->size - offset;
  if (len == 0)
    return 0;

  if (nbd_pread (rw->u.nbd.ptr[0], data, len, offset, 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  return len;
}

static void
nbd_ops_synch_write (struct rw *rw,
                 const void *data, size_t len, uint64_t offset)
{
  if (nbd_pwrite (rw->u.nbd.ptr[0], data, len, offset, 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

static bool
nbd_ops_synch_trim (struct rw *rw, uint64_t offset, uint64_t count)
{
  if (nbd_can_trim (rw->u.nbd.ptr[0]) == 0)
    return false;

  if (nbd_trim (rw->u.nbd.ptr[0], count, offset, 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  return true;
}

static bool
nbd_ops_synch_zero (struct rw *rw, uint64_t offset, uint64_t count)
{
  if (nbd_can_zero (rw->u.nbd.ptr[0]) == 0)
    return false;

  if (nbd_zero (rw->u.nbd.ptr[0],
                count, offset, LIBNBD_CMD_FLAG_NO_HOLE) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  return true;
}

static void
nbd_ops_asynch_read (struct rw *rw,
                     struct buffer *buffer,
                     nbd_completion_callback cb)
{
  if (nbd_aio_pread (rw->u.nbd.ptr[buffer->index],
                     buffer->data, buffer->len, buffer->offset,
                     cb, 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

static void
nbd_ops_asynch_write (struct rw *rw,
                      struct buffer *buffer,
                      nbd_completion_callback cb)
{
  if (nbd_aio_pwrite (rw->u.nbd.ptr[buffer->index],
                      buffer->data, buffer->len, buffer->offset,
                      cb, 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

static bool
nbd_ops_asynch_trim (struct rw *rw, struct buffer *buffer,
                     nbd_completion_callback cb)
{
  if (nbd_can_trim (rw->u.nbd.ptr[0]) == 0)
    return false;

  assert (buffer->len <= UINT32_MAX);

  if (nbd_aio_trim (rw->u.nbd.ptr[buffer->index],
                    buffer->len, buffer->offset,
                    cb, 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  return true;
}

static bool
nbd_ops_asynch_zero (struct rw *rw, struct buffer *buffer,
                     nbd_completion_callback cb)
{
  if (nbd_can_zero (rw->u.nbd.ptr[0]) == 0)
    return false;

  assert (buffer->len <= UINT32_MAX);

  if (nbd_aio_zero (rw->u.nbd.ptr[buffer->index],
                    buffer->len, buffer->offset,
                    cb, LIBNBD_CMD_FLAG_NO_HOLE) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  return true;
}

static int
add_extent (void *vp, const char *metacontext,
            uint64_t offset, uint32_t *entries, size_t nr_entries,
            int *error)
{
  extent_list *ret = vp;
  size_t i;

  if (strcmp (metacontext, "base:allocation") != 0)
    return 0;

  for (i = 0; i < nr_entries; i += 2) {
    struct extent e;

    e.offset = offset;
    e.length = entries[i];
    /* Note we deliberately don't care about the ZERO flag. */
    e.hole = (entries[i+1] & LIBNBD_STATE_HOLE) != 0;
    if (extent_list_append (ret, e) == -1) {
      perror ("realloc");
      exit (EXIT_FAILURE);
    }

    offset += entries[i];
  }

  return 0;
}

static unsigned
nbd_ops_in_flight (struct rw *rw, uintptr_t index)
{
  /* Since the commands are auto-retired in the callbacks we don't
   * need to count "done" commands.
   */
  return nbd_aio_in_flight (rw->u.nbd.ptr[index]);
}

static void
nbd_ops_get_polling_fd (struct rw *rw, uintptr_t index,
                        int *fd, int *direction)
{
  struct nbd_handle *nbd;

  nbd = rw->u.nbd.ptr[index];

  *fd = nbd_aio_get_fd (nbd);
  if (*fd == -1) {
  error:
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  *direction = nbd_aio_get_direction (nbd);
  if (*direction == -1)
    goto error;
}

static void
nbd_ops_asynch_notify_read (struct rw *rw, uintptr_t index)
{
  if (nbd_aio_notify_read (rw->u.nbd.ptr[index]) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

static void
nbd_ops_asynch_notify_write (struct rw *rw, uintptr_t index)
{
  if (nbd_aio_notify_write (rw->u.nbd.ptr[index]) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

/* This is done synchronously, but that's fine because commands from
 * the previous work range in flight continue to run, it's difficult
 * to (sanely) start new work until we have the full list of extents,
 * and in almost every case the remote NBD server can answer our
 * request for extents in a single round trip.
 */
static void
nbd_ops_get_extents (struct rw *rw, uintptr_t index,
                     uint64_t offset, uint64_t count,
                     extent_list *ret)
{
  extent_list exts = empty_vector;
  struct nbd_handle *nbd;

  nbd = rw->u.nbd.ptr[index];

  ret->size = 0;

  while (count > 0) {
    size_t i;

    exts.size = 0;
    if (nbd_block_status (nbd, count, offset,
                          (nbd_extent_callback) {
                            .user_data = &exts,
                            .callback = add_extent
                          }, 0) == -1) {
      /* XXX We could call default_get_extents, but unclear if it's
       * the right thing to do if the server is returning errors.
       */
      fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }

    /* The server should always make progress. */
    if (exts.size == 0) {
      fprintf (stderr, "%s: NBD server is broken: it is not returning extent information.\nTry nbdcopy --no-extents as a workaround.\n",
               rw->name);
      exit (EXIT_FAILURE);
    }

    /* Copy the extents returned into the final list (ret).  This is
     * complicated because the extents returned by the server may
     * begin earlier and begin or end later than the requested size.
     */
    for (i = 0; i < exts.size; ++i) {
      uint64_t d;

      if (exts.ptr[i].offset + exts.ptr[i].length <= offset)
        continue;
      if (exts.ptr[i].offset < offset) {
        d = offset - exts.ptr[i].offset;
        exts.ptr[i].offset += d;
        exts.ptr[i].length -= d;
        assert (exts.ptr[i].offset == offset);
      }
      if (exts.ptr[i].offset + exts.ptr[i].length > offset + count) {
        d = exts.ptr[i].offset + exts.ptr[i].length - offset - count;
        exts.ptr[i].length -= d;
        assert (exts.ptr[i].offset + exts.ptr[i].length == offset + count);
      }
      if (exts.ptr[i].length == 0)
        continue;
      if (extent_list_append (ret, exts.ptr[i]) == -1) {
        perror ("realloc");
        exit (EXIT_FAILURE);
      }

      offset += exts.ptr[i].length;
      count -= exts.ptr[i].length;
    }
  }

  free (exts.ptr);
}

struct rw_ops nbd_ops = {
  .close = nbd_ops_close,
  .flush = nbd_ops_flush,
  .synch_read = nbd_ops_synch_read,
  .synch_write = nbd_ops_synch_write,
  .synch_trim = nbd_ops_synch_trim,
  .synch_zero = nbd_ops_synch_zero,
  .asynch_read = nbd_ops_asynch_read,
  .asynch_write = nbd_ops_asynch_write,
  .asynch_trim = nbd_ops_asynch_trim,
  .asynch_zero = nbd_ops_asynch_zero,
  .in_flight = nbd_ops_in_flight,
  .get_polling_fd = nbd_ops_get_polling_fd,
  .asynch_notify_read = nbd_ops_asynch_notify_read,
  .asynch_notify_write = nbd_ops_asynch_notify_write,
  .get_extents = nbd_ops_get_extents,
};
