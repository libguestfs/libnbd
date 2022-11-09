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
#include <assert.h>
#include <limits.h>

#include "internal.h"

static int
wait_for_command (struct nbd_handle *h, int64_t cookie)
{
  int r;

  while ((r = nbd_unlocked_aio_command_completed (h, cookie)) == 0) {
    if (nbd_unlocked_poll (h, -1) == -1)
      return -1;
  }

  return r == -1 ? -1 : 0;
}

/* Issue a read command and wait for the reply. */
int
nbd_unlocked_pread (struct nbd_handle *h, void *buf,
                    size_t count, uint64_t offset, uint32_t flags)
{
  int64_t cookie;
  nbd_completion_callback c = NBD_NULL_COMPLETION;

  cookie = nbd_unlocked_aio_pread (h, buf, count, offset, &c, flags);
  if (cookie == -1)
    return -1;

  return wait_for_command (h, cookie);
}

/* Issue a read command with callbacks and wait for the reply. */
int
nbd_unlocked_pread_structured (struct nbd_handle *h, void *buf,
                               size_t count, uint64_t offset,
                               nbd_chunk_callback *chunk,
                               uint32_t flags)
{
  int64_t cookie;
  nbd_completion_callback c = NBD_NULL_COMPLETION;

  cookie = nbd_unlocked_aio_pread_structured (h, buf, count, offset,
                                              chunk, &c, flags);
  if (cookie == -1)
    return -1;

  assert (CALLBACK_IS_NULL (*chunk));
  return wait_for_command (h, cookie);
}

/* Issue a write command and wait for the reply. */
int
nbd_unlocked_pwrite (struct nbd_handle *h, const void *buf,
                     size_t count, uint64_t offset, uint32_t flags)
{
  int64_t cookie;
  nbd_completion_callback c = NBD_NULL_COMPLETION;

  cookie = nbd_unlocked_aio_pwrite (h, buf, count, offset, &c, flags);
  if (cookie == -1)
    return -1;

  return wait_for_command (h, cookie);
}

/* Issue a flush command and wait for the reply. */
int
nbd_unlocked_flush (struct nbd_handle *h, uint32_t flags)
{
  int64_t cookie;
  nbd_completion_callback c = NBD_NULL_COMPLETION;

  cookie = nbd_unlocked_aio_flush (h, &c, flags);
  if (cookie == -1)
    return -1;

  return wait_for_command (h, cookie);
}

/* Issue a trim command and wait for the reply. */
int
nbd_unlocked_trim (struct nbd_handle *h,
                   uint64_t count, uint64_t offset, uint32_t flags)
{
  int64_t cookie;
  nbd_completion_callback c = NBD_NULL_COMPLETION;

  cookie = nbd_unlocked_aio_trim (h, count, offset, &c, flags);
  if (cookie == -1)
    return -1;

  return wait_for_command (h, cookie);
}

/* Issue a cache command and wait for the reply. */
int
nbd_unlocked_cache (struct nbd_handle *h,
                    uint64_t count, uint64_t offset, uint32_t flags)
{
  int64_t cookie;
  nbd_completion_callback c = NBD_NULL_COMPLETION;

  cookie = nbd_unlocked_aio_cache (h, count, offset, &c, flags);
  if (cookie == -1)
    return -1;

  return wait_for_command (h, cookie);
}

/* Issue a zero command and wait for the reply. */
int
nbd_unlocked_zero (struct nbd_handle *h,
                   uint64_t count, uint64_t offset, uint32_t flags)
{
  int64_t cookie;
  nbd_completion_callback c = NBD_NULL_COMPLETION;

  cookie = nbd_unlocked_aio_zero (h, count, offset, &c, flags);
  if (cookie == -1)
    return -1;

  return wait_for_command (h, cookie);
}

/* Issue a block status command and wait for the reply. */
int
nbd_unlocked_block_status (struct nbd_handle *h,
                           uint64_t count, uint64_t offset,
                           nbd_extent_callback *extent,
                           uint32_t flags)
{
  int64_t cookie;
  nbd_completion_callback c = NBD_NULL_COMPLETION;

  cookie = nbd_unlocked_aio_block_status (h, count, offset, extent, &c, flags);
  if (cookie == -1)
    return -1;

  assert (CALLBACK_IS_NULL (*extent));
  return wait_for_command (h, cookie);
}

