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

/* Test structured reply read callback. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <errno.h>

#include <libnbd.h>

/* Depends on structured-read.sh setting things up so that qemu-nbd
 * exposes an image with a 512-byte hole at offset 2048 followed by a
 * 512-byte data section containing all '1' bytes at offset 2560
 * (non-zero offsets to test that everything is calculated correctly).
 */
static char rbuf[1024];

struct data {
  bool df;         /* input: true if DF flag was passed to request */
  int count;       /* input: count of expected remaining calls */
  bool fail;       /* input: true to return failure */
  bool seen_hole;  /* output: true if hole encountered */
  bool seen_data;  /* output: true if data encountered */
};

static int
read_cb (void *opaque,
         const void *bufv, size_t count, uint64_t offset,
         unsigned status, int *error)
{
  struct data *data = opaque;
  const char *buf = bufv;

  /* The NBD spec allows chunks to be reordered; we are relying on the
   * fact that qemu-nbd does not do so.
   */
  assert (!*error || (data->fail && data->count == 1 && *error == EPROTO));
  assert (data->count-- > 0);

  switch (status) {
  case LIBNBD_READ_DATA:
    if (data->df) {
      assert (buf == rbuf);
      assert (count == 1024);
      assert (offset == 2048);
      assert (buf[0] == 0 && memcmp (buf, buf + 1, 511) == 0);
      assert (buf[512] == 1 && memcmp (buf + 512, buf + 513, 511) == 0);
    }
    else {
      assert (buf == rbuf + 512);
      assert (count == 512);
      assert (offset == 2048 + 512);
      assert (buf[0] == 1 && memcmp (buf, buf + 1, 511) == 0);
    }
    assert (!data->seen_data);
    data->seen_data = true;
    break;
  case LIBNBD_READ_HOLE:
    assert (!data->df); /* Relies on qemu-nbd's behavior */
    assert (buf == rbuf);
    assert (count == 512);
    assert (offset == 2048);
    assert (buf[0] == 0 && memcmp (buf, buf + 1, 511) == 0);
    assert (!data->seen_hole);
    data->seen_hole = true;
    break;
  case LIBNBD_READ_ERROR:
    /* For now, qemu-nbd cannot provoke this status. */
  default:
    assert (false);
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

  if (argc < 2) {
    fprintf (stderr, "%s qemu-nbd [args ...]\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_connect_systemd_socket_activation (nbd, &argv[1]) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  exportsize = nbd_get_size (nbd);
  if (exportsize == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (exportsize != 3072) {
    fprintf (stderr, "unexpected file size\n");
    exit (EXIT_FAILURE);
  }

  if (nbd_can_df (nbd) != 1) {
    fprintf (stderr, "skipping test: qemu too old to use structured reads\n");
    exit (77);
  }

  memset (rbuf, 2, sizeof rbuf);
  data = (struct data) { .count = 2, };
  if (nbd_pread_structured (nbd, rbuf, sizeof rbuf, 2048,
                            (nbd_chunk_callback) { .callback = read_cb, .user_data = &data },
                            0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  assert (data.seen_data && data.seen_hole);

  /* Repeat with DF flag. */
  memset (rbuf, 2, sizeof rbuf);
  data = (struct data) { .df = true, .count = 1, };
  if (nbd_pread_structured (nbd, rbuf, sizeof rbuf, 2048,
                            (nbd_chunk_callback) { .callback = read_cb, .user_data = &data },
                            LIBNBD_CMD_FLAG_DF) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  assert (data.seen_data && !data.seen_hole);

  /* Trigger a failed callback, to prove connection stays up. With
   * reads, all chunks trigger a callback even after failure, but the
   * first errno sticks.
   */
  memset (rbuf, 2, sizeof rbuf);
  data = (struct data) { .count = 2, .fail = true, };
  if (nbd_pread_structured (nbd, rbuf, sizeof rbuf, 2048,
                            (nbd_chunk_callback) { .callback = read_cb, .user_data = &data },
                            0) != -1) {
    fprintf (stderr, "unexpected pread callback success\n");
    exit (EXIT_FAILURE);
  }
  assert (nbd_get_errno () == EPROTO && nbd_aio_is_ready (nbd));
  assert (data.seen_data && data.seen_hole);

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
