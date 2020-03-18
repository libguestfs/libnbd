(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* nbd client library in userspace: generator
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

(* OCaml bindings.
 *
 * Note we always pass the parameters as: optargs, handle, args.
 *)

open Printf

open API
open Utils

(* String representation of args and return value. *)
let rec ocaml_fundecl_to_string args optargs ret =
  let optargs = List.map ocaml_optarg_to_string optargs in
  let args = List.map ocaml_arg_to_string args in
  let ret = ocaml_ret_to_string ret in
  String.concat " -> " (optargs @ ["t"] @ args @ [ret])

(* String representation of a single OCaml arg. *)
and ocaml_arg_to_string = function
  | Bool _ -> "bool"
  | BytesIn _ -> "bytes"
  | BytesPersistIn _ -> "Buffer.t"
  | BytesOut _ -> "bytes"
  | BytesPersistOut _ -> "Buffer.t"
  | Closure { cbargs } ->
     sprintf "(%s)" (ocaml_closuredecl_to_string cbargs)
  | Enum (_, { enum_prefix }) -> sprintf "%s.t" enum_prefix
  | Fd _ -> "Unix.file_descr"
  | Flags (_, { flag_prefix }) -> sprintf "%s.t" flag_prefix
  | Int _ -> "int"
  | Int64 _ -> "int64"
  | Path _ -> "string"
  | SockAddrAndLen _ -> "string" (* XXX not impl *)
  | String _ -> "string"
  | StringList _ -> "string list"
  | UInt _ -> "int"
  | UInt32 _ -> "int32"
  | UInt64 _ -> "int64"

and ocaml_ret_to_string = function
  | RBool -> "bool"
  | RStaticString -> "string"
  | RErr -> "unit"
  | RFd -> "Unix.file_descr"
  | RInt -> "int"
  | RInt64 -> "int64"
  | RCookie -> "cookie"
  | RString -> "string"
  | RUInt -> "int"

and ocaml_optarg_to_string = function
  | OClosure { cbname; cbargs } ->
     sprintf "?%s:(%s)" cbname (ocaml_closuredecl_to_string cbargs)
  | OFlags (n, { flag_prefix }) -> sprintf "?%s:%s.t list" n flag_prefix

and ocaml_closuredecl_to_string cbargs =
  let cbargs = List.map ocaml_cbarg_to_string cbargs in
  String.concat " -> " (cbargs @ ["int"])

and ocaml_cbarg_to_string = function
  | CBArrayAndLen (arg, _) ->
     sprintf "%s array" (ocaml_arg_to_string arg)
  | CBBytesIn _ -> "bytes"
  | CBInt _ -> "int"
  | CBInt64 _ -> "int64"
  | CBMutable arg ->
     sprintf "%s ref" (ocaml_arg_to_string arg)
  | CBString _ -> "string"
  | CBUInt _ -> "int"
  | CBUInt64 _ -> "int64"

let ocaml_name_of_arg = function
  | Bool n -> n
  | BytesIn (n, len) -> n
  | BytesOut (n, len) -> n
  | BytesPersistIn (n, len) -> n
  | BytesPersistOut (n, len) -> n
  | Closure { cbname } -> cbname
  | Enum (n, _) -> n
  | Fd n -> n
  | Flags (n, _) -> n
  | Int n -> n
  | Int64 n -> n
  | Path n -> n
  | SockAddrAndLen (n, len) -> n
  | String n -> n
  | StringList n -> n
  | UInt n -> n
  | UInt32 n -> n
  | UInt64 n -> n

let ocaml_name_of_optarg = function
  | OClosure { cbname } -> cbname
  | OFlags (n, _) -> n

let num_params args optargs =
  List.length optargs + 1 (* handle *) + List.length args

let generate_ocaml_nbd_mli () =
  generate_header OCamlStyle;

  pr "\
(** OCaml bindings for libnbd.

    For full documentation see libnbd-ocaml(3) and libnbd(3).

    For examples written in OCaml see the libnbd source code
    [ocaml/examples] subdirectory.
*)

exception Error of string * int
(** Exception thrown when an API call fails.

    The string is the error message, and the int is the raw errno
    (if available).
*)

exception Closed of string
(** Exception thrown if you call a closed handle. *)

type cookie = int64

