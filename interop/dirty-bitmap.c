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

/* Test reading qemu dirty bitmap. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <errno.h>

#include <libnbd.h>

static const char *bitmap;

struct data {
  bool req_one;    /* input: true if req_one was passed to request */
  int count;       /* input: count of expected remaining calls */
  bool fail;       /* input: true to return failure */
  bool seen_base;  /* output: true if base:allocation encountered */
  bool seen_dirty; /* output: true if qemu:dirty-bitmap encountered */
};

static int
cb (void *opaque, const char *metacontext, uint64_t offset,
    uint32_t *entries, size_t len, int *error)
{
  struct data *data = opaque;

  /* libnbd does not actually verify that a server is fully compliant
   * to the spec; the asserts marked [qemu-nbd] are thus dependent on
   * the fact that qemu-nbd is compliant.
   */
  assert (offset == 0);
  assert (!*error || (data->fail && data->count == 1 && *error == EPROTO));
  assert (data->count-- > 0); /* [qemu-nbd] */

  if (strcmp (metacontext, LIBNBD_CONTEXT_BASE_ALLOCATION) == 0) {
    assert (!data->seen_base); /* [qemu-nbd] */
    data->seen_base = true;
    assert (len == (data->req_one ? 2 : 8)); /* [qemu-nbd] */

    /* Data block offset 0 size 128k */
    assert (entries[0] == 131072); assert (entries[1] == 0);
    if (!data->req_one) {
      /* hole|zero offset 128k size 384k */
      assert (entries[2] == 393216); assert (entries[3] == (LIBNBD_STATE_HOLE|
                                                            LIBNBD_STATE_ZERO));
      /* allocated zero offset 512k size 64k */
      assert (entries[4] ==  65536); assert (entries[5] == LIBNBD_STATE_ZERO);
      /* hole|zero offset 576k size 448k */
      assert (entries[6] == 458752); assert (entries[7] == (LIBNBD_STATE_HOLE|
                                                            LIBNBD_STATE_ZERO));
    }
  }
  else if (strcmp (metacontext, bitmap) == 0) {
    assert (!data->seen_dirty); /* [qemu-nbd] */
    data->seen_dirty = true;
    assert (len == (data->req_one ? 2 : 10)); /* [qemu-nbd] */

    assert (entries[0] ==  65536); assert (entries[1] == 0);
    if (!data->req_one) {
      /* dirty block offset 64K size 64K */
      assert (entries[2] ==  65536); assert (entries[3] == 1);
      assert (entries[4] == 393216); assert (entries[5] == 0);
      /* dirty block offset 512K size 64K */
      assert (entries[6] ==  65536); assert (entries[7] == 1);
      assert (entries[8] == 458752); assert (entries[9] == 0);
    }
  }
  else {
    fprintf (stderr, "unexpected context %s\n", metacontext);
    exit (EXIT_FAILURE);
  }

  if (data->fail) {
    /* Something NBD servers can't send */
    *error = data->count == 1 ? EPROTO : ECONNREFUSED;
    return -1;
  }
  return 0;
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  int64_t exportsize;
  struct data data;
  char c;

  if (argc < 3) {
    fprintf (stderr, "%s bitmap qemu-nbd [args ...]\n", argv[0]);
    exit (EXIT_FAILURE);
  }
  bitmap = argv[1];

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_add_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION);
  nbd_add_meta_context (nbd, bitmap);

  if (nbd_connect_systemd_socket_activation (nbd, &argv[2]) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  exportsize = nbd_get_size (nbd);
  if (exportsize == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  data = (struct data) { .count = 2, };
  if (nbd_block_status (nbd, exportsize, 0,
                        (nbd_extent_callback) { .callback = cb, .user_data = &data },
                        0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  assert (data.seen_base && data.seen_dirty);

  data = (struct data) { .req_one = true, .count = 2, };
  if (nbd_block_status (nbd, exportsize, 0,
                        (nbd_extent_callback) { .callback = cb, .user_data = &data },
                        LIBNBD_CMD_FLAG_REQ_ONE) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  assert (data.seen_base && data.seen_dirty);

  /* Trigger a failed callback, to prove connection stays up. */
  data = (struct data) { .count = 2, .fail = true, };
  if (nbd_block_status (nbd, exportsize, 0,
                        (nbd_extent_callback) { .callback = cb, .user_data = &data },
                        0) != -1) {
    fprintf (stderr, "unexpected block status success\n");
    exit (EXIT_FAILURE);
  }
  assert (nbd_get_errno () == EPROTO && nbd_aio_is_ready (nbd));
  assert (data.seen_base && data.seen_dirty);

  if (nbd_pread (nbd, &c, 1, 0, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);

  exit (EXIT_SUCCESS);
}
