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

/* Miscellaneous helper functions used in the OCaml bindings. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <assert.h>

#include <caml/alloc.h>
#include <caml/callback.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>
#include <caml/printexc.h>
#include <caml/socketaddr.h>
#include <caml/unixsupport.h>

#include <libnbd.h>

#include "nbd-c.h"

/* NB: @@noalloc function */
value
nbd_internal_code_of_unix_error (value error)
{
  return Val_int (code_of_unix_error (error));
}

void
nbd_internal_ocaml_raise_error (void)
{
  CAMLparam0 ();
  CAMLlocal3 (sv, errv, ev);
  value v[2];
  const char *msg;
  int err;

  msg = nbd_get_error ();
  err = nbd_get_errno ();

  if (msg)
    sv = caml_copy_string (msg);
  else
    sv = caml_copy_string ("no error message available");

  /* Convert raw C errno to OCaml Unix.error, if available. */
  if (err == 0) {
    errv = Val_int (0); /* None */
  }
  else {
    ev = unix_error_of_code (err);
    errv = caml_alloc (1, 0);   /* Some ev */
    Field (errv, 0) = ev;
  }

  v[0] = sv;
  v[1] = errv;
  caml_raise_with_args (*caml_named_value ("nbd_internal_ocaml_error"),
                        2, v);
  CAMLnoreturn;
}

void
nbd_internal_ocaml_raise_closed (const char *func)
{
  CAMLparam0 ();
  CAMLlocal1 (sv);

  sv = caml_copy_string (func);
  caml_raise_with_arg (*caml_named_value ("nbd_internal_ocaml_closed"), sv);
  CAMLnoreturn;
}

/* The caller should free the array, but NOT the strings, since the
 * strings point to OCaml strings.
 */
const char **
nbd_internal_ocaml_string_list (value ssv)
{
  CAMLparam1 (ssv);
  CAMLlocal1 (sv);
  size_t i, len;
  const char **r;

  sv = ssv;
  for (len = 0; sv != Val_emptylist; sv = Field (sv, 1))
    len++;

  r = malloc (sizeof (const char *) * (len+1));
  if (r == NULL) caml_raise_out_of_memory ();

  sv = ssv;
  for (i = 0; sv != Val_emptylist; sv = Field (sv, 1), i++)
    r[i] = String_val (Field (sv, 0));

  r[len] = NULL;
  CAMLreturnT (const char **, r);
}

value
nbd_internal_ocaml_alloc_int64_from_uint32_array (uint32_t *a, size_t len)
{
  CAMLparam0 ();
  CAMLlocal2 (v, rv);
  size_t i;

  rv = caml_alloc (len, 0);
  for (i = 0; i < len; ++i) {
    v = caml_copy_int64 (a[i]);
    Store_field (rv, i, v);
  }

  CAMLreturn (rv);
}

/* Convert a Unix.sockaddr to a C struct sockaddr. */
void
nbd_internal_unix_sockaddr_to_sa (value sockaddrv,
                                  struct sockaddr_storage *ss,
                                  socklen_t *len)
{
  CAMLparam1 (sockaddrv);
  union sock_addr_union sa_u;
  socklen_param_type sl; /* this is really an int or socklen_t */

  memset (ss, 0, sizeof *ss);

  get_sockaddr (sockaddrv, &sa_u, &sl);
  assert (sl <= sizeof *ss);
  memcpy (ss, &sa_u, sl);
  *len = sl;

  CAMLreturn0;
}

/* Common code when an exception is raised in an OCaml callback.
 *
 * We handle Assert_failure specially by abort()-ing.  Other
 * exceptions are printed to make sure we don't lose information.
 */
void
nbd_internal_ocaml_exception_in_wrapper (const char *cbname, value rv)
{
  CAMLparam1 (rv);
  CAMLlocal1 (exn);
  const char *exn_name;
  char *s;

  exn = Extract_exception (rv);
  s = caml_format_exception (exn);
  fprintf (stderr,
           "libnbd: %s: uncaught OCaml exception: %s\n", cbname, s);
  free (s);

  /* For how we're getting the exception name, see:
   * https://github.com/libguestfs/libguestfs/blob/5d94be2583d557cfc7f8a8cfee7988abfa45a3f8/daemon/daemon-c.c#L40
   */
  if (Tag_val (Field (exn, 0)) == String_tag)
    exn_name = String_val (Field (exn, 0));
  else
    exn_name = String_val (Field (Field (exn, 0), 0));

  /* If the exception is fatal (like Assert_failure) then abort. */
  if (exn_name && strcmp (exn_name, "Assert_failure") == 0)
    abort ();

  CAMLreturn0;
}