/* count_err represents the errno to return if bounds check fail */
int64_t
nbd_internal_command_common (struct nbd_handle *h,
                             uint16_t flags, uint16_t type,
                             uint64_t offset, uint64_t count, int count_err,
                             void *data, struct command_cb *cb)
{
  struct command *cmd;

  if (h->disconnect_request) {
      set_error (EINVAL, "cannot request more commands after NBD_CMD_DISC");
      goto err;
  }
  if (h->in_flight == INT_MAX) {
      set_error (ENOMEM, "too many commands already in flight");
      goto err;
  }

  if (count_err) {
    if ((h->strict & LIBNBD_STRICT_ZERO_SIZE) && count == 0) {
      set_error (EINVAL, "count cannot be 0");
      goto err;
    }

    if ((h->strict & LIBNBD_STRICT_BOUNDS) &&
        (offset > h->exportsize || count > h->exportsize - offset)) {
      set_error (count_err, "request out of bounds");
      goto err;
    }

    if (h->block_minimum && (h->strict & LIBNBD_STRICT_ALIGN) &&
        (offset | count) & (h->block_minimum - 1)) {
      set_error (EINVAL, "request is unaligned");
      goto err;
    }
  }

  switch (type) {
    /* Commands which send or receive data are limited to MAX_REQUEST_SIZE. */
  case NBD_CMD_WRITE:
    if (h->strict & LIBNBD_STRICT_PAYLOAD && count > h->payload_maximum) {
      set_error (ERANGE,
                 "request too large: maximum payload size is %" PRIu32,
                 h->payload_maximum);
      goto err;
    }
    /* fallthrough */
  case NBD_CMD_READ:
    if (count > MAX_REQUEST_SIZE) {
      set_error (ERANGE, "request too large: maximum request size is %d",
                 MAX_REQUEST_SIZE);
      goto err;
    }
    break;

    /* Other commands are currently limited by the 32 bit field in the
     * command structure on the wire, but in future we hope to support
     * 64 bit values here with a change to the NBD protocol which is
     * being discussed upstream.
     */
  default:
    if (count > UINT32_MAX) {
      set_error (ERANGE, "request too large: maximum request size is %" PRIu32,
                 UINT32_MAX);
      goto err;
    }
    break;
  }

  cmd = calloc (1, sizeof *cmd);
  if (cmd == NULL) {
    set_error (errno, "calloc");
    goto err;
  }
  cmd->flags = flags;
  cmd->type = type;
  cmd->cookie = h->unique++;
  cmd->offset = offset;
  cmd->count = count;
  cmd->data = data;
  if (cb)
    cmd->cb = *cb;

  /* For NBD_CMD_READ, cmd->data defaults to being pre-zeroed in the
   * prologue created by the generator.  Thus, if a (non-compliant)
   * server with structured replies fails to send back sufficient data
   * to cover the whole buffer, we still behave as if it had sent
   * zeroes for those portions, rather than leaking any uninitialized
   * data, and without having to complicate our state machine to track
   * which portions of the read buffer were actually populated.  But
   * if the user opts in to disabling set_pread_initialize, then we
   * need to memset zeroes as they are read (and the user gets their
   * own garbage back in the case of a non-compliant server).
   */
  cmd->initialized = h->pread_initialize;

  /* Add the command to the end of the queue. Kick the state machine
   * if there is no other command being processed, otherwise, it will
   * be handled automatically on a future cycle around to READY.
   * Beyond this point, we have to return a cookie to the user, since
   * we are queuing the command, even if kicking the state machine
   * detects a failure.  Not reporting a state machine failure here is
   * okay - any caller of an async command will be calling more API to
   * await results, and will eventually learn that the machine has
   * moved on to DEAD at that time.
   */
  h->in_flight++;
  if (h->cmds_to_issue != NULL) {
    assert (nbd_internal_is_state_processing (get_next_state (h)));
    h->cmds_to_issue_tail = h->cmds_to_issue_tail->next = cmd;
  }
  else {
    assert (h->cmds_to_issue_tail == NULL);
    h->cmds_to_issue = h->cmds_to_issue_tail = cmd;
    if (nbd_internal_is_state_ready (get_next_state (h)) &&
        nbd_internal_run (h, cmd_issue) == -1)
      debug (h, "command queued, ignoring state machine failure");
  }

  return cmd->cookie;

 err:
  /* Since we did not queue the command, we must free the callbacks. */
  if (cb) {
    if (type == NBD_CMD_BLOCK_STATUS)
      FREE_CALLBACK (cb->fn.extent);
    if (type == NBD_CMD_READ)
      FREE_CALLBACK (cb->fn.chunk);
    FREE_CALLBACK (cb->completion);
  }
  return -1;
}

