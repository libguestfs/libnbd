/* nbd client library in userspace: internal definitions
 * Copyright (C) 2013-2020 Red Hat Inc.
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

#ifdef HAVE_STDATOMIC_H
#include <stdatomic.h>
#else
/* Rely on ints being atomic enough on the platform. */
#define _Atomic /**/
#endif

#include <stdbool.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>

#include <pthread.h>

#include "libnbd.h"
#include "nbd-protocol.h"

#include "byte-swapping.h"
#include "states.h"
#include "unlocked.h"

/* Define unlikely macro, but only for GCC.  These are used to move
 * debug and error handling code out of hot paths, making the hot path
 * into common functions use less instruction cache.
 */
#if defined(__GNUC__)
#define unlikely(x) __builtin_expect (!!(x), 0)
#define if_debug(h) if (unlikely ((h)->debug))
#else
#define unlikely(x) (x)
#define if_debug(h) if ((h)->debug)
#endif

/* MSG_MORE is an optimization.  If not present, ignore it. */
#ifndef MSG_MORE
#define MSG_MORE 0
#endif

/* XXX This is the same as nbdkit, but probably it should be detected
 * from the server (NBD_INFO_BLOCK_SIZE) or made configurable.
 */
#define MAX_REQUEST_SIZE (64 * 1024 * 1024)

struct meta_context;
struct socket;
struct command;

struct nbd_handle {
  /* Unique name assigned to this handle for debug messages
   * (to avoid having to print actual pointers).
   */
  char *hname;

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
  bool request_sr;
  char **request_meta_contexts;

  /* Allowed in URIs, see lib/uri.c. */
  uint32_t uri_allow_transports;
  int uri_allow_tls;
  bool uri_allow_local_file;

  /* Global flags from the server. */
  uint16_t gflags;

  /* Export size and per-export flags, received during handshake.  NB:
   * These are *both* *only* valid if eflags != 0.  This is because
   * all servers should set NBD_FLAG_HAS_FLAGS, so eflags should
   * always be != 0, and we set both fields at the same time.
   */
  uint64_t exportsize;
  uint16_t eflags;

  /* Flags set by the state machine to tell what protocol and whether
   * TLS was negotiated.
   */
  const char *protocol;
  bool tls_negotiated;

  int64_t unique;               /* Used for generating cookie numbers. */

  /* For debugging. */
  bool debug;
  nbd_debug_callback debug_callback;

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
          char str[NBD_MAX_STRING];
        }  __attribute__((packed)) context;
        char err_msg[NBD_MAX_STRING];
      } payload;
    }  __attribute__((packed)) or;
    struct nbd_export_name_option_reply export_name_reply;
    struct nbd_simple_reply simple_reply;
    struct {
      struct nbd_structured_reply structured_reply;
      union {
        struct nbd_structured_reply_offset_data offset_data;
        struct nbd_structured_reply_offset_hole offset_hole;
        struct {
          struct nbd_structured_reply_error error;
          char msg[NBD_MAX_STRING]; /* Common to all error types */
          uint64_t offset; /* Only used for NBD_REPLY_TYPE_ERROR_OFFSET */
        } __attribute__((packed)) error;
      } payload;
    }  __attribute__((packed)) sr;
    uint16_t gflags;
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
  bool in_write_shutdown;

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

  /* When using systemd socket activation, this directory and socket
   * must be deleted, and the pid above must be killed.
   */
  char *sa_tmpdir;
  char *sa_sockpath;

  /* When connecting to TCP ports, these fields are used. */
  char *hostname, *port;
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int connect_errno;

  /* When sending metadata contexts, this is used. */
  size_t querynum;

  /* When receiving block status, this is used. */
  uint32_t *bs_entries;

  /* Commands which are waiting to be issued [meaning the request
   * packet is sent to the server].  This is used as a simple linked
   * list queue - commands are added to the back, and commands are
   * issued starting with the one on the front.  When commands have
   * been issued they are moved to cmds_in_flight.
   */
  struct command *cmds_to_issue;
  struct command *cmds_to_issue_tail;

  /* Commands which have been issued and are waiting for replies.
   * Order does not matter here, since the server can reply out-of-order.
   */
  struct command *cmds_in_flight;

  /* Commands which have received replies, waiting for the main
   * program to acknowledge them.  Maintained as a queue, with new
   * replies at the back, in case a client uses peek to process
   * replies in server order.
   */
  struct command *cmds_done;
  struct command *cmds_done_tail;

  /* length (cmds_to_issue) + length (cmds_in_flight). */
  int in_flight;

  /* Current command during a REPLY cycle */
  struct command *reply_cmd;

  bool disconnect_request;      /* True if we've queued NBD_CMD_DISC */
};

struct meta_context {
  struct meta_context *next;    /* Linked list. */
  char *name;                   /* Name of meta context. */
  uint32_t context_id;          /* Context ID negotiated with the server. */
};

