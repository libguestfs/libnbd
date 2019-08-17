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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>

#include <libnbd.h>

#include "nbd-c.h"

void
nbd_internal_ocaml_buffer_finalize (value bv)
{
  struct nbd_buffer *b = NBD_buffer_val (bv);

  free (b->data);
}

/* Allocate an NBD persistent buffer. */
value
nbd_internal_ocaml_buffer_alloc (value sizev)
{
  CAMLparam1 (sizev);
  CAMLlocal1 (rv);
  struct nbd_buffer b;

  b.len = Int_val (sizev);
  b.data = malloc (b.len);
  if (b.data == NULL)
    caml_raise_out_of_memory ();

  rv = Val_nbd_buffer (b);
  CAMLreturn (rv);
}

/* Copy an NBD persistent buffer to an OCaml bytes. */
value
nbd_internal_ocaml_buffer_to_bytes (value bv)
{
  CAMLparam1 (bv);
  CAMLlocal1 (rv);
  struct nbd_buffer *b = NBD_buffer_val (bv);

  rv = caml_alloc_string (b->len);
  memcpy (Bytes_val (rv), b->data, b->len);

  CAMLreturn (rv);
}

/* Copy an OCaml bytes into an NBD persistent buffer. */
value
nbd_internal_ocaml_buffer_of_bytes (value bytesv)
{
  CAMLparam1 (bytesv);
  CAMLlocal1 (rv);
  struct nbd_buffer b;

  b.len = caml_string_length (bytesv);
  b.data = malloc (b.len);
  if (b.data == NULL)
    caml_raise_out_of_memory ();
  memcpy (b.data, Bytes_val (bytesv), b.len);

  rv = Val_nbd_buffer (b);
  CAMLreturn (rv);
}

value
nbd_internal_ocaml_buffer_size (value bv)
{
  CAMLparam1 (bv);
  CAMLlocal1 (rv);
  struct nbd_buffer *b = NBD_buffer_val (bv);

  CAMLreturn (Val_int (b->len));
}