int64_t
nbd_unlocked_aio_pread (struct nbd_handle *h, void *buf,
                        size_t count, uint64_t offset,
                        nbd_completion_callback *completion,
                        uint32_t flags)
{
  struct command_cb cb = { .completion = *completion };

  SET_CALLBACK_TO_NULL (*completion);
  return nbd_internal_command_common (h, flags, NBD_CMD_READ, offset, count,
                                      EINVAL, buf, &cb);
}

int64_t
nbd_unlocked_aio_pread_structured (struct nbd_handle *h, void *buf,
                                   size_t count, uint64_t offset,
                                   nbd_chunk_callback *chunk,
                                   nbd_completion_callback *completion,
                                   uint32_t flags)
{
  struct command_cb cb = { .fn.chunk = *chunk,
                           .completion = *completion };

  if (h->strict & LIBNBD_STRICT_COMMANDS) {
    if ((flags & LIBNBD_CMD_FLAG_DF) != 0 &&
        nbd_unlocked_can_df (h) != 1) {
      set_error (EINVAL, "server does not support the DF flag");
      return -1;
    }
  }

  SET_CALLBACK_TO_NULL (*chunk);
  SET_CALLBACK_TO_NULL (*completion);
  return nbd_internal_command_common (h, flags, NBD_CMD_READ, offset, count,
                                      EINVAL, buf, &cb);
}

int64_t
nbd_unlocked_aio_pwrite (struct nbd_handle *h, const void *buf,
                         size_t count, uint64_t offset,
                         nbd_completion_callback *completion,
                         uint32_t flags)
{
  struct command_cb cb = { .completion = *completion };

  if (h->strict & LIBNBD_STRICT_COMMANDS) {
    if (nbd_unlocked_is_read_only (h) == 1) {
      set_error (EPERM, "server does not support write operations");
      return -1;
    }

    if ((flags & LIBNBD_CMD_FLAG_FUA) != 0 &&
        nbd_unlocked_can_fua (h) != 1) {
      set_error (EINVAL, "server does not support the FUA flag");
      return -1;
    }
  }

  SET_CALLBACK_TO_NULL (*completion);
  return nbd_internal_command_common (h, flags, NBD_CMD_WRITE, offset, count,
                                      ENOSPC, (void *) buf, &cb);
}

int64_t
nbd_unlocked_aio_flush (struct nbd_handle *h,
                        nbd_completion_callback *completion,
                        uint32_t flags)
{
  struct command_cb cb = { .completion = *completion };

  if (h->strict & LIBNBD_STRICT_COMMANDS) {
    if (nbd_unlocked_can_flush (h) != 1) {
      set_error (EINVAL, "server does not support flush operations");
      return -1;
    }
  }

  SET_CALLBACK_TO_NULL (*completion);
  return nbd_internal_command_common (h, flags, NBD_CMD_FLUSH, 0, 0,
                                      0, NULL, &cb);
}

