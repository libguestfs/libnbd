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

/* Test closure lifetimes. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <libnbd.h>

static char *nbdkit[] =
  { "nbdkit", "-s", "--exit-with-parent", "-v",
    "null", "size=512",
    NULL };

static char *nbdkit_delay[] =
  { "nbdkit", "-s", "--exit-with-parent", "-v",
    "--filter=delay",
    "null", "size=512",
    "delay-read=10",
    NULL };

static unsigned debug_fn_called;
static unsigned debug_fn_freed;
static unsigned read_cb_called;
static unsigned read_cb_freed;
static unsigned completion_cb_called;
static unsigned completion_cb_freed;

static int
debug_fn (void *opaque, const char *context, const char *msg)
{
  debug_fn_called++;
  return 0;
}

static void
debug_fn_free (void *opaque)
{
  debug_fn_freed++;
}

static int
read_cb (void *opaque,
         const void *subbuf, size_t count,
         uint64_t offset, unsigned status, int *error)
{
  assert (!read_cb_freed);
  read_cb_called++;
  return 0;
}

static void
read_cb_free (void *opaque)
{
  read_cb_freed++;
}

static int
completion_cb (void *opaque, int *error)
{
  assert (!completion_cb_freed);
  completion_cb_called++;
  return 0;
}

static void
completion_cb_free (void *opaque)
{
  completion_cb_freed++;
}

#define NBD_ERROR                                               \
  do {                                                          \
    fprintf (stderr, "%s: %s\n", argv[0], nbd_get_error ());    \
    exit (EXIT_FAILURE);                                        \
  } while (0)

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int64_t cookie;
  char buf[512];

  /* Check debug functions are freed when a new debug function is
   * registered, and when the handle is closed.
   */
  nbd = nbd_create ();
  if (nbd == NULL) NBD_ERROR;

  nbd_set_debug_callback (nbd,
                          (nbd_debug_callback) { .callback = debug_fn,
                                                 .free = debug_fn_free });
  assert (debug_fn_freed == 0);

  nbd_set_debug_callback (nbd, (nbd_debug_callback) { .callback = debug_fn,
                                                      .free = debug_fn_free });
  assert (debug_fn_freed == 1);

  debug_fn_freed = 0;
  nbd_close (nbd);
  assert (debug_fn_freed == 1);

  /* Test command callbacks are freed when the command is retired. */
  nbd = nbd_create ();
  if (nbd == NULL) NBD_ERROR;
  if (nbd_connect_command (nbd, nbdkit) == -1) NBD_ERROR;

  cookie = nbd_aio_pread_structured (nbd, buf, sizeof buf, 0,
                                     (nbd_chunk_callback) { .callback = read_cb,
                                                            .free = read_cb_free },
                                     (nbd_completion_callback) { .callback = completion_cb,
                                                                 .free = completion_cb_free },
                                     0);
  if (cookie == -1) NBD_ERROR;
  assert (read_cb_freed == 0);
  assert (completion_cb_freed == 0);
  while (!nbd_aio_command_completed (nbd, cookie)) {
    if (nbd_poll (nbd, -1) == -1) NBD_ERROR;
  }

  assert (read_cb_called == 1);
  assert (completion_cb_called == 1);
  assert (read_cb_freed == 1);
  assert (completion_cb_freed == 1);

  nbd_kill_subprocess (nbd, 0);
  nbd_close (nbd);

  /* Test command callbacks are freed if the handle is closed without
   * running the commands.
   */
  read_cb_called = read_cb_freed =
    completion_cb_called = completion_cb_freed = 0;
  nbd = nbd_create ();
  if (nbd == NULL) NBD_ERROR;
  if (nbd_connect_command (nbd, nbdkit_delay) == -1) NBD_ERROR;

  cookie = nbd_aio_pread_structured (nbd, buf, sizeof buf, 0,
                                     (nbd_chunk_callback) { .callback = read_cb,
                                                            .free = read_cb_free },
                                     (nbd_completion_callback) { .callback = completion_cb,
                                                                 .free = completion_cb_free },
                                     0);
  if (cookie == -1) NBD_ERROR;
  nbd_kill_subprocess (nbd, 0);
  nbd_close (nbd);

  assert (read_cb_freed == 1);
  assert (completion_cb_freed == 1);

  exit (EXIT_SUCCESS);
}
