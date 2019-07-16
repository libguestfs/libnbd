/* NBD client library in userspace
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

/* Miscellaneous helper functions used in the OCaml bindings. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <caml/alloc.h>
#include <caml/callback.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>

#include <libnbd.h>

#include "nbd-c.h"

void
nbd_internal_ocaml_raise_error (void)
{
  CAMLparam0 ();
  CAMLlocal1 (sv);
  value v[2];
  const char *msg;
  int err;

  msg = nbd_get_error ();
  err = nbd_get_errno ();

  if (msg)
    sv = caml_copy_string (msg);
  else
    sv = caml_copy_string ("no error message available");

  v[0] = sv;
  v[1] = Val_int (err);
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
char **
nbd_internal_ocaml_string_list (value ssv)
{
  CAMLparam1 (ssv);
  CAMLlocal1 (sv);
  size_t i, len;
  char **r;

  sv = ssv;
  for (len = 0; sv != Val_emptylist; sv = Field (sv, 1))
    len++;

  r = malloc (sizeof (char *) * (len+1));
  if (r == NULL) caml_raise_out_of_memory ();

  sv = ssv;
  for (i = 0; sv != Val_emptylist; sv = Field (sv, 1), i++)
    r[i] = String_val (Field (sv, 0));

  r[len] = NULL;
  CAMLreturnT (char **, r);
}

value
nbd_internal_ocaml_alloc_int32_array (uint32_t *a, size_t len)
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
