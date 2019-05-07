/* nbd client library in userspace: internal definitions
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

#ifndef LIBNBD_INTERNAL_H
#define LIBNBD_INTERNAL_H

#include <pthread.h>

#include "libnbd.h"
#include "nbd-protocol.h"

#include "states.h"
#include "unlocked.h"

/* XXX This is the same as nbdkit, but probably it should be detected
 * from the server or made configurable.
 */
#define MAX_REQUEST_SIZE (64 * 1024 * 1024)

/* These correspond to the external events in generator/generator. */
enum external_event {
  notify_read,
  notify_write,
  cmd_create,
  cmd_connect,
  cmd_issue,
};

struct nbd_handle {
  /* Lock protecting concurrent access to either this handle or the
   * connections owned by the handle.
   */
  pthread_mutex_t lock;

  /* Connection(s).  Usually 1 but there may be several.  The length
   * of the list is multi_conn.  The elements are never NULL.  If a
   * connection is closed then it is replaced with a newly created
   * connection immediately.
   */
  struct nbd_connection **conns;
  unsigned multi_conn;

  char *export;                 /* Export name, never NULL. */
  int64_t unique;               /* Used for generating handle numbers. */
};

/* This corresponds to a single socket connection to a remote server.
 * Usually there is one of these in the handle, but for multi_conn
 * there may be several.
 */
struct nbd_connection {
  struct nbd_handle *h;         /* Parent handle. */

  enum state state;             /* State machine. */
  int fd;                       /* Socket. */

  /* Generic way to read into a buffer - set rbuf to point to a
   * buffer, rlen to the amount of data you expect, and in the state
   * machine call recv_into_rbuf.
   */
  void *rbuf;
  size_t rlen;

  /* As above, but for writing using send_from_wbuf. */
  const void *wbuf;
  size_t wlen;

  /* Buffer used for short amounts of data, such as handshake and
   * commands.
   */
  union {
    struct nbd_new_handshake handshake;
    struct nbd_new_option option;
    struct nbd_fixed_new_option_reply option_reply;
    struct nbd_request request;
    struct nbd_simple_reply simple_reply;
    uint32_t cflags;
    uint32_t len;
    uint16_t nrinfos;
  } sbuf;

  /* When connecting, this stores the socket address. */
  struct sockaddr_storage connaddr;
  socklen_t connaddrlen;

  /* Global flags from the server. */
  uint16_t gflags;

  /* When issuing a command, the first list contains commands waiting
   * to be issued.  The second list contains commands which have been
   * issued and waiting for replies.  The third list contains commands
   * which we have received replies, waiting for the main program to
   * acknowledge them.
   */
  struct command_in_flight *cmds_to_issue, *cmds_in_flight, *cmds_done;
};

struct command_in_flight {
  struct command_in_flight *next;
  uint16_t flags;
  uint16_t type;
  uint64_t handle;
  uint64_t offset;
  uint32_t count;
  void *data;
  uint32_t error;
};

/* debug.c */
/* XXX */
#define debug(fs, ...) fprintf (stderr, fs "\n", ##__VA_ARGS__)

/* errors.c */
/* XXX */
#include <string.h>
#define set_error(errno, fs, ...) do { fprintf (stderr, "libnbd: " fs, ##__VA_ARGS__); if (errno) fprintf (stderr, ": %s", strerror (errno)); fprintf (stderr, "\n"); } while (0)

/* protocol.c */
extern int nbd_internal_errno_of_nbd_error (uint32_t error);
extern const char *nbd_internal_name_of_nbd_cmd (uint16_t type);

/* states.c */
extern int nbd_internal_run (struct nbd_handle *h, struct nbd_connection *conn,
                             enum external_event ev);
extern const char *nbd_internal_state_short_string (enum state state);

#endif /* LIBNBD_INTERNAL_H */
