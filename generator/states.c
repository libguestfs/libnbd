/* nbd client library in userspace: state machine
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

/* This isn't "real" C code.  It is read by the generator, parsed, and
 * put into generated files.  Also it won't make much sense unless you
 * read the generator state machine and documentation in
 * generator/generator first.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "internal.h"

/* Uncomment this to dump received protocol packets to stderr. */
/*#define DUMP_PACKETS 1*/

static int
recv_into_rbuf (struct nbd_handle *h)
{
  ssize_t r;
  char buf[BUFSIZ];
  void *rbuf;
  size_t rlen;

  if (h->rlen == 0)
    return 0;                   /* move to next state */

  /* As a special case h->rbuf is allowed to be NULL, meaning
   * throw away the data.
   */
  if (h->rbuf) {
    rbuf = h->rbuf;
    rlen = h->rlen;
  }
  else {
    rbuf = &buf;
    rlen = h->rlen > sizeof buf ? sizeof buf : h->rlen;
  }

  r = h->sock->ops->recv (h, h->sock, rbuf, rlen);
  if (r == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return 1;                 /* more data */
    /* sock->ops->recv called set_error already. */
    return -1;
  }
  if (r == 0) {
    set_error (0, "recv: server disconnected unexpectedly");
    return -1;
  }
#ifdef DUMP_PACKETS
  if (h->rbuf != NULL)
    nbd_internal_hexdump (h->rbuf, r, stderr);
#endif
  if (h->rbuf)
    h->rbuf += r;
  h->rlen -= r;
  if (h->rlen == 0)
    return 0;                   /* move to next state */
  else
    return 1;                   /* more data */
}

static int
send_from_wbuf (struct nbd_handle *h)
{
  ssize_t r;

  if (h->wlen == 0)
    goto next_state;
  r = h->sock->ops->send (h, h->sock, h->wbuf, h->wlen, h->wflags);
  if (r == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK)
      return 1;                 /* more data */
    /* sock->ops->send called set_error already. */
    return -1;
  }
  h->wbuf += r;
  h->wlen -= r;
  if (h->wlen == 0)
    goto next_state;
  else
    return 1;                   /* more data */

 next_state:
  h->wflags = 0;                /* reset this when moving to next state */
  return 0;                     /* move to next state */
}

/* Forcefully fail any remaining in-flight commands in list */
void abort_commands (struct nbd_handle *h,
                     struct command **list)
{
  struct command *next, *cmd;

  for (cmd = *list, *list = NULL; cmd != NULL; cmd = next) {
    bool retire = cmd->type == NBD_CMD_DISC;

    next = cmd->next;
    if (CALLBACK_IS_NOT_NULL (cmd->cb.completion)) {
      int error = cmd->error ? cmd->error : ENOTCONN;
      int r;

      assert (cmd->type != NBD_CMD_DISC);
      r = CALL_CALLBACK (cmd->cb.completion, &error);
      switch (r) {
      case -1:
        if (error)
          cmd->error = error;
        break;
      case 1:
        retire = true;
        break;
      }
    }
    if (cmd->error == 0)
      cmd->error = ENOTCONN;
    if (retire)
      nbd_internal_retire_and_free_command (cmd);
    else {
      cmd->next = NULL;
      if (h->cmds_done_tail)
        h->cmds_done_tail->next = cmd;
      else {
        assert (h->cmds_done == NULL);
        h->cmds_done = cmd;
      }
      h->cmds_done_tail = cmd;
    }
  }
}

STATE_MACHINE {
 READY:
  if (h->cmds_to_issue)
    SET_NEXT_STATE (%ISSUE_COMMAND.START);
  else {
    assert (h->sock);
    if (h->sock->ops->pending && h->sock->ops->pending (h->sock))
      SET_NEXT_STATE (%REPLY.START);
  }
  return 0;

 DEAD:
  /* The caller should have used set_error() before reaching here */
  assert (nbd_get_error ());
  abort_commands (h, &h->cmds_to_issue);
  abort_commands (h, &h->cmds_in_flight);
  h->in_flight = 0;
  if (h->sock) {
    h->sock->ops->close (h->sock);
    h->sock = NULL;
  }
  return -1;

 CLOSED:
  abort_commands (h, &h->cmds_to_issue);
  abort_commands (h, &h->cmds_in_flight);
  h->in_flight = 0;
  if (h->sock) {
    h->sock->ops->close (h->sock);
    h->sock = NULL;
  }
  return 0;

} /* END STATE MACHINE */