int64_t
nbd_unlocked_aio_trim (struct nbd_handle *h,
                       uint64_t count, uint64_t offset,
                       nbd_completion_callback *completion,
                       uint32_t flags)
{
  struct command_cb cb = { .completion = *completion };

  if (h->strict & LIBNBD_STRICT_COMMANDS) {
    if (nbd_unlocked_can_trim (h) != 1) {
      set_error (EINVAL, "server does not support trim operations");
      return -1;
    }
    if (nbd_unlocked_is_read_only (h) == 1) {
      set_error (EPERM, "server does not support write operations");
      return -1;
    }

    if ((flags & LIBNBD_CMD_FLAG_FUA) != 0 &&
        nbd_unlocked_can_fua (h) != 1) {
      set_error (EINVAL, "server does not support the FUA flag");
      return -1;
    }
  }

  SET_CALLBACK_TO_NULL (*completion);
  return nbd_internal_command_common (h, flags, NBD_CMD_TRIM, offset, count,
                                      ENOSPC, NULL, &cb);
}

int64_t
nbd_unlocked_aio_cache (struct nbd_handle *h,
                        uint64_t count, uint64_t offset,
                        nbd_completion_callback *completion,
                        uint32_t flags)
{
  struct command_cb cb = { .completion = *completion };

  if (h->strict & LIBNBD_STRICT_COMMANDS) {
    /* Actually according to the NBD protocol document, servers do exist
     * that support NBD_CMD_CACHE but don't advertise the
     * NBD_FLAG_SEND_CACHE bit, but we ignore those.
     */
    if (nbd_unlocked_can_cache (h) != 1) {
      set_error (EINVAL, "server does not support cache operations");
      return -1;
    }
  }

  SET_CALLBACK_TO_NULL (*completion);
  return nbd_internal_command_common (h, flags, NBD_CMD_CACHE, offset, count,
                                      EINVAL, NULL, &cb);
}

int64_t
nbd_unlocked_aio_zero (struct nbd_handle *h,
                       uint64_t count, uint64_t offset,
                       nbd_completion_callback *completion,
                       uint32_t flags)
{
  struct command_cb cb = { .completion = *completion };

  if (h->strict & LIBNBD_STRICT_COMMANDS) {
    if (nbd_unlocked_can_zero (h) != 1) {
      set_error (EINVAL, "server does not support zero operations");
      return -1;
    }
    if (nbd_unlocked_is_read_only (h) == 1) {
      set_error (EPERM, "server does not support write operations");
      return -1;
    }

    if ((flags & LIBNBD_CMD_FLAG_FUA) != 0 &&
        nbd_unlocked_can_fua (h) != 1) {
      set_error (EINVAL, "server does not support the FUA flag");
      return -1;
    }

    if ((flags & LIBNBD_CMD_FLAG_FAST_ZERO) != 0 &&
        nbd_unlocked_can_fast_zero (h) != 1) {
      set_error (EINVAL, "server does not support the fast zero flag");
      return -1;
    }
  }

  SET_CALLBACK_TO_NULL (*completion);
  return nbd_internal_command_common (h, flags, NBD_CMD_WRITE_ZEROES, offset,
                                      count, ENOSPC, NULL, &cb);
}

int64_t
nbd_unlocked_aio_block_status (struct nbd_handle *h,
                               uint64_t count, uint64_t offset,
                               nbd_extent_callback *extent,
                               nbd_completion_callback *completion,
                               uint32_t flags)
{
  struct command_cb cb = { .fn.extent = *extent,
                           .completion = *completion };

  if (h->strict & LIBNBD_STRICT_COMMANDS) {
    if (!h->structured_replies) {
      set_error (ENOTSUP, "server does not support structured replies");
      return -1;
    }

    if (!h->meta_valid || h->meta_contexts.len == 0) {
      set_error (ENOTSUP, "did not negotiate any metadata contexts, "
                 "either you did not call nbd_add_meta_context before "
                 "connecting or the server does not support it");
      return -1;
    }
  }

  SET_CALLBACK_TO_NULL (*extent);
  SET_CALLBACK_TO_NULL (*completion);
  return nbd_internal_command_common (h, flags, NBD_CMD_BLOCK_STATUS, offset,
                                      count, EINVAL, NULL, &cb);
}
