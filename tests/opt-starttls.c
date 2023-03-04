/* NBD client library in userspace
 * Copyright Red Hat
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

/* Test nbd_opt_starttls to nbdkit in various tls modes. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include <libnbd.h>

#include "requires.h"

struct expected {
  int first_opt_sr;
  int64_t first_size;
  int first_opt_meta;
  int first_can_meta;
  int first_opt_tls;
  int first_get_sr;
  int second_opt_sr;
  int second_can_meta;
  int second_opt_meta;
  int second_opt_tls;
  int get_tls;
  int second_get_sr;
  int third_can_meta;
  int64_t second_size;
};

static int
meta (void *user_data, const char *name)
{
  return 0;
}

#define check(got, exp) do_check (#got, got, exp)

static void
do_check (const char *act, int64_t got, int64_t exp)
{
  fprintf (stderr, "trying %s\n", act);
  if (got == -1)
    fprintf (stderr, "%s\n", nbd_get_error ());
  else
    fprintf (stderr, "succeeded, result %" PRId64 "\n", got);
  if (exp == -2 ? got < 0 : got != exp) {
    fprintf (stderr, "got %" PRId64 ", but expected %" PRId64 "\n", got, exp);
    exit (EXIT_FAILURE);
  }
}

static void
do_test (const char *server_tls, struct expected exp)
{
  struct nbd_handle *nbd = nbd_create ();

  if (nbd == NULL) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (nbd_set_opt_mode (nbd, true) == -1 ||
      nbd_set_request_structured_replies (nbd, false) == -1 ||
      nbd_set_request_meta_context (nbd, false) == -1 ||
      nbd_add_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION) == -1 ||
      nbd_set_tls_username (nbd, "alice") == -1 ||
      nbd_set_tls_psk_file (nbd, "keys.psk") == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  /* Run nbdkit as a subprocess. */
  const char *args[] = { "nbdkit", "-sv", "--exit-with-parent", server_tls,
                         "--tls-verify-peer", "--tls-psk=keys.psk",
                         "--filter=tls-fallback", "pattern",
                         "size=1M", "tlsreadme=fallback", NULL };

  if (nbd_connect_command (nbd, (char **) args) == -1) {
    fprintf (stderr, "%s\n", nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  check (nbd_opt_structured_reply (nbd), exp.first_opt_sr);
  check (nbd_opt_info (nbd), exp.first_size > 0 ? 0 : -1);
  check (nbd_get_size (nbd), exp.first_size);
  check (nbd_opt_set_meta_context (nbd,
                                   (nbd_context_callback) {.callback = meta}),
         exp.first_opt_meta);
  check (nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION),
         exp.first_can_meta);
  check (nbd_opt_starttls (nbd), exp.first_opt_tls);
  check (nbd_get_structured_replies_negotiated (nbd), exp.first_get_sr);
  /* When SR is already active, nbdkit 1.30.10 returns true, while
   * 1.33.2 returns false because the second SR request is redundant.
   * check() special-cases the value of -2 to cope with this.
   */
  check (nbd_opt_structured_reply (nbd), exp.second_opt_sr);
  check (nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION),
         exp.second_can_meta);
  check (nbd_opt_set_meta_context (nbd,
                                   (nbd_context_callback) {.callback = meta}),
         exp.second_opt_meta);
  check (nbd_opt_starttls (nbd), exp.second_opt_tls);
  check (nbd_get_tls_negotiated (nbd), exp.get_tls);
  check (nbd_get_structured_replies_negotiated (nbd), exp.second_get_sr);
  check (nbd_can_meta_context (nbd, LIBNBD_CONTEXT_BASE_ALLOCATION),
         exp.third_can_meta);
  check (nbd_opt_info (nbd), 0);
  check (nbd_get_size (nbd), exp.second_size);
  check (nbd_opt_abort (nbd), 0);

  nbd_close (nbd);
}

