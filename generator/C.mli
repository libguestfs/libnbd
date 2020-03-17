(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* nbd client library in userspace: generate the C API and documentation
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
 *)

val generate_lib_libnbd_syms : unit -> unit
val generate_include_libnbd_h : unit -> unit
val generate_lib_unlocked_h : unit -> unit
val generate_lib_api_c : unit -> unit
val generate_docs_Makefile_inc : unit -> unit
val generate_docs_api_links_pod : unit -> unit
val generate_docs_api_flag_links_pod : unit -> unit
val generate_docs_nbd_pod : string -> API.call -> unit -> unit
val print_arg_list : ?wrap:bool -> ?maxcol:int ->
                     ?handle:bool -> ?types:bool -> ?parens:bool ->
                     API.arg list -> API.optarg list -> unit
val print_cbarg_list : ?wrap:bool -> ?maxcol:int ->
                       ?types:bool -> ?parens:bool ->
                       API.cbarg list -> unit
val errcode_of_ret : API.ret -> string option
val type_of_ret : API.ret -> string
