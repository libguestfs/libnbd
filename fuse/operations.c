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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libnbd.h>

#include "nbdfuse.h"

#define MAX_REQUEST_SIZE (32 * 1024 * 1024)

/* Wraps calls to libnbd functions and automatically checks for error,
 * returning errors in the format required by FUSE.  It also prints
 * out the full error message on stderr, so that we don't lose it.
 */
#define CHECK_NBD_ERROR(CALL)                                   \
  do { if ((CALL) == -1) return check_nbd_error (); } while (0)
static int
check_nbd_error (void)
{
  int err;

  fprintf (stderr, "%s\n", nbd_get_error ());
  err = nbd_get_errno ();
  if (err != 0)
    return -err;
  else
    return -EIO;
}

static int
nbdfuse_getattr (const char *path, struct stat *statbuf,
                 struct fuse_file_info *fi)
{
  const int mode = readonly ? 0444 : 0666;

  memset (statbuf, 0, sizeof *statbuf);

  /* We're probably making some Linux-specific assumptions here, but
   * this file is not usually compiled on non-Linux systems (perhaps
   * on OpenBSD?).  XXX
   */
  statbuf->st_atim = start_t;
  statbuf->st_mtim = start_t;
  statbuf->st_ctim = start_t;
  statbuf->st_uid = geteuid ();
  statbuf->st_gid = getegid ();

  if (file_mode || (path[0] == '/' && strcmp (path+1, filename) == 0)) {
    /* getattr "/filename" */
    statbuf->st_mode = S_IFREG | mode;
    statbuf->st_nlink = 1;
    statbuf->st_size = size;
  }
  else if (strcmp (path, "/") == 0) {
    /* getattr "/" */
    statbuf->st_mode = S_IFDIR | (mode & 0111);
    statbuf->st_nlink = 2;
  }
  else
    return -ENOENT;

  return 0;
}

static int
nbdfuse_readdir (const char *path, void *buf,
                 fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi,
                 enum fuse_readdir_flags flags)
{
  if (strcmp (path, "/") != 0)
    return -ENOENT;

  filler (buf, ".", NULL, 0, 0);
  filler (buf, "..", NULL, 0, 0);
  filler (buf, filename, NULL, 0, 0);

  return 0;
}

/* This function checks the O_RDONLY/O_RDWR flags passed to the
 * open(2) call, so we have to check the open mode is compatible with
 * the readonly flag.
 */
static int
nbdfuse_open (const char *path, struct fuse_file_info *fi)
{
  if (!file_mode && (path[0] != '/' || strcmp (path+1, filename) != 0))
    return -ENOENT;

  if (readonly && (fi->flags & O_ACCMODE) != O_RDONLY)
    return -EACCES;

  return 0;
}

static int
nbdfuse_read (const char *path, char *buf,
              size_t count, off_t offset,
              struct fuse_file_info *fi)
{
  if (!file_mode && (path[0] != '/' || strcmp (path+1, filename) != 0))
    return -ENOENT;

  if (offset >= size)
    return 0;

  if (count > MAX_REQUEST_SIZE)
    count = MAX_REQUEST_SIZE;

  if (offset + count > size)
    count = size - offset;

  CHECK_NBD_ERROR (nbd_pread (nbd, buf, count, offset, 0));

  return (int) count;
}

static int
nbdfuse_write (const char *path, const char *buf,
               size_t count, off_t offset,
               struct fuse_file_info *fi)
{
  /* Probably shouldn't happen because of nbdfuse_open check. */
  if (readonly)
    return -EACCES;

  if (!file_mode && (path[0] != '/' || strcmp (path+1, filename) != 0))
    return -ENOENT;

  if (offset >= size)
    return 0;

  if (count > MAX_REQUEST_SIZE)
    count = MAX_REQUEST_SIZE;

  if (offset + count > size)
    count = size - offset;

  CHECK_NBD_ERROR (nbd_pwrite (nbd, buf, count, offset, 0));

  return (int) count;
}

static int
nbdfuse_fsync (const char *path, int datasync, struct fuse_file_info *fi)
{
  if (readonly)
    return 0;

  /* If the server doesn't support flush then the operation is
   * silently ignored.
   */
  if (nbd_can_flush (nbd))
    CHECK_NBD_ERROR (nbd_flush (nbd, 0));

  return 0;
}

/* This is called on the last close of a file.  We do a flush here to
 * be on the safe side, but it's not strictly necessary.
 */
static int
nbdfuse_release (const char *path, struct fuse_file_info *fi)
{
  if (readonly)
    return 0;

  return nbdfuse_fsync (path, 0, fi);
}

struct fuse_operations nbdfuse_operations = {
  .getattr           = nbdfuse_getattr,
  .readdir           = nbdfuse_readdir,
  .open              = nbdfuse_open,
  .read              = nbdfuse_read,
  .write             = nbdfuse_write,
  .fsync             = nbdfuse_fsync,
  .release           = nbdfuse_release,
};
