/* Example usage with qemu-nbd:
 *
 * sock=`mktemp -u`
 * qemu-nbd -f $format -k $sock -r image
 * ./strict-structured-reads $sock
 *
 * This will perform read randomly over the image and check that all
 * structured replies comply with the NBD spec (chunks may be out of
 * order or interleaved, but no read succeeds unless chunks cover the
 * entire region, with no overlapping or zero-length chunks).
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include <libnbd.h>

/* A linked list of ranges still not seen. */
struct range {
  uint64_t first;
  uint64_t last;
  struct range *next;
};

/* Per-read data. */
struct data {
  uint64_t offset;
  size_t count;
  uint32_t flags;
  size_t chunks;
  struct range *remaining;
};

#define MAX_BUF (2 * 1024 * 1024)
static char buf[MAX_BUF];

/* Various statistics */
static int total_data_chunks;
static int64_t total_data_bytes;
static int total_hole_chunks;
static int64_t total_hole_bytes;
static int total_chunks;
static int total_df_reads;
static int total_reads;
static int64_t total_bytes;
static int total_success;

static int
read_chunk (void *opaque,
            const void *bufv, size_t count, uint64_t offset,
            unsigned status, int *error)
{
  struct data *data = opaque;
  struct range *r, **prev;

  /* libnbd guarantees this: */
  assert (offset >= data->offset);
  assert (offset + count <= data->offset + data->count);

  switch (status) {
  case LIBNBD_READ_DATA:
    total_data_chunks++;
    total_data_bytes += count;
    break;
  case LIBNBD_READ_HOLE:
    total_hole_chunks++;
    total_hole_bytes += count;
    break;
  case LIBNBD_READ_ERROR:
    assert (count == 0);
    count = 1; /* Ensure no further chunks visit that offset */
    break;
  default:
    goto error;
  }
  data->chunks++;
  if (count == 0) {
    fprintf (stderr, "buggy server: chunk must have non-zero size\n");
    goto error;
  }

  /* Find element in remaining, or the server is in error */
  for (prev = &data->remaining, r = *prev; r; prev = &r->next, r = r->next) {
    if (offset >= r->first)
      break;
  }
  if (r == NULL || offset + count > r->last) {
    /* we fail to detect double errors reported at the same offset,
     * but at least the read is already going to fail.
     */
    if (status == LIBNBD_READ_ERROR)
      return 0;
    fprintf (stderr, "buggy server: chunk with overlapping range\n");
    goto error;
  }

  /* Resize or split r to track new remaining bytes */
  if (offset == r->first) {
    if (offset + count == r->last) {
      *prev = r->next;
      free (r);
    }
    else
      r->first += count;
  }
  else if (offset + count == r->last) {
    r->last -= count;
  }
  else {
    struct range *n = malloc (sizeof *n);
    assert (n);
    n->next = r->next;
    r->next = n;
    n->last = r->last;
    r->last = offset - r->first;
    n->first = offset + count;
  }

  return 0;
 error:
  *error = EPROTO;
  return -1;
}

static int
read_verify (void *opaque, int *error)
{
  int ret = 0;
  struct data *data = opaque;

  ret = -1;
  total_reads++;
  total_chunks += data->chunks;
  if (*error)
    goto cleanup;
  assert (data->chunks > 0);
  if (data->flags & LIBNBD_CMD_FLAG_DF) {
    total_df_reads++;
    if (data->chunks > 1) {
      fprintf (stderr, "buggy server: too many chunks for DF flag\n");
      *error = EPROTO;
      goto cleanup;
    }
  }
  if (data->remaining && !*error) {
    fprintf (stderr, "buggy server: not enough chunks on success\n");
    *error = EPROTO;
    goto cleanup;
  }
  total_bytes += data->count;
  total_success++;
  ret = 0;

 cleanup:
  while (data->remaining) {
    struct range *r = data->remaining;
    data->remaining = r->next;
    free (r);
  }

  return ret;
}

int
main (int argc, char *argv[])
{
  struct nbd_handle *nbd;
  size_t i;
  int64_t exportsize;
  int64_t maxsize = MAX_BUF;
  uint64_t offset;

  srand (time (NULL));

  if (argc != 2) {
    fprintf (stderr, "%s socket|uri\n", argv[0]);
    exit (EXIT_FAILURE);
  }

  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (strstr (argv[1], "://")) {
    if (nbd_connect_uri (nbd, argv[1]) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }
  else if (nbd_connect_unix (nbd, argv[1]) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  exportsize = nbd_get_size (nbd);
  if (exportsize == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  if (exportsize < 512) {
    fprintf (stderr, "image is too small for useful testing\n");
    exit (EXIT_FAILURE);
  }
  if (exportsize <= maxsize)
    maxsize = exportsize - 1;

  /* Queue up 1000 parallel reads. We are reusing the same buffer,
   * which is not safe in real life, but okay here because we aren't
   * validating contents, only server behavior.
   */
  for (i = 0; i < 1000; ++i) {
    uint32_t flags = 0;
    struct data *d = malloc (sizeof *d);
    struct range *r = malloc (sizeof *r);

    assert (d && r);
    offset = rand () % (exportsize - maxsize);
    if (rand() & 1)
      flags = LIBNBD_CMD_FLAG_DF;
    *r = (struct range) { .first = offset, .last = offset + maxsize, };
    *d = (struct data) { .offset = offset, .count = maxsize, .flags = flags,
                         .remaining = r, };
    if (nbd_aio_pread_structured (nbd, buf, sizeof buf, offset,
                                  (nbd_chunk_callback) { .callback = read_chunk, .user_data = d },
                                  (nbd_completion_callback) { .callback = read_verify, .user_data = d, .free = free },
                                  flags) == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  while (nbd_aio_in_flight (nbd) > 0) {
    int64_t cookie = nbd_aio_peek_command_completed (nbd);

    if (cookie == -1) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    if (cookie == 0) {
      if (nbd_poll (nbd, -1) == -1) {
        fprintf (stderr, "%s\n", nbd_get_error ());
        exit (EXIT_FAILURE);
      }
    }
    else
      nbd_aio_command_completed (nbd, cookie);
  }

  if (nbd_shutdown (nbd, 0) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  nbd_close (nbd);

  printf ("totals:\n");
  printf (" data chunks: %10d\n", total_data_chunks);
  printf (" data bytes:  %10" PRId64 "\n", total_data_bytes);
  printf (" hole chunks: %10d\n", total_hole_chunks);
  printf (" hole bytes:  %10" PRId64 "\n", total_hole_bytes);
  printf (" all chunks:  %10d\n", total_chunks);
  printf (" df reads:    %10d\n", total_df_reads);
  printf (" reads:       %10d\n", total_reads);
  printf (" bytes read:  %10" PRId64 "\n", total_bytes);
  printf (" compliant:   %10d\n", total_success);

  exit (EXIT_SUCCESS);
}