struct socket_ops {
  ssize_t (*recv) (struct nbd_handle *h,
                   struct socket *sock, void *buf, size_t len);
  ssize_t (*send) (struct nbd_handle *h,
                   struct socket *sock, const void *buf, size_t len, int flags);
  bool (*pending) (struct socket *sock);
  int (*get_fd) (struct socket *sock);
  bool (*shut_writes) (struct nbd_handle *h, struct socket *sock);
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

struct command_cb {
  union {
    nbd_extent_callback extent;
    nbd_chunk_callback chunk;
  } fn;
  nbd_completion_callback completion;
};

struct command {
  struct command *next;
  uint16_t flags;
  uint16_t type;
  uint64_t cookie;
  uint64_t offset;
  uint32_t count;
  void *data; /* Buffer for read/write */
  struct command_cb cb;
  enum state state; /* State to resume with on next POLLIN */
  bool data_seen; /* For read, true if at least one data chunk seen */
  uint32_t error; /* Local errno value */
};

/* Test if a callback is "null" or not, and set it to null. */
#define CALLBACK_IS_NULL(cb)     ((cb).callback == NULL && (cb).free == NULL)
#define CALLBACK_IS_NOT_NULL(cb) (! CALLBACK_IS_NULL ((cb)))
#define SET_CALLBACK_TO_NULL(cb) ((cb).callback = NULL, (cb).free = NULL)

/* Call a callback. */
#define CALL_CALLBACK(cb, ...)                                          \
  ((cb).callback != NULL ? (cb).callback ((cb).user_data, ##__VA_ARGS__) : 0)

/* Free a callback. */
#define FREE_CALLBACK(cb)                               \
  do {                                                  \
    if ((cb).free != NULL)                              \
      (cb).free ((cb).user_data);                       \
    SET_CALLBACK_TO_NULL (cb);                          \
  } while (0)

/* aio.c */
extern void nbd_internal_retire_and_free_command (struct command *);

/* connect.c */
extern int nbd_internal_wait_until_connected (struct nbd_handle *h);

/* crypto.c */
extern struct socket *nbd_internal_crypto_create_session (struct nbd_handle *, struct socket *oldsock);
extern bool nbd_internal_crypto_is_reading (struct nbd_handle *);
extern int nbd_internal_crypto_handshake (struct nbd_handle *);
extern void nbd_internal_crypto_debug_tls_enabled (struct nbd_handle *);

/* debug.c */
extern void nbd_internal_debug (struct nbd_handle *h, const char *fs, ...);
#define debug(h, fs, ...)                               \
  do {                                                  \
    if_debug ((h))                                      \
      nbd_internal_debug ((h), (fs), ##__VA_ARGS__);    \
  } while (0)

/* errors.c */
extern void nbd_internal_set_error_context (const char *context);
extern const char *nbd_internal_get_error_context (void);
extern void nbd_internal_set_last_error (int errnum, char *error);
#define set_error(errnum, fs, ...)                                      \
  do {                                                                  \
    int _e = (errnum);                                                  \
    const char *_context =                                              \
      nbd_internal_get_error_context () ? : "unknown";                  \
    char *_errp;                                                        \
    int _r;                                                             \
    if (_e == 0)                                                        \
      _r = asprintf (&_errp, "%s: " fs, _context, ##__VA_ARGS__);       \
    else                                                                \
      _r = asprintf (&_errp, "%s: " fs ": %s",                          \
                     _context, ##__VA_ARGS__, strerror (_e));           \
    if (_r >= 0)                                                        \
      nbd_internal_set_last_error (_e, _errp);                          \
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
                                            void *data, struct command_cb *cb);

/* socket.c */
struct socket *nbd_internal_socket_create (int fd);

/* states.c */
extern int nbd_internal_run (struct nbd_handle *h, enum external_event ev);
extern const char *nbd_internal_state_short_string (enum state state);
extern enum state_group nbd_internal_state_group (enum state state);
extern enum state_group nbd_internal_state_group_parent (enum state_group group);
extern int nbd_internal_aio_get_direction (enum state state);

#define set_next_state(h,next_state) ((h)->state) = (next_state)
#define get_next_state(h) ((h)->state)
#define get_public_state(h) ((h)->public_state)

/* utils.c */
extern void nbd_internal_hexdump (const void *data, size_t len, FILE *fp);
extern size_t nbd_internal_string_list_length (char **argv);
extern char **nbd_internal_copy_string_list (char **argv);
extern void nbd_internal_free_string_list (char **argv);
extern const char *nbd_internal_fork_safe_itoa (long v, char *buf, size_t len);
extern void nbd_internal_fork_safe_perror (const char *s);
extern char *nbd_internal_printable_buffer (const void *buf, size_t count);
extern char *nbd_internal_printable_string (const char *str);
extern char *nbd_internal_printable_string_list (char **list);

#endif /* LIBNBD_INTERNAL_H */
