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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <libnbd.h>

#include "nbdcopy.h"

#include "const-string-vector.h"
#include "ispowerof2.h"
#include "vector.h"

static struct rw_ops nbd_ops;

DEFINE_VECTOR_TYPE (handles, struct nbd_handle *)

struct rw_nbd {
  struct rw rw;

  /* Because of multi-conn we have to remember enough state in this
   * handle in order to be able to open another connection with the
   * same parameters after nbd_rw_create* has been called once.
   */
  enum { CREATE_URI, CREATE_SUBPROCESS } create_t;
  const char *uri;              /* For CREATE_URI */
  const_string_vector argv;     /* For CREATE_SUBPROCESS */
  direction d;

  handles handles;              /* One handle per connection. */
  bool can_zero;                /* Cached nbd_can_zero. */
};

static void
open_one_nbd_handle (struct rw_nbd *rwn)
{
  struct nbd_handle *nbd;

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s: %s\n", prog, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_set_debug (nbd, verbose);

  /* Set the handle name for debugging.  We could use rwn->rw.name
   * here but it is usually set to the lengthy NBD URI
   * (eg. "nbd://localhost:10809") which makes debug messages very
   * long.
   */
  if (verbose) {
    char *name;
    const size_t index = rwn->handles.len;

    if (asprintf (&name, "%s%zu",
                  rwn->d == READING ? "src" : "dst",
                  index) == -1) {
      perror ("asprintf");
      exit (EXIT_FAILURE);
    }
    nbd_set_handle_name (nbd, name);
    free (name);
  }

  if (extents && rwn->d == READING &&
      nbd_add_meta_context (nbd, "base:allocation") == -1) {
    fprintf (stderr, "%s: %s\n", prog, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  switch (rwn->create_t) {
  case CREATE_URI:
    nbd_set_uri_allow_local_file (nbd, true); /* Allow ?tls-psk-file. */

    if (nbd_connect_uri (nbd, rwn->uri) == -1) {
      fprintf (stderr, "%s: %s: %s\n", prog, rwn->uri, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    break;

  case CREATE_SUBPROCESS:
    if (nbd_connect_systemd_socket_activation (nbd,
                                               (char **) rwn->argv.ptr)
        == -1) {
      fprintf (stderr, "%s: %s: %s\n", prog, rwn->argv.ptr[0],
               nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  /* Cache these.  We assume with multi-conn that each handle will act
   * the same way.
   */
  if (rwn->handles.len == 0) {
    int64_t block_size;

    rwn->can_zero = nbd_can_zero (nbd) > 0;

    rwn->rw.size = nbd_get_size (nbd);
    if (rwn->rw.size == -1) {
      fprintf (stderr, "%s: %s: %s\n", prog, rwn->rw.name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }

    block_size = nbd_get_block_size (nbd, LIBNBD_SIZE_PREFERRED);
    if (block_size == -1) {
      fprintf (stderr, "%s: %s: %s\n", prog, rwn->rw.name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    if (block_size > 0 && is_power_of_2 (block_size))
      rwn->rw.preferred = block_size;
    else
      rwn->rw.preferred = 4096;
  }

  if (handles_append (&rwn->handles, nbd) == -1) {
    perror ("realloc");
    exit (EXIT_FAILURE);
  }
}

struct rw *
nbd_rw_create_uri (const char *name, const char *uri, direction d)
{
  struct rw_nbd *rwn = calloc (1, sizeof *rwn);
  if (rwn == NULL) { perror ("calloc"); exit (EXIT_FAILURE); }

  rwn->rw.ops = &nbd_ops;
  rwn->rw.name = name;
  rwn->create_t = CREATE_URI;
  rwn->uri = uri;
  rwn->d = d;

  open_one_nbd_handle (rwn);

  return &rwn->rw;
}

struct rw *
nbd_rw_create_subprocess (const char **argv, size_t argc, direction d)
{
  size_t i;
  struct rw_nbd *rwn = calloc (1, sizeof *rwn);
  if (rwn == NULL) { perror ("calloc"); exit (EXIT_FAILURE); }

  rwn->rw.ops = &nbd_ops;
  rwn->rw.name = argv[0];
  rwn->create_t = CREATE_SUBPROCESS;
  rwn->d = d;

  /* We have to copy the args so we can null-terminate them. */
  for (i = 0; i < argc; ++i) {
    if (const_string_vector_append (&rwn->argv, argv[i]) == -1)
      goto error;
  }
  if (const_string_vector_append (&rwn->argv, NULL) == -1)
    goto error;

  open_one_nbd_handle (rwn);

  return &rwn->rw;

error:
  perror ("realloc");
  exit (EXIT_FAILURE);
}

static void
nbd_ops_close (struct rw *rw)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;
  size_t i;

  for (i = 0; i < rwn->handles.len; ++i) {
    if (nbd_shutdown (rwn->handles.ptr[i], 0) == -1) {
      fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    nbd_close (rwn->handles.ptr[i]);
  }

  handles_reset (&rwn->handles);
  const_string_vector_reset (&rwn->argv);
  free (rw);
}

static void
nbd_ops_flush (struct rw *rw)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;
  size_t i;

  for (i = 0; i < rwn->handles.len; ++i) {
    if (nbd_flush (rwn->handles.ptr[i], 0) == -1) {
      fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }
}

static bool
nbd_ops_is_read_only (struct rw *rw)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;

  if (rwn->handles.len > 0)
    return nbd_is_read_only (rwn->handles.ptr[0]);
  else
    return false;
}

static bool
nbd_ops_can_extents (struct rw *rw)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;

  if (rwn->handles.len > 0)
    return nbd_can_meta_context (rwn->handles.ptr[0], "base:allocation");
  else
    return false;
}

static bool
nbd_ops_can_multi_conn (struct rw *rw)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;

  if (rwn->handles.len > 0)
    return nbd_can_multi_conn (rwn->handles.ptr[0]);
  else
    return false;
}

static void
nbd_ops_start_multi_conn (struct rw *rw)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;
  size_t i;

  for (i = 1; i < connections; ++i)
    open_one_nbd_handle (rwn);

  assert (rwn->handles.len == connections);
}

static size_t
nbd_ops_synch_read (struct rw *rw,
                    void *data, size_t len, uint64_t offset)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;

  if (len > rw->size - offset)
    len = rw->size - offset;
  if (len == 0)
    return 0;

  if (nbd_pread (rwn->handles.ptr[0], data, len, offset, 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  return len;
}

static void
nbd_ops_synch_write (struct rw *rw,
                     const void *data, size_t len, uint64_t offset)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;

  if (nbd_pwrite (rwn->handles.ptr[0], data, len, offset, 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

static bool
nbd_ops_synch_zero (struct rw *rw, uint64_t offset, uint64_t count,
                    bool allocate)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;

  if (!rwn->can_zero)
    return false;

  if (nbd_zero (rwn->handles.ptr[0], count, offset,
                allocate ? LIBNBD_CMD_FLAG_NO_HOLE : 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  return true;
}

static void
nbd_ops_asynch_read (struct rw *rw,
                     struct command *command,
                     nbd_completion_callback cb)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;

  if (nbd_aio_pread (rwn->handles.ptr[command->worker->index],
                     slice_ptr (command->slice),
                     command->slice.len, command->offset,
                     cb, 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

static void
nbd_ops_asynch_write (struct rw *rw,
                      struct command *command,
                      nbd_completion_callback cb)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;

  if (nbd_aio_pwrite (rwn->handles.ptr[command->worker->index],
                      slice_ptr (command->slice),
                      command->slice.len, command->offset,
                      cb, 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

static bool
nbd_ops_asynch_zero (struct rw *rw, struct command *command,
                     nbd_completion_callback cb, bool allocate)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;

  if (!rwn->can_zero)
    return false;

  assert (command->slice.len <= UINT32_MAX);

  if (nbd_aio_zero (rwn->handles.ptr[command->worker->index],
                    command->slice.len, command->offset,
                    cb, allocate ? LIBNBD_CMD_FLAG_NO_HOLE : 0) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  return true;
}

static unsigned
nbd_ops_in_flight (struct rw *rw, size_t index)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;

  /* Since the commands are auto-retired in the callbacks we don't
   * need to count "done" commands.
   */
  return nbd_aio_in_flight (rwn->handles.ptr[index]);
}

static void
nbd_ops_get_polling_fd (struct rw *rw, size_t index,
                        int *fd, int *direction)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;
  struct nbd_handle *nbd;

  nbd = rwn->handles.ptr[index];

  *fd = nbd_aio_get_fd (nbd);
  if (*fd == -1)
    goto error;

  *direction = nbd_aio_get_direction (nbd);
  if (*direction == -1)
    goto error;

  return;

error:
  fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
  exit (EXIT_FAILURE);
}

static void
nbd_ops_asynch_notify_read (struct rw *rw, size_t index)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;
  if (nbd_aio_notify_read (rwn->handles.ptr[index]) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

static void
nbd_ops_asynch_notify_write (struct rw *rw, size_t index)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;
  if (nbd_aio_notify_write (rwn->handles.ptr[index]) == -1) {
    fprintf (stderr, "%s: %s\n", rw->name, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

/* Get the extents.
 *
 * This is done synchronously, but that's fine because commands from
 * the previous work range in flight continue to run, it's difficult
 * to (sanely) start new work until we have the full list of extents,
 * and in almost every case the remote NBD server can answer our
 * request for extents in a single round trip.
 */
static int add_extent (void *vp, const char *metacontext,
                       uint64_t offset, uint32_t *entries, size_t nr_entries,
                       int *error);

static void
nbd_ops_get_extents (struct rw *rw, size_t index,
                     uint64_t offset, uint64_t count,
                     extent_list *ret)
{
  struct rw_nbd *rwn = (struct rw_nbd *) rw;
  extent_list exts = empty_vector;
  struct nbd_handle *nbd;

  nbd = rwn->handles.ptr[index];

  ret->len = 0;

  while (count > 0) {
    const uint64_t old_offset = offset;
    size_t i;

    exts.len = 0;
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

    /* Copy the extents returned into the final list (ret). */
    for (i = 0; i < exts.len; ++i) {
      assert (exts.ptr[i].offset == offset);
      if (exts.ptr[i].offset + exts.ptr[i].length > offset + count) {
        uint64_t d = exts.ptr[i].offset + exts.ptr[i].length - offset - count;
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

    /* The server should always make progress. */
    if (offset == old_offset) {
      fprintf (stderr, "%s: NBD server is broken: it is not returning extent information.\nTry nbdcopy --no-extents as a workaround.\n",
               rw->name);
      exit (EXIT_FAILURE);
    }
  }

  free (exts.ptr);
}

static int
add_extent (void *vp, const char *metacontext,
            uint64_t offset, uint32_t *entries, size_t nr_entries,
            int *error)
{
  extent_list *ret = vp;
  size_t i;

  if (strcmp (metacontext, "base:allocation") != 0 || *error)
    return 0;

  for (i = 0; i < nr_entries; i += 2) {
    struct extent e;

    e.offset = offset;
    e.length = entries[i];

    /* Note we deliberately don't care about the HOLE flag.  There is
     * no need to read extent that reads as zeroes.  We will convert
     * to it to a hole or allocated extents based on the command line
     * arguments.
     */
    e.zero = (entries[i+1] & LIBNBD_STATE_ZERO) != 0;

    if (extent_list_append (ret, e) == -1) {
      perror ("realloc");
      exit (EXIT_FAILURE);
    }

    offset += entries[i];
  }

  return 0;
}

static struct rw_ops nbd_ops = {
  .ops_name = "nbd_ops",
  .close = nbd_ops_close,
  .is_read_only = nbd_ops_is_read_only,
  .can_extents = nbd_ops_can_extents,
  .can_multi_conn = nbd_ops_can_multi_conn,
  .start_multi_conn = nbd_ops_start_multi_conn,
  .flush = nbd_ops_flush,
  .synch_read = nbd_ops_synch_read,
  .synch_write = nbd_ops_synch_write,
  .synch_zero = nbd_ops_synch_zero,
  .asynch_read = nbd_ops_asynch_read,
  .asynch_write = nbd_ops_asynch_write,
  .asynch_zero = nbd_ops_asynch_zero,
  .in_flight = nbd_ops_in_flight,
  .get_polling_fd = nbd_ops_get_polling_fd,
  .asynch_notify_read = nbd_ops_asynch_notify_read,
  .asynch_notify_write = nbd_ops_asynch_notify_write,
  .get_extents = nbd_ops_get_extents,
};
