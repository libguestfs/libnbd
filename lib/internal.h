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
#include <stdatomic.h>
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

struct meta_context;
struct close_callback;
struct socket;
struct command_in_flight;

struct nbd_handle {
  /* Lock protecting concurrent access to the handle. */
  pthread_mutex_t lock;

  char *export_name;            /* Export name, never NULL. */

  /* TLS settings. */
  int tls;                      /* 0 = disable, 1 = enable, 2 = require */
  char *tls_certificates;       /* Certs dir, NULL = use default path */
  bool tls_verify_peer;         /* Verify the peer certificate. */
  char *tls_username;           /* Username, NULL = use current username */
  char *tls_psk_file;           /* PSK filename, NULL = no PSK */

  /* Desired metadata contexts. */
  char **request_meta_contexts;

  /* Global flags from the server. */
  uint16_t gflags;

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
  void *debug_data;
  int (*debug_fn) (void *, const char *, const char *);

  /* Linked list of close callbacks. */
  struct close_callback *close_callbacks;

  /* State machine.
   *
   * The actual current state is ‘state’.  ‘public_state’ is updated
   * before we release the lock.
   *
   * Note don't access these fields directly, use the SET_NEXT_STATE
   * macro in generator/states* code, or the set_next_state,
   * get_next_state and get_public_state macros in regular code.
   */
  _Atomic enum state public_state;
  enum state state;

  bool structured_replies;      /* If we negotiated NBD_OPT_STRUCTURED_REPLY */

  /* Linked list of negotiated metadata contexts. */
  struct meta_context *meta_contexts;

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
  int wflags;

  /* Static buffer used for short amounts of data, such as handshake
   * and commands.
   */
  union {
    struct nbd_old_handshake old_handshake;
    struct nbd_new_handshake new_handshake;
    struct nbd_new_option option;
    struct {
      struct nbd_fixed_new_option_reply option_reply;
      union {
        struct nbd_fixed_new_option_reply_info_export export;
        struct {
          struct nbd_fixed_new_option_reply_meta_context context;
          char str[64];
        }  __attribute__((packed)) context;
      } payload;
    }  __attribute__((packed)) or;
    struct nbd_export_name_option_reply export_name_reply;
    struct nbd_simple_reply simple_reply;
    struct {
      struct nbd_structured_reply structured_reply;
      union {
        struct nbd_structured_reply_offset_data offset_data;
        struct nbd_structured_reply_offset_hole offset_hole;
        struct nbd_structured_reply_error error;
      } payload;
    }  __attribute__((packed)) sr;
    uint32_t cflags;
    uint32_t len;
    uint16_t nrinfos;
    uint32_t nrqueries;
  } sbuf;

  /* Issuing a command must use a buffer separate from sbuf, for the
   * case when we interrupt a request to service a reply.
   */
  struct nbd_request request;
  bool in_write_payload;

  /* When connecting, this stores the socket address. */
  struct sockaddr_storage connaddr;
  socklen_t connaddrlen;

  /* When connecting to a local command, this points to the argv.  A
   * local copy is taken to simplify callers.  The PID is the PID of
   * the subprocess so we can wait on it when the connection is
   * closed.
   */
  char **argv;
  pid_t pid;

  /* When connecting to Unix domain socket. */
  char *unixsocket;

  /* When connecting to TCP ports, these fields are used. */
  char *hostname, *port;
  struct addrinfo hints;
  struct addrinfo *result, *rp;

  /* When sending metadata contexts, this is used. */
  size_t querynum;

  /* When receiving block status, this is used. */
  uint32_t *bs_entries;

  /* When issuing a command, the first list contains commands waiting
   * to be issued.  The second list contains commands which have been
   * issued and waiting for replies.  The third list contains commands
   * which we have received replies, waiting for the main program to
   * acknowledge them.
   */
  struct command_in_flight *cmds_to_issue, *cmds_in_flight, *cmds_done;
};

