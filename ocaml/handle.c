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

#include <caml/alloc.h>
#include <caml/callback.h>
#include <caml/fail.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>

#include <libnbd.h>

#include "nbd-c.h"

void
nbd_internal_ocaml_handle_finalize (value hv)
{
  struct nbd_handle *h = NBD_val (hv);

  nbd_close (h);
}

value
nbd_internal_ocaml_nbd_create (value unitv)
{
  CAMLparam1 (unitv);
  CAMLlocal1 (rv);
  struct nbd_handle *h;

  h = nbd_create ();
  if (h == NULL)
    nbd_internal_ocaml_raise_error ();

  rv = Val_nbd (h);
  CAMLreturn (rv);
}

value
nbd_internal_ocaml_nbd_close (value hv)
{
  CAMLparam1 (hv);

  nbd_internal_ocaml_handle_finalize (hv);

  /* So we don't double-free in the finalizer. */
  NBD_val (hv) = NULL;

  CAMLreturn (Val_unit);
}
