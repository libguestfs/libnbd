/* NBD client library in userspace
 * Copyright (C) 2013-2022 Red Hat Inc.
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
#include <errno.h>

#include "internal.h"
#include "minmax.h"

/* Reset connection data.  Called after swapping export name, after
 * failed OPT_GO/OPT_INFO, or after successful OPT_STARTTLS.
 */
void
nbd_internal_reset_size_and_flags (struct nbd_handle *h)
{
  h->exportsize = 0;
  h->eflags = 0;
  h->block_minimum = 0;
  h->block_preferred = 0;
  h->block_maximum = 0;
  h->payload_maximum = 0;
  free (h->canonical_name);
  h->canonical_name = NULL;
  free (h->description);
  h->description = NULL;
}

/* Set the export size and eflags on the handle, validating them.
 * This is called from the state machine when either the newstyle or
 * oldstyle negotiation reaches the point that these are available.
 */
int
nbd_internal_set_size_and_flags (struct nbd_handle *h,
                                 uint64_t exportsize, uint16_t eflags)
{
  debug (h, "exportsize: %" PRIu64 " eflags: 0x%" PRIx16, exportsize, eflags);

  if (eflags == 0) {
    set_error (EINVAL, "handshake: invalid eflags == 0 from server");
    return -1;
  }

  if (eflags & NBD_FLAG_SEND_DF && !h->structured_replies) {
    debug (h, "server lacks structured replies, ignoring claim of df");
    eflags &= ~NBD_FLAG_SEND_DF;
  }

  if (eflags & NBD_FLAG_SEND_FAST_ZERO &&
      !(eflags & NBD_FLAG_SEND_WRITE_ZEROES)) {
    debug (h, "server lacks write zeroes, ignoring claim of fast zero");
    eflags &= ~NBD_FLAG_SEND_FAST_ZERO;
  }

  if (h->request_meta &&
      (!h->structured_replies || h->request_meta_contexts.len == 0)) {
    assert (h->meta_contexts.len == 0);
    h->meta_valid = true;
  }

  h->exportsize = exportsize;
  h->eflags = eflags;
  return 0;
}

/* Set the block size constraints on the handle, validating them.
 * This is called from the state machine if newstyle negotiation encounters
 * a server advertisement.
 */
int
nbd_internal_set_block_size (struct nbd_handle *h, uint32_t min,
                             uint32_t pref, uint32_t max)
{
  debug (h, "server block size constraints: min: %u preferred: %u max: %u",
         min, pref, max);
  /* NBD spec recommends:
   *  min and pref are power of 2
   *  min <= MIN(pref, 64k)
   *  pref >= 512
   *  max >= MIN(MAX(pref, 1M), exportsize)
   *  max is either multiple of min or 0xffffffff
   *  exportsize is multiple of min
   * At the point of this call, we don't necessarily know exportsize yet;
   * but we ignore server advertisement if any other constraints are wrong.
   */
  if (!min || min > 64*1024 || min > pref || pref < 512 || pref > max)
    goto ignore;
  if ((min & (min - 1)) != 0 || (pref & (pref - 1)) != 0)
    goto ignore;
  if (max != 0xffffffffU && max % min != 0)
    goto ignore;

  h->block_minimum = min;
  h->block_preferred = pref;
  h->block_maximum = max;
  return 0;

 ignore:
  debug (h, "ignoring improper server size constraints");
  return 0; /* Use return -1 if we want to reject such servers */
}

/* Set the payload size constraint on the handle.
 * This is called from the state machine after all other information
 * about an export is available, regardless of oldstyle or newstyle.
 */
void
nbd_internal_set_payload (struct nbd_handle *h)
{
  /* The NBD protocol says we should not assume the server will allow
   * more than 32M payload if it did not advertise anything.  If it
   * advertised more than 64M, we still cap at the maximum buffer size
   * we are willing to allocate as a client; if it advertised smaller
   * than 1M (presumably because the export itself was small), the NBD
   * protocol says we can still send payloads that large.
   */
  if (h->block_maximum)
    h->payload_maximum = MIN (MAX_REQUEST_SIZE,
                              MAX (1024 * 1024, h->block_maximum));
  else
    h->payload_maximum = 32 * 1024 * 1024;
}

static int
get_flag (struct nbd_handle *h, uint16_t flag)
{
  if (h->eflags == 0) {
    set_error (EINVAL, "server has not returned export flags, "
               "you need to connect to the server first");
    return -1;
  }

  return (h->eflags & flag) != 0;
}

int
nbd_unlocked_is_read_only (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_READ_ONLY);
}

int
nbd_unlocked_can_flush (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_SEND_FLUSH);
}

int
nbd_unlocked_can_fua (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_SEND_FUA);
}

int
nbd_unlocked_is_rotational (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_ROTATIONAL);
}

int
nbd_unlocked_can_trim (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_SEND_TRIM);
}

int
nbd_unlocked_can_zero (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_SEND_WRITE_ZEROES);
}

int
nbd_unlocked_can_fast_zero (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_SEND_FAST_ZERO);
}

int
nbd_unlocked_can_df (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_SEND_DF);
}

int
nbd_unlocked_can_multi_conn (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_CAN_MULTI_CONN);
}

int
nbd_unlocked_can_cache (struct nbd_handle *h)
{
  return get_flag (h, NBD_FLAG_SEND_CACHE);
}

int
nbd_unlocked_can_meta_context (struct nbd_handle *h, const char *name)
{
  size_t i;

  if (h->request_meta_contexts.len && !h->meta_valid) {
    set_error (EINVAL, "need a successful server meta context request first");
    return -1;
  }

  for (i = 0; i < h->meta_contexts.len; ++i)
    if (strcmp (h->meta_contexts.ptr[i].name, name) == 0)
      return 1;
  return 0;
}

int64_t
nbd_unlocked_get_size (struct nbd_handle *h)
{
  /* exportsize is only valid when we've read both the eflags and the
   * exportsize.  See comment in lib/internal.h.
   */
  if (h->eflags == 0) {
    set_error (EINVAL, "server has not returned export size, "
               "you need to connect to the server first");
    return -1;
  }

  return h->exportsize;
}

int64_t
nbd_unlocked_get_block_size (struct nbd_handle *h, int type)
{
  if (h->eflags == 0) {
    set_error (EINVAL, "server has not returned export flags, "
               "you need to connect to the server first");
    return -1;
  }

  switch (type) {
  case LIBNBD_SIZE_MINIMUM:
    return h->block_minimum;
  case LIBNBD_SIZE_PREFERRED:
    return h->block_preferred;
  case LIBNBD_SIZE_MAXIMUM:
    return h->block_maximum;
  case LIBNBD_SIZE_PAYLOAD:
    return h->payload_maximum;
  }
  return 0;
}
