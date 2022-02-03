/* NBD client library in userspace.
 * Copyright (C) 2020-2022 Red Hat Inc.
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

#ifndef NBDCOPY_H
#define NBDCOPY_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libnbd.h>

#include "vector.h"

#define MAX_REQUEST_SIZE (32 * 1024 * 1024)

/* This must be a multiple of MAX_REQUEST_SIZE.  Larger is better up
 * to a point, but it reduces the effectiveness of threads if the work
 * ranges are large compared to the virtual file size.
 */
#define THREAD_WORK_SIZE (128 * 1024 * 1024)

DEFINE_VECTOR_TYPE (handles, struct nbd_handle *)

/* Abstracts the input (src) and output (dst) parameters on the
 * command line.
 */
struct rw {
  struct rw_ops *ops;           /* Operations. */
  const char *name;             /* Printable name, for error messages etc. */
  int64_t size;                 /* May be -1 for streams. */
  union {
    struct {                    /* For files and pipes. */
      int fd;
      struct stat stat;
      bool seek_hole_supported;
      int sector_size;
    } local;
    struct {
      handles handles;          /* For NBD, one handle per connection. */
      bool can_trim, can_zero;  /* Cached nbd_can_trim, nbd_can_zero. */
    } nbd;
  } u;
};

extern struct rw src, dst;

/* Underlying data buffers. */
struct buffer {
  char *data;                   /* Pointer to base address of allocation. */
  unsigned refs;                /* Reference count. */
};

/* Slice used to share whole or part of underlying buffers. */
struct slice {
  size_t len;                   /* Length of slice. */
  size_t base;                  /* Start of slice relative to buffer. */
  struct buffer *buffer;        /* Underlying allocation (may be shared
                                 * or NULL).
                                 */
};

#define slice_ptr(slice) ((slice).buffer->data + (slice).base)

/* Commands for asynchronous operations in flight.
 *
 * We don't store the command type (read/write/trim/etc) because it is
 * implicit in the function being called and because commands
 * naturally change from read -> write/trim/etc as they progress.
 *
 * slice.buffer may be NULL for commands (like trim) that have no
 * associated data.
 *
 * A separate set of commands, slices and buffers is maintained per
 * thread so no locking is necessary.
 */
struct command {
  uint64_t offset;              /* Offset relative to start of disk. */
  struct slice slice;           /* Data slice. */
  uintptr_t index;              /* Thread number. */
};

/* List of extents for rw->ops->get_extents. */
struct extent {
  uint64_t offset;
  uint64_t length;
  bool hole;
};
DEFINE_VECTOR_TYPE(extent_list, struct extent);

/* The operations struct hides some of the differences between local
 * file, NBD and pipes from the copying code.
 *
 * All these functions exit on error so they do not have to return
 * error indications.
 */
struct rw_ops {
  /* Close the connection and free up associated resources. */
  void (*close) (struct rw *rw);

  /* Flush pending writes to permanent storage. */
  void (*flush) (struct rw *rw);

  /* Synchronous I/O operations.
   *
   * synch_read may not return the requested length of data (eg. for
   * pipes) and returns 0 at end of file.
   */
  size_t (*synch_read) (struct rw *rw,
                        void *data, size_t len, uint64_t offset);
  void (*synch_write) (struct rw *rw,
                       const void *data, size_t len, uint64_t offset);

  /* Synchronously trim.  If not possible, returns false. */
  bool (*synch_trim) (struct rw *rw, uint64_t offset, uint64_t count);

  /* Synchronously zero.  If not possible, returns false. */
  bool (*synch_zero) (struct rw *rw, uint64_t offset, uint64_t count);

  /* Asynchronous I/O operations.  These start the operation and call
   * 'cb' on completion.  'cb' will return 1, for auto-retiring with
   * asynchronous libnbd calls.
   *
   * The file_ops versions are actually implemented synchronously, but
   * still call 'cb'.
   *
   * These always read/write the full amount.  These functions cannot
   * be called on pipes because pipes force --synchronous mode.
   */
  void (*asynch_read) (struct rw *rw,
                       struct command *command,
                       nbd_completion_callback cb);
  void (*asynch_write) (struct rw *rw,
                        struct command *command,
                        nbd_completion_callback cb);

  /* Asynchronously trim.  command->slice.buffer is not used.  If not
   * possible, returns false.
   */
  bool (*asynch_trim) (struct rw *rw, struct command *command,
                       nbd_completion_callback cb);

  /* Asynchronously zero.  command->slice.buffer is not used.  If not possible,
   * returns false.  'cb' must be called only if returning true.
   */
  bool (*asynch_zero) (struct rw *rw, struct command *command,
                       nbd_completion_callback cb);

  /* Number of asynchronous commands in flight for a particular thread. */
  unsigned (*in_flight) (struct rw *rw, uintptr_t index);

  /* Get polling file descriptor and direction, and notify read/write.
   * For sources which cannot be polled (such as files and pipes)
   * get_polling_fd returns fd == -1 (NOT an error), and the
   * asynch_notify_* functions are no-ops.
   */
  void (*get_polling_fd) (struct rw *rw, uintptr_t index,
                          int *fd_rtn, int *direction_rtn);
  void (*asynch_notify_read) (struct rw *rw, uintptr_t index);
  void (*asynch_notify_write) (struct rw *rw, uintptr_t index);

  /* Read base:allocation extents metadata for a region of the source.
   * For local files the same information is read from the kernel.
   *
   * Note that qemu-img fetches extents for the entire disk up front,
   * and we want to avoid doing that because it had very negative
   * behaviour for certain sources (ie. VDDK).
   */
  void (*get_extents) (struct rw *rw, uintptr_t index,
                       uint64_t offset, uint64_t count,
                       extent_list *ret);
};
extern struct rw_ops file_ops;
extern struct rw_ops nbd_ops;
extern struct rw_ops pipe_ops;

extern void default_get_extents (struct rw *rw, uintptr_t index,
                                 uint64_t offset, uint64_t count,
                                 extent_list *ret);
extern void get_polling_fd_not_supported (struct rw *rw, uintptr_t index,
                                          int *fd_rtn, int *direction_rtn);
extern void asynch_notify_read_write_not_supported (struct rw *rw,
                                                    uintptr_t index);

extern bool allocated;
extern unsigned connections;
extern bool destination_is_zero;
extern bool extents;
extern bool flush;
extern unsigned max_requests;
extern bool progress;
extern int progress_fd;
extern unsigned sparse_size;
extern bool synchronous;
extern unsigned threads;

extern void progress_bar (off_t pos, int64_t size);
extern void synch_copying (void);
extern void multi_thread_copying (void);

#endif /* NBDCOPY_H */
