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

/* Various shared definitions for OCaml bindings. */

#ifndef LIBNBD_NBD_C_H
#define LIBNBD_NBD_C_H

#include <stdint.h>

#include <caml/custom.h>
#include <caml/memory.h>
#include <caml/mlvalues.h>

// Workaround for OCaml < 4.06.0
#ifndef Bytes_val
#define Bytes_val(x) String_val(x)
#endif

extern void libnbd_finalize (value);
extern void nbd_buffer_finalize (value);

extern void nbd_internal_ocaml_raise_error (void) Noreturn;
extern void nbd_internal_ocaml_raise_closed (const char *func) Noreturn;

/* Extract an NBD handle from an OCaml heap value. */
#define NBD_val(v) (*((struct nbd_handle **)Data_custom_val(v)))

static struct custom_operations libnbd_custom_operations = {
  (char *) "libnbd_custom_operations",
  libnbd_finalize,
  custom_compare_default,
  custom_hash_default,
  custom_serialize_default,
  custom_deserialize_default,
  custom_compare_ext_default,
};

/* Embed an NBD handle in an OCaml heap value. */
static inline value
Val_nbd (struct nbd_handle *h)
{
  CAMLparam0 ();
  CAMLlocal1 (rv);

  rv = caml_alloc_custom (&libnbd_custom_operations,
                          sizeof (struct nbd_handle *), 0, 1);
  NBD_val (rv) = h;
  CAMLreturn (rv);
}

/* Persistent buffer for AIO. */
struct nbd_buffer {
  void *data;
  size_t len;
};

/* Extract a persistent buffer from an OCaml heap value.  Note the
 * whole struct is stored in the custom, not a pointer.  This
 * returns a pointer to the struct .
 */
#define NBD_buffer_val(v) ((struct nbd_buffer *)Data_custom_val(v))

static struct custom_operations nbd_buffer_custom_operations = {
  (char *) "nbd_buffer_custom_operations",
  nbd_buffer_finalize,
  custom_compare_default,
  custom_hash_default,
  custom_serialize_default,
  custom_deserialize_default,
  custom_compare_ext_default,
};

/* Embed an NBD persistent buffer in an OCaml heap value. */
static inline value
Val_nbd_buffer (struct nbd_buffer b)
{
  CAMLparam0 ();
  CAMLlocal1 (rv);

  rv = caml_alloc_custom (&nbd_buffer_custom_operations,
                          sizeof (struct nbd_buffer), 0, 1);
  *NBD_buffer_val (rv) = b;
  CAMLreturn (rv);
}

struct callback_data {
  value *cb;
  value *data;
};

extern char **nbd_internal_ocaml_string_list (value);
extern value nbd_internal_ocaml_alloc_int32_array (uint32_t *, size_t);

#endif /* LIBNBD_NBD_C_H */
