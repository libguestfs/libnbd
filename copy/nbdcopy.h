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

#ifndef NBDCOPY_H
#define NBDCOPY_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libnbd.h>

#include "vector.h"

#define MAX_REQUEST_SIZE (32 * 1024 * 1024)

/* This must be a multiple of MAX_REQUEST_SIZE. */
#define THREAD_WORK_SIZE (128 * 1024 * 1024)

DEFINE_VECTOR_TYPE (handles, struct nbd_handle *)

/* Abstracts the input (src) and output (dst) parameters on the
 * command line.
 */
struct rw {
  enum { LOCAL = 1, NBD = 2 } t;/* Type. */
  struct rw_ops *ops;           /* Operations. */
  const char *name;             /* Printable name, for error messages etc. */
  int64_t size;                 /* May be -1 for streams. */
  union {
    struct {                    /* For LOCAL. */
      int fd;
      struct stat stat;
    } local;
    handles nbd;                /* For NBD, one handle per connection. */
  } u;
};

extern struct rw src, dst;

/* Buffer used for asynchronous operations in flight. */
struct buffer {
  uint64_t offset;
  size_t len;
  char *data;                   /* Data buffer. */
  void (*free_data) (void *);   /* Usually free(3), can be NULL. */
  uintptr_t index;              /* Thread number. */
};

/* The operations struct hides some of the differences between local
 * file, NBD and pipes from the copying code.
 *
 * All these functions exit on error so they do not have to return
 * error indications.
 */
struct rw_ops {
  /* Synchronous I/O operations.
   *
   * synch_read may not return the requested length of data (eg. for
   * pipes) and returns 0 at end of file.
   */
  size_t (*synch_read) (struct rw *rw,
                        void *data, size_t len, uint64_t offset);
  void (*synch_write) (struct rw *rw,
                       const void *data, size_t len, uint64_t offset);

  /* Asynchronous I/O operations.  These start the operation and call
   * 'cb' on completion.
   *
   * The file_ops versions are actually implemented synchronously, but
   * still call 'cb'.
   *
   * These always read/write the full amount.  These functions cannot
   * be called on pipes because pipes force --synchronous mode.
   */
  void (*asynch_read) (struct rw *rw,
                       struct buffer *buffer,
                       nbd_completion_callback cb);
  void (*asynch_write) (struct rw *rw,
                        struct buffer *buffer,
                        nbd_completion_callback cb);
};
extern struct rw_ops file_ops;
extern struct rw_ops nbd_ops;
extern struct rw_ops pipe_ops;

extern unsigned connections;
extern bool flush;
extern unsigned max_requests;
extern bool progress;
extern bool synchronous;
extern unsigned threads;

extern void progress_bar (off_t pos, int64_t size);
extern void synch_copying (void);
extern void multi_thread_copying (void);

#endif /* NBDCOPY_H */