";

  List.iter (
    fun { enum_prefix; enums } ->
      pr "module %s : sig\n" enum_prefix;
      pr "  type t =\n";
      List.iter (
        fun (enum, _) ->
          pr "  | %s\n" enum
      ) enums;
      pr "end\n";
      pr "\n"
  ) all_enums;
  List.iter (
    fun { flag_prefix; flags } ->
      pr "module %s : sig\n" flag_prefix;
      pr "  type t =\n";
      List.iter (
        fun (flag, _) ->
          pr "  | %s\n" flag
      ) flags;
      pr "end\n";
      pr "\n"
  ) all_flags;
  List.iter (
    fun (n, _) -> pr "val %s : int32\n" (String.lowercase_ascii n)
  ) constants;
  List.iter (
    fun (ns, ctxts) ->
      pr "val namespace_%s : string\n" ns;
      List.iter (
        fun (ctxt, consts) ->
          pr "val context_%s_%s : string\n" ns ctxt;
          List.iter (fun (n, _) ->
              pr "val %s : int32\n" (String.lowercase_ascii n)
          ) consts
      ) ctxts;
  ) metadata_namespaces;
  pr "\n";

  pr "\
module Buffer : sig
  type t
  (** Persistent, mutable C-compatible malloc'd buffer, used in AIO calls. *)

  val alloc : int -> t
  (** Allocate an uninitialized buffer.  The parameter is the size
      in bytes. *)

  val to_bytes : t -> bytes
  (** Copy buffer to an OCaml [bytes] object. *)

  val of_bytes : bytes -> t
  (** Copy an OCaml [bytes] object to a newly allocated buffer. *)

  val size : t -> int
  (** Return the size of the buffer. *)

end
(** Persistent buffer used in AIO calls. *)

type t
(** The handle. *)

val create : unit -> t
(** Create a new handle. *)

val close : t -> unit
(** Close a handle.

    Handles can also be closed by the garbage collector when
    they become unreachable.  This call is used only if you want
    to force the handle to close now and reclaim resources
    immediately.
*)

";

  List.iter (
    fun (name, { args; optargs; ret; shortdesc; longdesc }) ->
      pr "val %s : %s\n" name (ocaml_fundecl_to_string args optargs ret);

      pr "(** %s\n" shortdesc;
      pr "\n";
      pr "%s" (String.concat "\n" (pod2text longdesc));
      pr "*)\n";
      pr "\n";

  ) handle_calls

let generate_ocaml_nbd_ml () =
  generate_header OCamlStyle;

  pr "\
exception Error of string * int
exception Closed of string
type cookie = int64

(* Give the exceptions names so that they can be raised from the C code. *)
let () =
  Callback.register_exception \"nbd_internal_ocaml_error\" (Error (\"\", 0));
  Callback.register_exception \"nbd_internal_ocaml_closed\" (Closed \"\")

";

  List.iter (
    fun { enum_prefix; enums } ->
      pr "module %s = struct\n" enum_prefix;
      pr "  type t =\n";
      List.iter (
        fun (enum, _) ->
          pr "  | %s\n" enum
      ) enums;
      pr "end\n";
      pr "\n"
  ) all_enums;
  List.iter (
    fun { flag_prefix; flags } ->
      pr "module %s = struct\n" flag_prefix;
      pr "  type t =\n";
      List.iter (
        fun (flag, _) ->
          pr "  | %s\n" flag
      ) flags;
      pr "end\n";
      pr "\n"
  ) all_flags;
  List.iter (
    fun (n, i) -> pr "let %s = %d_l\n" (String.lowercase_ascii n) i
  ) constants;
  List.iter (
    fun (ns, ctxts) ->
      pr "let namespace_%s = \"%s:\"\n" ns ns;
      List.iter (
        fun (ctxt, consts) ->
          pr "let context_%s_%s = \"%s:%s\"\n" ns ctxt ns ctxt;
          List.iter (fun (n, i) ->
              pr "let %s = %d_l\n" (String.lowercase_ascii n) i
          ) consts
      ) ctxts;
  ) metadata_namespaces;
  pr "\n";

  pr "\
module Buffer = struct
  type t
  external alloc : int -> t = \"nbd_internal_ocaml_buffer_alloc\"
  external to_bytes : t -> bytes = \"nbd_internal_ocaml_buffer_to_bytes\"
  external of_bytes : bytes -> t = \"nbd_internal_ocaml_buffer_of_bytes\"
  external size : t -> int = \"nbd_internal_ocaml_buffer_size\"
end

type t

external create : unit -> t = \"nbd_internal_ocaml_nbd_create\"
external close : t -> unit = \"nbd_internal_ocaml_nbd_close\"

";

  List.iter (
    fun (name, { args; optargs; ret }) ->
      pr "external %s : %s\n" name (ocaml_fundecl_to_string args optargs ret);
      pr "    = ";
      (* In OCaml, argument lists longer than 5 elements require
       * special handling in the C bindings.
       *)
      if num_params args optargs > 5 then
        pr "\"nbd_internal_ocaml_nbd_%s_byte\" " name;
      pr "\"nbd_internal_ocaml_nbd_%s\"\n" name
  ) handle_calls

let print_ocaml_enum_val { enum_prefix; enums } =
  pr "/* Convert OCaml %s.t to int. */\n" enum_prefix;
  pr "static int\n";
  pr "%s_val (value v)\n" enum_prefix;
  pr "{\n";
  pr "  /* NB: No allocation in this function, don't need to use\n";
  pr "   * CAML* wrappers.\n";
  pr "   */\n";
  pr "  int i, r = 0;\n";
  pr "\n";
  pr "  i = Int_val (Field (v, 0));\n";
  pr "  /* i is the index of the enum in the type\n";
  pr "   * (eg. i = 0 => enum = %s.%s).\n" enum_prefix (fst (List.hd enums));
  pr "   * Convert it to the C representation.\n";
  pr "   */\n";
  pr "  switch (i) {\n";
  List.iteri (
    fun i (enum, _) ->
      pr "  case %d: r = LIBNBD_%s_%s; break;\n" i enum_prefix enum
  ) enums;
  pr "  }\n";
  pr "\n";
  pr "  return r;\n";
  pr "}\n";
  pr "\n"

let print_ocaml_flag_val { flag_prefix; flags } =
  pr "/* Convert OCaml %s.t list to uint32_t bitmask. */\n" flag_prefix;
  pr "static uint32_t\n";
  pr "%s_val (value v)\n" flag_prefix;
  pr "{\n";
  pr "  /* NB: No allocation in this function, don't need to use\n";
  pr "   * CAML* wrappers.\n";
  pr "   */\n";
  pr "  int i;\n";
  pr "  uint32_t r = 0;\n";
  pr "\n";
  pr "  for (; v != Val_emptylist; v = Field (v, 1)) {\n";
  pr "    i = Int_val (Field (v, 0));\n";
  pr "    /* i is the index of the flag in the type\n";
  pr "     * (eg. i = 0 => flag = %s.%s).\n" flag_prefix (fst (List.hd flags));
  pr "     * Convert it to the C representation.\n";
  pr "     */\n";
  pr "    switch (i) {\n";
  List.iteri (
    fun i (flag, _) ->
      pr "    case %d: r |= LIBNBD_%s_%s; break;\n" i flag_prefix flag
  ) flags;
  pr "    }\n";
  pr "  }\n";
  pr "\n";
  pr "  return r;\n";
  pr "}\n";
  pr "\n"

let print_ocaml_closure_wrapper { cbname; cbargs } =
  let argnames =
    List.map (
      function
      | CBArrayAndLen (UInt32 n, _) | CBBytesIn (n, _)
      | CBInt n | CBInt64 n
      | CBMutable (Int n) | CBString n | CBUInt n | CBUInt64 n ->
         n ^ "v"
      | CBArrayAndLen _ | CBMutable _ -> assert false
      ) cbargs in

  pr "/* Wrapper for %s callback. */\n" cbname;
  pr "static int\n";
  pr "%s_wrapper_locked " cbname;
  C.print_cbarg_list ~wrap:true cbargs;
  pr "\n";
  pr "{\n";
  pr "  CAMLparam0 ();\n";
  assert (List.length argnames <= 5);
  pr "  CAMLlocal%d (%s);\n" (List.length argnames)
    (String.concat ", " argnames);
  pr "  CAMLlocal2 (exn, rv);\n";
  pr "  const struct user_data *data = user_data;\n";
  pr "  int r;\n";
  pr "  value args[%d];\n" (List.length argnames);
  pr "\n";

  List.iter (
    function
    | CBArrayAndLen (UInt32 n, count) ->
       pr "  %sv = nbd_internal_ocaml_alloc_int32_array (%s, %s);\n"
         n n count;
    | CBBytesIn (n, len) ->
       pr "  %sv = caml_alloc_initialized_string (%s, %s);\n" n len n
    | CBInt n | CBUInt n ->
       pr "  %sv = Val_int (%s);\n" n n
    | CBInt64 n ->
       pr "  %sv = caml_copy_int64 (%s);\n" n n
    | CBString n ->
       pr "  %sv = caml_copy_string (%s);\n" n n
    | CBUInt64 n ->
       pr "  %sv = caml_copy_int64 (%s);\n" n n
    | CBMutable (Int n) ->
       pr "  %sv = caml_alloc_tuple (1);\n" n;
       pr "  Store_field (%sv, 0, Val_int (*%s));\n" n n
    | CBArrayAndLen _ | CBMutable _ -> assert false
  ) cbargs;

  List.iteri (fun i n -> pr "  args[%d] = %s;\n" i n) argnames;

  pr "  rv = caml_callbackN_exn (data->fnv, %d, args);\n"
    (List.length argnames);

  List.iter (
    function
    | CBArrayAndLen (UInt32 _, _)
    | CBBytesIn _
    | CBInt _
    | CBInt64 _
    | CBString _
    | CBUInt _
    | CBUInt64 _ -> ()
    | CBMutable (Int n) ->
       pr "  *%s = Int_val (Field (%sv, 0));\n" n n
    | CBArrayAndLen _ | CBMutable _ -> assert false
  ) cbargs;

  pr "  if (Is_exception_result (rv)) {\n";
  pr "    nbd_internal_ocaml_exception_in_wrapper (\"%s\", rv);\n" cbname;
  pr "    CAMLreturnT (int, -1);\n";
  pr "  }\n";

  pr "\n";
  pr "  r = Int_val (rv);\n";
  pr "  assert (r >= 0);\n";
  pr "  CAMLreturnT (int, r);\n";
  pr "}\n";
  pr "\n";
  pr "static int\n";
  pr "%s_wrapper " cbname;
  C.print_cbarg_list ~wrap:true cbargs;
  pr "\n";
  pr "{\n";
  pr "  int ret = 0;\n";
  pr "\n";
  pr "  caml_leave_blocking_section ();\n";
  pr "  ret = %s_wrapper_locked " cbname;
  C.print_cbarg_list ~wrap:true ~types:false cbargs;
  pr ";\n";
  pr "  caml_enter_blocking_section ();\n";
  pr "  return ret;\n";
  pr "}\n";
  pr "\n"

let print_ocaml_binding (name, { args; optargs; ret }) =
  (* Get the names of all the value arguments including the handle. *)
  let values =
    List.map ocaml_name_of_optarg optargs @ ["h"] @
      List.map ocaml_name_of_arg args in
  let values = List.map (fun v -> v ^ "v") values in

  (* Create the binding. *)
  pr "value\n";
  let params = List.map (sprintf "value %s") values in
  let params = String.concat ", " params in
  pr "nbd_internal_ocaml_nbd_%s (" name;
  pr_wrap ',' (fun () -> pr "%s" params);
  pr ")\n";
  pr "{\n";
  (* CAMLparam<N> can only take up to 5 parameters.  Further parameters
   * have to be passed in groups of 5 to CAMLxparam<N> calls.
   *)
  (match values with
   | p1 :: p2 :: p3 :: p4 :: p5 :: rest ->
      pr "  CAMLparam5 (%s);\n" (String.concat ", " [p1; p2; p3; p4; p5]);
      let rec loop = function
        | [] -> ()
        | p1 :: p2 :: p3 :: p4 :: p5 :: rest ->
           pr "  CAMLxparam5 (%s);\n"
              (String.concat ", " [p1; p2; p3; p4; p5]);
           loop rest
        | rest ->
           pr "  CAMLxparam%d (%s);\n"
              (List.length rest) (String.concat ", " rest)
      in
      loop rest
   | ps ->
      pr "  CAMLparam%d (%s);\n" (List.length ps) (String.concat ", " ps)
  );
  pr "  CAMLlocal1 (rv);\n";
  pr "\n";

  pr "  struct nbd_handle *h = NBD_val (hv);\n";
  pr "  if (h == NULL)\n";
  pr "    nbd_internal_ocaml_raise_closed (\"NBD.%s\");\n" name;
  pr "\n";

  List.iter (
    function
    | OClosure { cbname } ->
       pr "  nbd_%s_callback %s_callback = {0};\n" cbname cbname;
       pr "  struct user_data *%s_user_data = alloc_user_data ();\n" cbname;
       pr "  if (%sv != Val_int (0)) { /* Some closure */\n" cbname;
       pr "    /* The function may save a reference to the closure, so we\n";
       pr "     * must treat it as a possible GC root.\n";
       pr "     */\n";
       pr "    %s_user_data->fnv = Field (%sv, 0);\n" cbname cbname;
       pr "    caml_register_generational_global_root (&%s_user_data->fnv);\n"
         cbname;
       pr "    %s_callback.callback = %s_wrapper;\n" cbname cbname;
       pr "  }\n";
       pr "  %s_callback.user_data = %s_user_data;\n" cbname cbname;
       pr "  %s_callback.free = free_user_data;\n" cbname;
    | OFlags (n, { flag_prefix }) ->
       pr "  uint32_t %s;\n" n;
       pr "  if (%sv != Val_int (0)) /* Some [ list of %s.t ] */\n"
         n flag_prefix;
       pr "    %s = %s_val (Field (%sv, 0));\n" n flag_prefix n;
       pr "  else /* None */\n";
       pr "    %s = 0;\n" n
  ) optargs;

  List.iter (
    function
    | Bool n ->
       pr "  bool %s = Bool_val (%sv);\n" n n
    | BytesIn (n, count) ->
       pr "  const void *%s = Bytes_val (%sv);\n" n n;
       pr "  size_t %s = caml_string_length (%sv);\n" count n
    | BytesPersistIn (n, count) ->
       pr "  struct nbd_buffer *%s_buf = NBD_buffer_val (%sv);\n" n n;
       pr "  const void *%s = %s_buf->data;\n" n n;
       pr "  size_t %s = %s_buf->len;\n" count n
    | BytesOut (n, count) ->
       pr "  void *%s = Bytes_val (%sv);\n" n n;
       pr "  size_t %s = caml_string_length (%sv);\n" count n
    | BytesPersistOut (n, count) ->
       pr "  struct nbd_buffer *%s_buf = NBD_buffer_val (%sv);\n" n n;
       pr "  void *%s = %s_buf->data;\n" n n;
       pr "  size_t %s = %s_buf->len;\n" count n
    | Closure { cbname } ->
       pr "  nbd_%s_callback %s_callback;\n" cbname cbname;
       pr "  struct user_data *%s_user_data = alloc_user_data ();\n" cbname;
       pr "  /* The function may save a reference to the closure, so we\n";
       pr "   * must treat it as a possible GC root.\n";
       pr "   */\n";
       pr "  %s_user_data->fnv = %sv;\n" cbname cbname;
       pr "  caml_register_generational_global_root (&%s_user_data->fnv);\n"
         cbname;
       pr "  %s_callback.callback = %s_wrapper;\n" cbname cbname;
       pr "  %s_callback.user_data = %s_user_data;\n" cbname cbname;
       pr "  %s_callback.free = free_user_data;\n" cbname
    | Enum (n, { enum_prefix }) ->
       pr "  int %s = %s_val (%sv);\n" n enum_prefix n
    | Fd n ->
       pr "  /* OCaml Unix.file_descr is just an int, at least on Unix. */\n";
       pr "  int %s = Int_val (%sv);\n" n n
    | Flags (n, { flag_prefix }) ->
       pr "  uint32_t %s = %s_val (%sv);\n" n flag_prefix n
    | Int n ->
       pr "  int %s = Int_val (%sv);\n" n n
    | Int64 n ->
       pr "  int64_t %s = Int64_val (%sv);\n" n n
    | Path n | String n ->
       pr "  const char *%s = String_val (%sv);\n" n n
    | SockAddrAndLen (n, len) ->
       pr "  const struct sockaddr *%s;\n" n;
       pr "  socklen_t %s;\n" len;
       pr "  abort ();\n" (* XXX *)
    | StringList n ->
       pr "  char **%s = (char **) nbd_internal_ocaml_string_list (%sv);\n" n n
    | UInt n ->
       pr "  unsigned %s = Int_val (%sv);\n" n n
    | UInt32 n ->
       pr "  uint32_t %s = Int32_val (%sv);\n" n n
    | UInt64 n ->
       pr "  uint64_t %s = Int64_val (%sv);\n" n n
  ) args;

  (* If there is a BytesPersistIn/Out parameter then we need to
   * register it as a global root and save that into the
   * completion_callback.user_data so the root is removed on
   * command completion.
   *)
  List.iter (
    function
    | BytesPersistIn (n, _) | BytesPersistOut (n, _) ->
       pr "  completion_user_data->bufv = %sv;\n" n;
       pr "  caml_register_generational_global_root (&completion_user_data->bufv);\n"
    | _ -> ()
  ) args;

  let ret_c_type = C.type_of_ret ret and errcode = C.errcode_of_ret ret in
  pr "  %s r;\n" ret_c_type;
  pr "\n";
  pr "  caml_enter_blocking_section ();\n";
  pr "  r =  nbd_%s " name;
  C.print_arg_list ~wrap:true ~handle:true ~types:false args optargs;
  pr ";\n";
  pr "  caml_leave_blocking_section ();\n";
  pr "\n";
  (match errcode with
   | Some code ->
      pr "  if (r == %s)\n" code;
      pr "    nbd_internal_ocaml_raise_error ();\n";
      pr "\n"
   | None -> ()
  );
  (match ret with
   | RBool -> pr "  rv = Val_bool (r);\n"
   | RErr -> pr "  rv = Val_unit;\n"
   | RFd | RInt | RUInt -> pr "  rv = Val_int (r);\n"
   | RInt64 | RCookie -> pr "  rv = caml_copy_int64 (r);\n"
   | RStaticString -> pr "  rv = caml_copy_string (r);\n"
   | RString ->
      pr "  rv = caml_copy_string (r);\n";
      pr "  free (r);\n"
  );

  (* Any parameters which need to be freed. *)
  List.iter (
    function
    | StringList n -> pr "  free (%s);\n" n
    | Bool _
    | BytesIn _
    | BytesPersistIn _
    | BytesOut _
    | BytesPersistOut _
    | Closure _
    | Enum _
    | Fd _
    | Flags _
    | Int _
    | Int64 _
    | Path _
    | String _
    | SockAddrAndLen _
    | UInt _
    | UInt32 _
    | UInt64 _ -> ()
  ) args;

  pr "  CAMLreturn (rv);\n";
  pr "}\n";
  pr "\n";

  if num_params args optargs > 5 then (
    pr "/* Byte-code compat function because this method has > 5 parameters.\n";
    pr " */\n";
    pr "value\n";
    pr "nbd_internal_ocaml_nbd_%s_byte (value *argv, int argn)\n" name;
    pr "{\n";
    pr "  return nbd_internal_ocaml_nbd_%s (" name;
    for i = 0 to num_params args optargs - 1 do
      if i > 0 then pr ", ";
      pr "argv[%d]" i
    done;
    pr ");\n";
    pr "}\n";
    pr "\n"
  )

let generate_ocaml_nbd_c () =
  generate_header CStyle;

  pr "#include <config.h>\n";
  pr "\n";
  pr "#include <stdio.h>\n";
  pr "#include <stdlib.h>\n";
  pr "#include <string.h>\n";
  pr "#include <assert.h>\n";
  pr "\n";
  pr "#include <libnbd.h>\n";
  pr "\n";
  pr "#include \"nbd-c.h\"\n";
  pr "\n";
  pr "#include <caml/alloc.h>\n";
  pr "#include <caml/callback.h>\n";
  pr "#include <caml/fail.h>\n";
  pr "#include <caml/memory.h>\n";
  pr "#include <caml/mlvalues.h>\n";
  pr "#include <caml/threads.h>\n";
  pr "\n";
  pr "#pragma GCC diagnostic ignored \"-Wmissing-prototypes\"\n";
  pr "\n";

  pr "/* This is passed to *_wrapper as the user_data pointer\n";
  pr " * and freed in the free_user_data function below.\n";
  pr " */\n";
  pr "struct user_data {\n";
  pr "  value fnv;     /* Optional GC root pointing to OCaml function. */\n";
  pr "  value bufv;    /* Optional GC root pointing to persistent buffer. */\n";
  pr "};\n";
  pr "\n";
  pr "static struct user_data *\n";
  pr "alloc_user_data (void)\n";
  pr "{\n";
  pr "  struct user_data *data = calloc (1, sizeof *data);\n";
  pr "  if (data == NULL)\n";
  pr "    caml_raise_out_of_memory ();\n";
  pr "  return data;\n";
  pr "}\n";
  pr "\n";
  pr "static void\n";
  pr "free_user_data (void *user_data)\n";
  pr "{\n";
  pr "  struct user_data *data = user_data;\n";
  pr "\n";
  pr "  if (data->fnv != 0)\n";
  pr "    caml_remove_generational_global_root (&data->fnv);\n";
  pr "  if (data->bufv != 0)\n";
  pr "    caml_remove_generational_global_root (&data->bufv);\n";
  pr "  free (data);\n";
  pr "}\n";
  pr "\n";

  List.iter print_ocaml_closure_wrapper all_closures;
  List.iter print_ocaml_enum_val all_enums;
  List.iter print_ocaml_flag_val all_flags;
  List.iter print_ocaml_binding handle_calls
