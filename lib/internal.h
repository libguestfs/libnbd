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

#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>

#include <pthread.h>

#include "libnbd.h"
#include "nbd-protocol.h"

#include "states.h"
#include "unlocked.h"

/* XXX This is the same as nbdkit, but probably it should be detected
 * from the server or made configurable.
 */
#define MAX_REQUEST_SIZE (64 * 1024 * 1024)

struct socket;

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

  char *export_name;            /* Export name, never NULL. */

  /* TLS settings. */
  int tls;                      /* 0 = disable, 1 = enable, 2 = require */
  char *tls_certificates;       /* Certs dir, NULL = use default path */
  char *tls_username;           /* Username, NULL = use current username */
  char *tls_psk_file;           /* PSK filename, NULL = no PSK */

  /* Export size and per-export flags, received during handshake.  NB:
   * These are *both* *only* valid if eflags != 0.  This is because
   * all servers should set NBD_FLAG_HAS_FLAGS, so eflags should
   * always be != 0, and we set both fields at the same time.
   */
  uint64_t exportsize;
  uint16_t eflags;

  int64_t unique;               /* Used for generating handle numbers. */

  /* For debugging. */
  bool debug;
  int64_t debug_id;
  void (*debug_fn) (int64_t, const char *, const char *);
};

/* This corresponds to a single socket connection to a remote server.
 * Usually there is one of these in the handle, but for multi_conn
 * there may be several.
 */
struct nbd_connection {
  struct nbd_handle *h;         /* Parent handle. */

  /* To avoid leaking addresses in debug messages, and to make debug
   * easier to read, give this a unique ID used in debug.
   */
  int64_t id;

  enum state state;             /* State machine. */

  /* The socket or a wrapper if using GnuTLS. */
  struct socket *sock;

  /* Generic way to read into a buffer - set rbuf to point to a
   * buffer, rlen to the amount of data you expect, and in the state
   * machine call recv_into_rbuf.
   */
  void *rbuf;
  size_t rlen;

  /* As above, but for writing using send_from_wbuf. */
  const void *wbuf;
  size_t wlen;

  /* Static buffer used for short amounts of data, such as handshake
   * and commands.
   */
  union {
    struct nbd_new_handshake handshake;
    struct nbd_new_option option;
    struct {
      struct nbd_fixed_new_option_reply option_reply;
      union {
        struct nbd_fixed_new_option_reply_info_export export;
      } payload;
    } or;
    struct nbd_request request;
    struct nbd_simple_reply simple_reply;
    uint32_t cflags;
    uint32_t len;
    uint16_t nrinfos;
  } sbuf;

  /* When connecting, this stores the socket address. */
  struct sockaddr_storage connaddr;
  socklen_t connaddrlen;

  /* When connecting to a local command, this points to the command
   * name.  A local copy is taken to simplify callers.  The PID is the
   * PID of the subprocess so we can wait on it when the connection is
   * closed.
   */
  char *command;
  pid_t pid;

  /* When connecting to TCP ports, these fields are used. */
  char *hostname, *port;
  struct addrinfo hints;
  struct addrinfo *result, *rp;

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

struct socket_ops {
  ssize_t (*recv) (struct socket *sock, void *buf, size_t len);
  ssize_t (*send) (struct socket *sock, const void *buf, size_t len);
  int (*get_fd) (struct socket *sock);
  int (*close) (struct socket *sock);
};

struct socket {
  union {
    int fd;
  } u;
  const struct socket_ops *ops;
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
extern void nbd_internal_debug (struct nbd_handle *h, const char *fs, ...);
#define debug(h, fs, ...)                               \
  do {                                                  \
    if ((h)->debug)                                     \
      nbd_internal_debug ((h), (fs), ##__VA_ARGS__);    \
  } while (0)

/* errors.c */
extern void nbd_internal_reset_error (const char *context);
extern const char *nbd_internal_get_error_context (void);
extern void nbd_internal_set_last_error (int errnum, char *error);
#define set_error(errnum, fs, ...)                                      \
  do {                                                                  \
    const char *_context =                                              \
      nbd_internal_get_error_context () ? : "unknown";                  \
    char *_errp;                                                        \
    int _r;                                                             \
    if ((errnum) == 0)                                                  \
      _r = asprintf (&_errp, "%s: " fs, _context, ##__VA_ARGS__);       \
    else                                                                \
      _r = asprintf (&_errp, "%s: " fs ": %s",                          \
                     _context, ##__VA_ARGS__, strerror ((errnum)));     \
    if (_r >= 0)                                                        \
      nbd_internal_set_last_error ((errnum), _errp);                    \
  } while (0)

/* protocol.c */
extern int nbd_internal_errno_of_nbd_error (uint32_t error);
extern const char *nbd_internal_name_of_nbd_cmd (uint16_t type);

/* socket.c */
struct socket *nbd_internal_socket_create (int fd);

/* states.c */
extern int nbd_internal_run (struct nbd_handle *h, struct nbd_connection *conn,
                             enum external_event ev);
extern const char *nbd_internal_state_short_string (enum state state);

/* utils.c */
extern void nbd_internal_hexdump (const void *data, size_t len, FILE *fp);

#endif /* LIBNBD_INTERNAL_H */