int
main (int argc, char *argv[])
{
  /* Check --tls-verify-peer option is supported. */
  requires ("nbdkit --tls-verify-peer -U - null --run 'exit 0'");
  /* Check for nbdkit tls-fallback filter. */
  requires ("nbdkit --filter=tls-fallback null --dump-plugin");

  /* Reject nbdkit 1.33.1 and older where --tls=require chokes on
   * early NBD_OPT_INFO. nbdkit does not have a nice witness for this,
   * so just try to provoke the bug manually (a working server will
   * gracefully fail both nbd_opt; a buggy one will lose sync and move
   * us to DEAD during our handling of the second one).
   */
  {
    const char *args[] = { "nbdkit", "-sv", "--exit-with-parent",
                           "--tls=require", "--tls-psk=keys.psk",
                           "null", "size=1M", NULL };
    struct nbd_handle *nbd = nbd_create ();

    if (nbd == NULL) {
      fprintf (stderr, "%s\n", nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    if (nbd_supports_tls (nbd) != 1) {
      fprintf (stderr, "SKIP: missing TLS support in libnbd\n");
      exit (77);
    }
    if (nbd_set_opt_mode (nbd, true) == -1 ||
        nbd_connect_command (nbd, (char **) args) == -1 ||
        nbd_opt_info (nbd) != -1 ||
        nbd_opt_info (nbd) != -1 ||
        nbd_aio_is_dead (nbd) == 1) {
      fprintf (stderr, "SKIP: nbdkit botches OPT_INFO before STARTTLS\n");
      exit (77);
    }
    nbd_close (nbd);
  }

  /* Behavior of a server with no TLS support */
  do_test ("--tls=no", (struct expected) {
      .first_opt_sr = 1,        /* Structured reply succeeds */
      .first_size = 512,        /* Sees the tls-fallback safe size */
      .first_opt_meta = 1,      /* Requesting meta context works */
      .first_can_meta = 1,      /* Context is negotiated */
      .first_opt_tls = 0,       /* Server lacks TLS, but connection stays up */
      .first_get_sr = 1,        /* Structured reply still good */
      .second_opt_sr = -2,      /* Second SR is redundant, version-dependent */
      .second_can_meta = 1,     /* Context still negotiated */
      .second_opt_meta = 1,     /* Second meta request works */
      .second_opt_tls = 0,      /* Server still lacks TLS */
      .get_tls = 0,             /* Final state of TLS - not secure */
      .second_get_sr = 1,       /* Structured reply still good */
      .third_can_meta = 1,      /* Meta context still works */
      .second_size = 512,       /* Still the tls-fallback safe size */
    });

  /* Behavior of a server with opportunistic TLS support */
  do_test ("--tls=on", (struct expected) {
      .first_opt_sr = 1,        /* Structured reply succeeds */
      .first_opt_meta = 1,      /* Requesting meta context works */
      .first_can_meta = 1,      /* Context is negotiated */
      .first_size = 512,        /* Sees the tls-fallback safe size */
      .first_opt_tls = 1,       /* Server takes TLS */
      .first_get_sr = 0,        /* Structured reply wiped by TLS */
      .second_opt_sr = 1,       /* Server accepts second SR */
      .second_can_meta = -1,    /* Contexts not requested since TLS */
      .second_opt_meta = 1,     /* Requesting meta context works */
      .second_opt_tls = 0,      /* Server rejects second TLS as redundant */
      .get_tls = 1,             /* Final state of TLS - secure */
      .second_get_sr = 1,       /* Structured reply still good */
      .third_can_meta = 1,      /* Meta context still works */
      .second_size = 1024*1024, /* Sees the actual size */
    });

  /* Behavior of a server that requires TLS support */
  do_test ("--tls=require", (struct expected) {
      .first_opt_sr = 0,        /* Structured reply fails without TLS first */
      .first_opt_meta = -1,     /* Meta context fails without TLS first */
      .first_can_meta = -1,     /* Context requires successful request */
      .first_size = -1,         /* Cannot request info */
      .first_opt_tls = 1,       /* Server takes TLS */
      .first_get_sr = 0,        /* Structured reply hasn't been requested */
      .second_opt_sr = 1,       /* Server accepts second SR */
      .second_can_meta = -1,    /* Contexts not requested since TLS */
      .second_opt_meta = 1,     /* Requesting meta context works */
      .second_opt_tls = 0,      /* Server rejects second TLS as redundant */
      .get_tls = 1,             /* Final state of TLS - secure */
      .second_get_sr = 1,       /* Structured reply still good */
      .third_can_meta = 1,      /* Meta context still works */
      .second_size = 1024*1024, /* Sees the actual size */
    });

  exit (EXIT_SUCCESS);
}
