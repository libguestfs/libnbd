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

#include <libnbd.h>

#include "vector.h"

#define MIN_REQUEST_SIZE 4096
#define MAX_REQUEST_SIZE (32 * 1024 * 1024)

/* This must be a multiple of MAX_REQUEST_SIZE.  Larger is better up
 * to a point, but it reduces the effectiveness of threads if the work
 * ranges are large compared to the virtual file size.
 */
#define THREAD_WORK_SIZE (128 * 1024 * 1024)

/* Abstracts the input (src) and output (dst) parameters on the
 * command line.
 */
struct rw {
  struct rw_ops *ops;           /* Operations. */
  const char *name;             /* Printable name, for error messages etc. */
  int64_t size;                 /* May be -1 for streams. */
  uint64_t preferred;           /* Preferred block size. */
  /* Followed by private data for the particular subtype. */
};

/* Source and destination. */
extern struct rw *src, *dst;

typedef enum { READING, WRITING } direction;

/* Create subtypes. */
extern struct rw *file_create (const char *name, int fd,
                               off_t st_size, uint64_t preferred,
                               bool is_block, direction d);
extern struct rw *nbd_rw_create_uri (const char *name,
                                     const char *uri, direction d);
extern struct rw *nbd_rw_create_subprocess (const char **argv, size_t argc,
                                            direction d);
extern struct rw *null_create (const char *name);
extern struct rw *pipe_create (const char *name, int fd);

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

/* Worker state used by multi-threaded copying. */
struct worker {
  pthread_t thread;
  size_t index;

  /* The number of bytes queued for in flight read and write requests.
   * Tracking this allows issuing many small requests, but limiting the
   * number of large requests.
   */
  size_t queue_size;
};

/* Commands for asynchronous operations in flight.
 *
 * We don't store the command type (read/write/zero/etc) because it is
 * implicit in the function being called and because commands
 * naturally change from read -> write/zero/etc as they progress.
 *
 * slice.buffer may be NULL for commands (like zero) that have no
 * associated data.
 *
 * A separate set of commands, slices and buffers is maintained per
 * thread so no locking is necessary.
 */
struct command {
  uint64_t offset;              /* Offset relative to start of disk. */
  struct slice slice;           /* Data slice. */
  struct worker *worker;        /* The worker owning this command. */
};

/* List of extents for rw->ops->get_extents. */
struct extent {
  uint64_t offset;
  uint64_t length;
  bool zero;
};
DEFINE_VECTOR_TYPE(extent_list, struct extent);

/* The operations struct hides some of the differences between local
 * file, NBD and pipes from the copying code.
 *
 * All these functions exit on error so they do not have to return
 * error indications.
 */
struct rw_ops {
  /* Debug string. */
  const char *ops_name;

  /* Close the connection and free up associated resources. */
  void (*close) (struct rw *rw);

  /* Return true if this is a read-only connection. */
  bool (*is_read_only) (struct rw *rw);

  /* For source only, does it support reading extents? */
  bool (*can_extents) (struct rw *rw);

  /* Return true if the connection can do multi-conn.  This is true
   * for files, false for streams, and passed through for NBD.
   */
  bool (*can_multi_conn) (struct rw *rw);

  /* For multi-conn capable backends, before copying we must call this
   * to begin multi-conn.  For NBD this means opening the additional
   * connections.
   */
  void (*start_multi_conn) (struct rw *rw);

  /* Truncate, only called on output files.  This callback can be NULL
   * for types that don't support this.
   */
  void (*truncate) (struct rw *rw, int64_t size);

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

  /* Synchronously zero.  If not possible, returns false. */
  bool (*synch_zero) (struct rw *rw, uint64_t offset, uint64_t count,
                      bool allocate);

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

  /* Asynchronously zero.  command->slice.buffer is not used.  If not possible,
   * returns false.  'cb' must be called only if returning true.
   */
  bool (*asynch_zero) (struct rw *rw, struct command *command,
                       nbd_completion_callback cb, bool allocate);

  /* Number of asynchronous commands in flight for a particular thread. */
  unsigned (*in_flight) (struct rw *rw, size_t index);

  /* Get polling file descriptor and direction, and notify read/write.
   * For sources which cannot be polled (such as files and pipes)
   * get_polling_fd returns fd == -1 (NOT an error), and the
   * asynch_notify_* functions are no-ops.
   */
  void (*get_polling_fd) (struct rw *rw, size_t index,
                          int *fd_rtn, int *direction_rtn);
  void (*asynch_notify_read) (struct rw *rw, size_t index);
  void (*asynch_notify_write) (struct rw *rw, size_t index);

  /* Read base:allocation extents metadata for a region of the source.
   * For local files the same information is read from the kernel.
   *
   * Note that qemu-img fetches extents for the entire disk up front,
   * and we want to avoid doing that because it had very negative
   * behaviour for certain sources (ie. VDDK).
   */
  void (*get_extents) (struct rw *rw, size_t index,
                       uint64_t offset, uint64_t count,
                       extent_list *ret);
};

extern void default_get_extents (struct rw *rw, size_t index,
                                 uint64_t offset, uint64_t count,
                                 extent_list *ret);
extern void get_polling_fd_not_supported (struct rw *rw, size_t index,
                                          int *fd_rtn, int *direction_rtn);
extern void asynch_notify_read_write_not_supported (struct rw *rw,
                                                    size_t index);

extern bool allocated;
extern unsigned connections;
extern bool destination_is_zero;
extern bool extents;
extern bool flush;
extern unsigned max_requests;
extern bool progress;
extern int progress_fd;
extern unsigned request_size;
extern unsigned queue_size;
extern unsigned sparse_size;
extern bool synchronous;
extern unsigned threads;
extern bool verbose;

extern const char *prog;

extern void progress_bar (off_t pos, int64_t size);
extern void synch_copying (void);
extern void multi_thread_copying (void);

#endif /* NBDCOPY_H */