struct meta_context {
  struct meta_context *next;    /* Linked list. */
  char *name;                   /* Name of meta context. */
  uint32_t context_id;          /* Context ID negotiated with the server. */
};

struct close_callback {
  struct close_callback *next;  /* Linked list. */
  nbd_close_callback cb;        /* Function. */
  void *data;                   /* Data. */
};

struct socket_ops {
  ssize_t (*recv) (struct nbd_handle *h,
                   struct socket *sock, void *buf, size_t len);
  ssize_t (*send) (struct nbd_handle *h,
                   struct socket *sock, const void *buf, size_t len, int flags);
  bool (*pending) (struct socket *sock);
  int (*get_fd) (struct socket *sock);
  int (*close) (struct socket *sock);
};

struct socket {
  union {
    int fd;
    struct {
      /* These are ‘void *’ so that we don't need to include gnutls
       * headers from this file.
       */
      void *session;            /* really gnutls_session_t */
      void *pskcreds;           /* really gnutls_psk_client_credentials_t */
      void *xcreds;             /* really gnutls_certificate_credentials_t */
      struct socket *oldsock;
    } tls;
  } u;
  const struct socket_ops *ops;
};

typedef int (*extent_fn) (void *data, const char *metacontext, uint64_t offset, uint32_t *entries, size_t nr_entries);

struct command_in_flight {
  struct command_in_flight *next;
  uint16_t flags;
  uint16_t type;
  uint64_t handle;
  uint64_t offset;
  uint32_t count;
  void *data; /* Buffer for read/write, opaque for block status */
  extent_fn extent_fn;
  uint32_t error; /* Local errno value */
};

/* crypto.c */
extern struct socket *nbd_internal_crypto_create_session (struct nbd_handle *, struct socket *oldsock);
extern bool nbd_internal_crypto_is_reading (struct nbd_handle *);
extern int nbd_internal_crypto_handshake (struct nbd_handle *);
extern void nbd_internal_crypto_debug_tls_enabled (struct nbd_handle *);

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

/* flags.c */
extern int nbd_internal_set_size_and_flags (struct nbd_handle *h,
                                            uint64_t exportsize,
                                            uint16_t eflags);

/* is-state.c */
extern bool nbd_internal_is_state_created (enum state state);
extern bool nbd_internal_is_state_connecting (enum state state);
extern bool nbd_internal_is_state_ready (enum state state);
extern bool nbd_internal_is_state_processing (enum state state);
extern bool nbd_internal_is_state_dead (enum state state);
extern bool nbd_internal_is_state_closed (enum state state);

/* protocol.c */
extern int nbd_internal_errno_of_nbd_error (uint32_t error);
extern const char *nbd_internal_name_of_nbd_cmd (uint16_t type);

/* rw.c */
extern int64_t nbd_internal_command_common (struct nbd_handle *h,
                                            uint16_t flags, uint16_t type,
                                            uint64_t offset, uint64_t count,
                                            void *data, extent_fn extent);

/* socket.c */
struct socket *nbd_internal_socket_create (int fd);

/* states.c */
extern int nbd_internal_run (struct nbd_handle *h, enum external_event ev);
extern const char *nbd_internal_state_short_string (enum state state);
extern enum state_group nbd_internal_state_group (enum state state);
extern enum state_group nbd_internal_state_group_parent (enum state_group group);

#define set_next_state(h,next_state) ((h)->state) = (next_state)
#define get_next_state(h) ((h)->state)
#define get_public_state(h) ((h)->public_state)

/* utils.c */
extern void nbd_internal_hexdump (const void *data, size_t len, FILE *fp);
extern size_t nbd_internal_string_list_length (char **argv);
extern char **nbd_internal_copy_string_list (char **argv);
extern void nbd_internal_free_string_list (char **argv);

#endif /* LIBNBD_INTERNAL_H */
