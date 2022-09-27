(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* nbd client library in userspace: generate the C API and documentation
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
 *)

open Printf

open API
open Utils

type closure_style = Direct | AddressOf | Pointer

let generate_lib_libnbd_syms () =
  generate_header HashStyle;

  (* Sort and group the calls by first_version, and emit them in order. *)
  let cmp (_, {first_version = a}) (_, {first_version = b}) = compare a b in
  let calls = List.sort cmp handle_calls in
  let extract ((_, {first_version}) as call) = first_version, call in
  let calls = List.map extract calls in
  let calls = group_by calls in

  let prev = ref None in
  List.iter (
    fun ((major, minor), calls) ->
      pr "# Public symbols in libnbd %d.%d:\n" major minor;
      pr "LIBNBD_%d.%d {\n" major minor;
      pr "  global:\n";
      if (major, minor) = (1, 0) then (
        pr "    nbd_create;\n";
        pr "    nbd_close;\n";
        pr "    nbd_get_errno;\n";
        pr "    nbd_get_error;\n"
      );
      List.iter (fun (name, _) -> pr "    nbd_%s;\n" name) calls;
      (match !prev with
       | None ->
          pr "  # Everything else is hidden.\n";
          pr "  local: *;\n";
       | Some _ -> ()
      );
      pr "}";
      (match !prev with
       | None -> ()
       | Some (old_major, old_minor) ->
          pr " LIBNBD_%d.%d" old_major old_minor
      );
      pr ";\n";
      pr "\n";
      prev := Some (major, minor)
  ) calls

let errcode_of_ret =
  function
  | RBool | RErr | RFd | RInt | RInt64 | RCookie | RSizeT -> Some "-1"
  | RStaticString | RString -> Some "NULL"
  | RUInt | RUIntPtr | RUInt64 | REnum _
  | RFlags _ -> None (* errors not possible *)

let type_of_ret =
  function
  | RBool | RErr | RFd | RInt | REnum _ -> "int"
  | RInt64 | RCookie -> "int64_t"
  | RSizeT -> "ssize_t"
  | RStaticString -> "const char *"
  | RString -> "char *"
  | RUInt -> "unsigned"
  | RUIntPtr -> "uintptr_t"
  | RUInt64 -> "uint64_t"
  | RFlags _ -> "uint32_t"

let rec name_of_arg =
  function
  | Bool n -> [n]
  | BytesIn (n, len) -> [n; len]
  | BytesOut (n, len) -> [n; len]
  | BytesPersistIn (n, len) -> [n; len]
  | BytesPersistOut (n, len) -> [n; len]
  | Closure { cbname } ->
     [ sprintf "%s_callback" cbname; sprintf "%s_user_data" cbname ]
  | Enum (n, _) -> [n]
  | Fd n -> [n]
  | Flags (n, _) -> [n]
  | Int n -> [n]
  | Int64 n -> [n]
  | Path n -> [n]
  | SizeT n -> [n]
  | SockAddrAndLen (n, len) -> [n; len]
  | String n -> [n]
  | StringList n -> [n]
  | UInt n -> [n]
  | UInt32 n -> [n]
  | UInt64 n -> [n]
  | UIntPtr n -> [n]

let rec print_arg_list ?(wrap = false) ?maxcol ?handle ?types ?(parens = true)
          ?closure_style args optargs =
  if parens then pr "(";
  if wrap then
    pr_wrap ?maxcol ','
      (fun () -> print_arg_list' ?handle ?types ?closure_style args optargs)
  else
    print_arg_list' ?handle ?types ?closure_style args optargs;
  if parens then pr ")"

and print_arg_list' ?(handle = false) ?(types = true) ?(closure_style = Direct)
          args optargs =
  let comma = ref false in
  if handle then (
    comma := true;
    if types then pr "struct nbd_handle *";
    pr "h"
  );
  List.iter (
    fun arg ->
      if !comma then pr ", ";
      comma := true;
      match arg with
      | Bool n ->
         if types then pr "bool ";
         pr "%s" n
      | BytesIn (n, len)
      | BytesPersistIn (n, len) ->
         if types then pr "const void *";
         pr "%s, " n;
         if types then pr "size_t ";
         pr "%s" len
      | BytesOut (n, len)
      | BytesPersistOut (n, len) ->
         if types then pr "void *";
         pr "%s, " n;
         if types then pr "size_t ";
         pr "%s" len
      | Closure { cbname; cbargs } ->
         if types then pr "nbd_%s_callback " cbname;
         let mark = match closure_style with
           | Direct -> ""
           | AddressOf -> "&"
           | Pointer -> "*" in
         pr "%s%s_callback" mark cbname
      | Enum (n, _) ->
         if types then pr "int ";
         pr "%s" n
      | Flags (n, _) ->
         if types then pr "uint32_t ";
         pr "%s" n
      | Fd n | Int n ->
         if types then pr "int ";
         pr "%s" n
      | Int64 n ->
         if types then pr "int64_t ";
         pr "%s" n
      | SizeT n ->
         if types then pr "size_t ";
         pr "%s" n
      | Path n
      | String n ->
         if types then pr "const char *";
         pr "%s" n
      | StringList n ->
         if types then pr "char **";
         pr "%s" n
      | SockAddrAndLen (n, len) ->
         if types then pr "const struct sockaddr *";
         pr "%s, " n;
         if types then pr "socklen_t ";
         pr "%s" len
      | UInt n ->
         if types then pr "unsigned ";
         pr "%s" n
      | UInt32 n ->
         if types then pr "uint32_t ";
         pr "%s" n
      | UInt64 n ->
         if types then pr "uint64_t ";
         pr "%s" n
      | UIntPtr n ->
         if types then pr "uintptr_t ";
         pr "%s" n
  ) args;
  List.iter (
    fun optarg ->
      if !comma then pr ", ";
      comma := true;
      match optarg with
      | OClosure { cbname; cbargs } ->
         if types then pr "nbd_%s_callback " cbname;
         let mark = match closure_style with
           | Direct -> ""
           | AddressOf -> "&"
           | Pointer -> "*" in
         pr "%s%s_callback" mark cbname
      | OFlags (n, _, _) ->
         if types then pr "uint32_t ";
         pr "%s" n
  ) optargs

let print_call ?wrap ?maxcol ?closure_style name args optargs ret =
  pr "%s nbd_%s " (type_of_ret ret) name;
  print_arg_list ~handle:true ?wrap ?maxcol ?closure_style args optargs

let print_fndecl ?wrap ?closure_style name args optargs ret =
  pr "extern ";
  print_call ?wrap ?closure_style name args optargs ret;
  pr ";\n"

let rec print_cbarg_list ?(wrap = false) ?maxcol ?types ?(parens = true)
          cbargs =
  if parens then pr "(";
  if wrap then
    pr_wrap ?maxcol ','
      (fun () -> print_cbarg_list' ?types cbargs)
  else
    print_cbarg_list' ?types cbargs;
  if parens then pr ")"

and print_cbarg_list' ?(types = true) cbargs =
  if types then pr "void *";
  pr "user_data";

  List.iter (
    fun cbarg ->
      pr ", ";
      match cbarg with
      | CBArrayAndLen (UInt32 n, len) ->
         if types then pr "uint32_t *";
         pr "%s, " n;
         if types then pr "size_t ";
         pr "%s" len
      | CBArrayAndLen _ -> assert false
      | CBBytesIn (n, len) ->
         if types then pr "const void *";
         pr "%s, " n;
         if types then pr "size_t ";
         pr "%s" len
      | CBInt n ->
         if types then pr "int ";
         pr "%s" n
      | CBInt64 n ->
         if types then pr "int64_t ";
         pr "%s" n
      | CBMutable (Int n) ->
         if types then pr "int *";
         pr "%s" n
      | CBMutable arg -> assert false
      | CBString n ->
         if types then pr "const char *";
         pr "%s" n
      | CBUInt n ->
         if types then pr "unsigned ";
         pr "%s" n
      | CBUInt64 n ->
         if types then pr "uint64_t ";
         pr "%s" n
  ) cbargs

(* Callback structs/typedefs in <libnbd.h> *)
let print_closure_structs () =
  pr "/* These are used for callback parameters.  They are passed\n";
  pr " * by value not by reference.  See CALLBACKS in libnbd(3).\n";
  pr " */\n";
  List.iter (
    fun { cbname; cbargs } ->
      pr "typedef struct {\n";
      pr "  int (*callback) ";
      print_cbarg_list ~wrap:true cbargs;
      pr ";\n";
      pr "  void *user_data;\n";
      pr "  void (*free) (void *user_data);\n";
      pr "} nbd_%s_callback;\n" cbname;
      pr "#define LIBNBD_HAVE_NBD_%s_CALLBACK 1\n"
        (String.uppercase_ascii cbname);
      pr "\n"
  ) all_closures;

  pr "/* Note NBD_NULL_* are only generated for callbacks which are\n";
  pr " * optional.  (See OClosure in the generator).\n";
  pr " */\n";
  let oclosures =
    let optargs = List.map (fun (_, { optargs }) -> optargs) handle_calls in
    let optargs = List.flatten optargs in
    let optargs =
      filter_map (function OClosure cb -> Some cb | _ -> None) optargs in
    sort_uniq optargs in
  List.iter (
    fun { cbname } ->
      pr "#define NBD_NULL_%s ((nbd_%s_callback) { .callback = NULL })\n"
        (String.uppercase_ascii cbname) cbname;
  ) oclosures;
  pr "\n"

let print_fndecl_and_define ?wrap name args optargs ret =
  let name_upper = String.uppercase_ascii name in
  print_fndecl ?wrap name args optargs ret;
  pr "#define LIBNBD_HAVE_NBD_%s 1\n" name_upper;
  pr "\n"

let print_ns_ctxt ns ns_upper ctxt consts =
  let ctxt_upper = String.uppercase_ascii ctxt in
  pr "#define LIBNBD_CONTEXT_%s_%s \"%s:%s\"\n"
    ns_upper ctxt_upper ns ctxt;
  pr "\n";
  pr "/* \"%s:%s\" context related constants */\n" ns ctxt;
  List.iter (fun (n, i) -> pr "#define LIBNBD_%-30s %d\n" n i) consts

let print_ns ns ctxts =
  let ns_upper = String.uppercase_ascii ns in
  pr "/* \"%s\" namespace */\n" ns;
  pr "#define LIBNBD_NAMESPACE_%s \"%s:\"\n" ns_upper ns;
  pr "\n";
  pr "/* \"%s\" namespace contexts */\n" ns;
  List.iter (
    fun (ctxt, consts) -> print_ns_ctxt ns ns_upper ctxt consts
  ) ctxts;
  pr "\n"

let generate_include_libnbd_h () =
  generate_header CStyle;

  pr "#ifndef LIBNBD_H\n";
  pr "#define LIBNBD_H\n";
  pr "\n";
  pr "/* This is the public interface to libnbd, a client library for\n";
  pr " * accessing Network Block Device (NBD) servers.\n";
  pr " *\n";
  pr " * Please read the libnbd(3) manual page to\n";
  pr " * find out how to use this library.\n";
  pr " */\n";
  pr "\n";
  pr "#include <stdbool.h>\n";
  pr "#include <stdint.h>\n";
  pr "#include <sys/socket.h>\n";
  pr "\n";
  pr "#ifdef __cplusplus\n";
  pr "extern \"C\" {\n";
  pr "#endif\n";
  pr "\n";
  pr "struct nbd_handle;\n";
  pr "\n";
  List.iter (
    fun { enum_prefix; enums } ->
      List.iter (
        fun (enum, i) ->
          let enum = sprintf "LIBNBD_%s_%s" enum_prefix enum in
          pr "#define %-40s %d\n" enum i
      ) enums;
      pr "\n"
  ) all_enums;
  List.iter (
    fun { flag_prefix; flags } ->
      let mask = ref 0 in
      List.iter (
        fun (flag, i) ->
          let flag = sprintf "LIBNBD_%s_%s" flag_prefix flag in
          pr "#define %-40s 0x%02x\n" flag i;
          mask := !mask lor i
      ) flags;
      let flag = sprintf "LIBNBD_%s_MASK" flag_prefix in
      pr "#define %-40s 0x%02x\n" flag !mask;
      pr "\n"
  ) all_flags;
  List.iter (
    fun (n, i) ->
      let n = sprintf "LIBNBD_%s" n in
      pr "#define %-40s %d\n" n i
  ) constants;
  pr "\n";
  pr "extern struct nbd_handle *nbd_create (void);\n";
  pr "#define LIBNBD_HAVE_NBD_CREATE 1\n";
  pr "\n";
  pr "extern void nbd_close (struct nbd_handle *h);\n";
  pr "#define LIBNBD_HAVE_NBD_CLOSE 1\n";
  pr "\n";
  pr "extern const char *nbd_get_error (void);\n";
  pr "#define LIBNBD_HAVE_NBD_GET_ERROR 1\n";
  pr "\n";
  pr "extern int nbd_get_errno (void);\n";
  pr "#define LIBNBD_HAVE_NBD_GET_ERRNO 1\n";
  pr "\n";
  print_closure_structs ();
  List.iter (
    fun (name, { args; optargs; ret }) ->
      print_fndecl_and_define ~wrap:true name args optargs ret
  ) handle_calls;
  List.iter (
    fun (ns, ctxts) -> print_ns ns ctxts
  ) metadata_namespaces;
  pr "#ifdef __cplusplus\n";
  pr "}\n";
  pr "#endif\n";
  pr "\n";
  pr "#endif /* LIBNBD_H */\n"

let generate_lib_unlocked_h () =
  generate_header CStyle;

  pr "#ifndef LIBNBD_UNLOCKED_H\n";
  pr "#define LIBNBD_UNLOCKED_H\n";
  pr "\n";
  List.iter (
    fun (name, { args; optargs; ret }) ->
    print_fndecl ~wrap:true ~closure_style:Pointer ("unlocked_" ^ name)
      args optargs ret
  ) handle_calls;
  pr "\n";
  pr "#endif /* LIBNBD_UNLOCKED_H */\n"

let permitted_state_text ?(fold=false) permitted_states =
  assert (permitted_states <> []);
  let sep = if fold then ", or\n" else ", or " in
  String.concat sep
    (List.map (
         function
         | Created -> "newly created"
         | Connecting -> "connecting"
         | Negotiating -> "negotiating"
         | Connected -> "connected with the server"
         | Closed -> "shut down"
         | Dead -> "dead"
       ) permitted_states
    )

(* Generate wrappers around each API call which are a place to
 * grab the thread mutex (lock) and do logging.
 *)
let generate_lib_api_c () =
  (* Print the wrapper added around all API calls. *)
  let rec print_wrapper (name, {args; optargs; ret; permitted_states;
                                is_locked; may_set_error}) =
    if permitted_states <> [] then (
      pr "static inline bool\n";
      pr "%s_in_permitted_state (struct nbd_handle *h)\n" name;
      pr "{\n";
      pr "  const enum state state = get_public_state (h);\n";
      pr "\n";
      let tests =
        List.map (
          function
          | Created -> "nbd_internal_is_state_created (state)"
          | Connecting -> "nbd_internal_is_state_connecting (state)"
          | Negotiating -> "nbd_internal_is_state_negotiating (state)"
          | Connected -> "nbd_internal_is_state_ready (state) || nbd_internal_is_state_processing (state)"
          | Closed -> "nbd_internal_is_state_closed (state)"
          | Dead -> "nbd_internal_is_state_dead (state)"
        ) permitted_states in
      pr "  if (!(%s)) {\n" (String.concat " ||\n        " tests);
      pr "    set_error (nbd_internal_is_state_created (state) ? ENOTCONN : EINVAL,\n";
      pr "               \"invalid state: %%s: the handle must be %%s\",\n";
      pr "               nbd_internal_state_short_string (state),\n";
      pr "               \"%s\");\n" (permitted_state_text permitted_states);
      pr "    return false;\n";
      pr "  }\n";
      pr "  return true;\n";
      pr "}\n";
      pr "\n"
    );

    let need_out_label = ref false in

    let ret_c_type = type_of_ret ret and errcode = errcode_of_ret ret in
    pr "%s\n" ret_c_type;
    pr "nbd_%s " name;
    print_arg_list ~wrap:true ~handle:true args optargs;
    pr "\n";
    pr "{\n";
    pr "  %s ret;\n" ret_c_type;
    pr "\n";
    if may_set_error then (
      pr "  nbd_internal_set_error_context (\"nbd_%s\");\n" name;
      pr "\n";
    )
    else
      pr "  /* This function must not call set_error. */\n";

    (* Lock the handle. *)
    if is_locked then
      pr "  pthread_mutex_lock (&h->lock);\n";
    print_trace_enter name args optargs may_set_error;
    pr "\n";

    (* Sanitize read buffers before any error is possible. *)
    List.iter (
      function
      | BytesOut (n, count)
      | BytesPersistOut (n, count) ->
         pr "  if (h->pread_initialize)\n";
         pr "    memset (%s, 0, %s);\n" n count
      | _ -> ()
    ) args;

    (* Check current state is permitted for this call. *)
    if permitted_states <> [] then (
      let value = match errcode with
        | Some value -> value
        | None -> assert false in
      pr "  if (unlikely (!%s_in_permitted_state (h))) {\n" name;
      pr "    ret = %s;\n" value;
      pr "    goto out;\n";
      pr "  }\n";
      need_out_label := true
    );

    (* Check parameters are valid. *)
    let print_flags_check n { flag_prefix; flags; guard } subset =
      let value = match errcode with
        | Some value -> value
        | None -> assert false in
      let mask = match subset with
        | Some [] -> "0"
        | Some subset ->
           let v = ref 0 in
           List.iter (
             fun (flag, i) ->
               if List.mem flag subset then v := !v lor i
           ) flags;
           sprintf "0x%x" !v
        | None -> "LIBNBD_" ^ flag_prefix ^ "_MASK" in
      let guard = match guard with
        | Some value -> " && " ^ value
        | None -> "" in
      pr "  if (unlikely ((%s & ~%s) != 0)%s) {\n" n mask guard;
      pr "    set_error (EINVAL, \"%%s: invalid value for flag: 0x%%x\",\n";
      pr "               \"%s\", %s);\n" n n;
      pr "    ret = %s;\n" value;
      pr "    goto out;\n";
      pr "  }\n";
      need_out_label := true
    in
    List.iter (
      function
      | Closure { cbname } ->
         let value = match errcode with
           | Some value -> value
           | None -> assert false in
         pr "  if (CALLBACK_IS_NULL (%s_callback)) {\n" cbname;
         pr "    set_error (EFAULT, \"%%s cannot be NULL\", \"%s\");\n" cbname;
         pr "    ret = %s;\n" value;
         pr "    goto out;\n";
         pr "  }\n";
         need_out_label := true
      | Enum (n, { enum_prefix; enums }) ->
         let value = match errcode with
           | Some value -> value
           | None -> assert false in
         pr "  switch (%s) {\n" n;
         List.iter (
           fun (enum, _) ->
             pr "  case LIBNBD_%s_%s:\n" enum_prefix enum
         ) enums;
         pr "    break;\n";
         pr "  default:\n";
         pr "    set_error (EINVAL, \"%%s: invalid value for parameter: %%d\",\n";
         pr "               \"%s\", %s);\n" n n;
         pr "    ret = %s;\n" value;
         pr "    goto out;\n";
         pr "  }\n";
         need_out_label := true
      | Flags (n, flags) ->
         print_flags_check n flags None
      | String n ->
         let value = match errcode with
           | Some value -> value
           | None -> assert false in
         pr "  if (%s == NULL) {\n" n;
         pr "    set_error (EFAULT, \"%%s cannot be NULL\", \"%s\");\n" n;
         pr "    ret = %s;\n" value;
         pr "    goto out;\n";
         pr "  }\n";
         need_out_label := true
      | _ -> ()
    ) args;
    List.iter (
      function
      | OClosure _ -> ()
      | OFlags (n, flags, subset) ->
         print_flags_check n flags subset
    ) optargs;

    (* Make the call. *)
    pr "  ret = nbd_unlocked_%s " name;
    print_arg_list ~wrap:true ~types:false ~handle:true
      ~closure_style:AddressOf args optargs;
    pr ";\n";
    pr "\n";
    if !need_out_label then
      pr " out:\n";
    print_trace_leave name ret may_set_error;
    pr "\n";
    (* Finish any closures not transferred to state machine. *)
    List.iter (
      function
      | Closure { cbname } ->
         pr "  FREE_CALLBACK (%s_callback);\n" cbname
      | _ -> ()
    ) args;
    List.iter (
      function
      | OClosure { cbname } ->
         pr "  FREE_CALLBACK (%s_callback);\n" cbname
      | OFlags _ -> ()
    ) optargs;
    if is_locked then (
      pr "  if (h->public_state != get_next_state (h))\n";
      pr "    h->public_state = get_next_state (h);\n";
      pr "  pthread_mutex_unlock (&h->lock);\n"
    );
    pr "  return ret;\n";
    pr "}\n";
    pr "\n"

  (* Print the trace when we enter a call with debugging enabled. *)
  and print_trace_enter name args optargs may_set_error =
    pr "  if_debug (h) {\n";
    List.iter (
      function
      | BytesIn (n, count)
      | BytesPersistIn (n, count) ->
         pr "    char *%s_printable =\n" n;
         pr "        nbd_internal_printable_buffer (%s, %s);\n" n count
      | Path n
      | String n ->
         pr "    char *%s_printable =\n" n;
         pr "        nbd_internal_printable_string (%s);\n" n
      | StringList n ->
         pr "    char *%s_printable =\n" n;
         pr "        nbd_internal_printable_string_list (%s);\n" n
      | BytesOut _ | BytesPersistOut _
      | Bool _ | Closure _ | Enum _ | Flags _ | Fd _ | Int _
      | Int64 _ | SizeT _
      | SockAddrAndLen _ | UInt _ | UInt32 _ | UInt64 _ | UIntPtr _ -> ()
    ) args;
    if may_set_error then
      pr "    debug (h, \"enter:"
    else (
      pr "    debug_direct (h, \"nbd_%s\", \"enter:" name
    );
    List.iter (
      function
      | Bool n -> pr " %s=%%s" n
      | BytesOut (n, count)
      | BytesPersistOut (n, count) -> pr " %s=<buf> %s=%%zu" n count
      | BytesIn (n, count)
      | BytesPersistIn (n, count) ->
         pr " %s=\\\"%%s\\\" %s=%%zu" n count
      | Closure { cbname } -> pr " %s=<fun>" cbname
      | Enum (n, _) -> pr " %s=%%d" n
      | Flags (n, _) -> pr " %s=0x%%x" n
      | Fd n | Int n -> pr " %s=%%d" n
      | Int64 n -> pr " %s=%%\" PRIi64 \"" n
      | SizeT n -> pr " %s=%%zu" n
      | SockAddrAndLen (n, len) -> pr " %s=<sockaddr> %s=%%d" n len
      | Path n
      | String n -> pr " %s=%%s" n
      | StringList n -> pr " %s=%%s" n
      | UInt n -> pr " %s=%%u" n
      | UInt32 n -> pr " %s=%%\" PRIu32 \"" n
      | UInt64 n -> pr " %s=%%\" PRIu64 \"" n
      | UIntPtr n -> pr " %s=%%\" PRIuPTR \"" n
    ) args;
    List.iter (
      function
      | OClosure { cbname } -> pr " %s=%%s" cbname
      | OFlags (n, _, _) -> pr " %s=0x%%x" n
    ) optargs;
    pr "\"";
    List.iter (
      function
      | Bool n -> pr ", %s ? \"true\" : \"false\"" n
      | BytesOut (n, count)
      | BytesPersistOut (n, count) -> pr ", %s" count
      | BytesIn (n, count)
      | BytesPersistIn (n, count) ->
         pr ", %s_printable ? %s_printable : \"\", %s" n n count
      | Closure { cbname } -> ()
      | Enum (n, _) -> pr ", %s" n
      | Flags (n, _) -> pr ", %s" n
      | Fd n | Int n | Int64 n | SizeT n -> pr ", %s" n
      | SockAddrAndLen (_, len) -> pr ", (int) %s" len
      | Path n | String n | StringList n ->
         pr ", %s_printable ? %s_printable : \"\"" n n
      | UInt n | UInt32 n | UInt64 n | UIntPtr n -> pr ", %s" n
    ) args;
    List.iter (
      function
      | OClosure { cbname } ->
         pr ", CALLBACK_IS_NULL (%s_callback) ? \"<fun>\" : \"NULL\"" cbname
      | OFlags (n, _, _) -> pr ", %s" n
    ) optargs;
    pr ");\n";
    List.iter (
      function
      | BytesIn (n, _)
      | BytesPersistIn (n, _)
      | Path n
      | String n
      | StringList n ->
         pr "    free (%s_printable);\n" n
      | BytesOut _ | BytesPersistOut _
      | Bool _ | Closure _ | Enum _ | Flags _ | Fd _ | Int _
      | Int64 _ | SizeT _
      | SockAddrAndLen _ | UInt _ | UInt32 _ | UInt64 _ | UIntPtr _ -> ()
    ) args;
    pr "  }\n"
  (* Print the trace when we leave a call with debugging enabled. *)
  and print_trace_leave name ret may_set_error =
    pr "  if_debug (h) {\n";
    if may_set_error then (
      (match errcode_of_ret ret with
       | Some r ->
          pr "    if (ret == %s)\n" r;
       | None -> assert false
      );
      pr "      debug (h, \"leave: error=\\\"%%s\\\"\", nbd_get_error ());\n";
      pr "    else {\n";
      (match ret with
       | RString ->
          pr "      char *ret_printable =\n";
          pr "          nbd_internal_printable_string (ret);\n"
       | _ -> ()
      );
      pr "      debug (h, \"leave: ret="
    )
    else (
      pr "    debug_direct (h, \"nbd_%s\", \"leave: ret=" name
    );
    (match ret with
     | RBool | RErr | RFd | RInt | REnum _ -> pr "%%d\", ret"
     | RInt64 | RCookie -> pr "%%\" PRIi64, ret"
     | RSizeT -> pr "%%zd\", ret"
     | RString -> pr "%%s\", ret_printable ? ret_printable : \"\""
     | RStaticString -> pr "%%s\", ret"
     | RUInt | RFlags _ -> pr "%%u\", ret"
     | RUIntPtr -> pr "%%\" PRIuPTR, ret"
     | RUInt64 -> pr "%%\" PRIu64, ret"
    );
    pr ");\n";
    if may_set_error then (
      (match ret with
       | RString -> pr "      free (ret_printable);\n"
       | _ -> ()
      );
      pr "    }\n";
    );
    pr "  }\n"
  in

  generate_header CStyle;

  pr "#include <config.h>\n";
  pr "\n";
  pr "#include <stdio.h>\n";
  pr "#include <stdlib.h>\n";
  pr "#include <stdint.h>\n";
  pr "#include <inttypes.h>\n";
  pr "#include <errno.h>\n";
  pr "\n";
  pr "#include <pthread.h>\n";
  pr "\n";
  pr "#include \"libnbd.h\"\n";
  pr "#include \"internal.h\"\n";
  pr "\n";
  List.iter print_wrapper handle_calls

(* We generate a fragment of Makefile.am containing the list
 * of generated functions, used in rules for building the manual
 * pages.  We exploit GNU make's sinclude to use this file without
 * upsetting automake.
 *)
let generate_docs_Makefile_inc () =
  generate_header HashStyle;

  pr "api_built += \\\n";
  List.iter (
    fun (name, _) ->
      pr "\tnbd_%s \\\n" name;
  ) handle_calls;
  pr "\t$(NULL)\n"

let generate_docs_api_links_pod () =
  let pages =
    List.map (fun (name, _) -> sprintf "nbd_%s(3)" name) handle_calls in
  let pages =
    "nbd_create(3)" ::
    "nbd_close(3)" ::
    "nbd_get_error(3)" ::
    "nbd_get_errno(3)" ::
    pages in
  let pages = List.sort compare pages in

  List.iteri (
    fun i page ->
      if i > 0 then pr ",\n";
      pr "L<%s>" page
  ) pages;
  pr ".\n"

let generate_docs_api_flag_links_pod () =
  let pages =
    filter_map (
      fun (name, _) ->
        if is_prefix name "is_" || is_prefix name "can_" then
          Some (sprintf "nbd_%s(3)" name)
        else
          None
    ) handle_calls in
  let pages = List.sort compare pages in

  List.iteri (
    fun i page ->
      if i > 0 then pr ",\n";
      pr "L<%s>" page
  ) pages;
  pr ".\n"

let print_docs_closure { cbname; cbargs } =
  pr " typedef struct {\n";
  pr "   int (*callback) ";
  print_cbarg_list ~wrap:true ~maxcol:60 cbargs;
  pr ";\n";
  pr "   void *user_data;\n";
  pr "   void (*free) (void *user_data);\n";
  pr " } nbd_%s_callback;\n" cbname

let generate_docs_nbd_pod name { args; optargs; ret;
                                 shortdesc; longdesc; example; see_also;
                                 permitted_states; may_set_error;
                                 first_version = (major, minor) } () =
  pr "=head1 NAME\n";
  pr "\n";
  pr "nbd_%s -" name;
  (* Don't use pr_wrap here because it leaves trailing whitespace. *)
  let words = nsplit " " shortdesc in
  List.iter (
    fun word ->
      if output_column () + String.length word > 70 then pr "\n%s" word
      else pr " %s" word
  ) words;
  pr "\n";
  pr "\n";

  pr "=head1 SYNOPSIS\n";
  pr "\n";
  pr " #include <libnbd.h>\n";
  pr "\n";

  List.iter (
    function
    | Closure cb -> print_docs_closure cb; pr "\n"
    | _ -> ()
  ) args;
  List.iter (
    function
    | OClosure cb -> print_docs_closure cb; pr "\n"
    | _ -> ()
  ) optargs;

  pr " ";
  print_call ~wrap:true ~maxcol:60 name args optargs ret;
  pr ";\n";
  pr "\n";

  pr "=head1 DESCRIPTION\n";
  pr "\n";
  pr "%s\n" longdesc;
  pr "\n";

  pr "=head1 RETURN VALUE\n";
  pr "\n";
  let errcode = errcode_of_ret ret in
  (match ret with
   | RBool ->
      pr "This call returns a boolean value.\n"
   | RStaticString ->
      pr "This call returns a statically allocated string, valid for the\n";
      pr "lifetime of the process or until libnbd is unloaded by\n";
      pr "L<dlclose(3)>.  You B<must not> try to free the string.\n"
   | RErr ->
      pr "If the call is successful the function returns C<0>.\n"
   | RFd ->
      pr "This call returns a file descriptor.\n"
   | RInt ->
      pr "This call returns an integer E<ge> C<0>.\n";
   | RInt64 ->
      pr "This call returns a 64 bit signed integer E<ge> C<0>.\n";
   | RCookie ->
      pr "This call returns the 64 bit cookie of the command.\n";
      pr "The cookie is E<ge> C<1>.\n";
      pr "Cookies are unique (per libnbd handle, not globally).\n"
   | RSizeT ->
      pr "This call returns an integer size E<ge> C<0>.\n";
   | RString ->
      pr "This call returns a string.  The caller must free the\n";
      pr "returned string to avoid a memory leak.\n";
   | RUInt ->
      pr "This call returns a bitmask.\n";
   | RUIntPtr ->
      pr "This call returns a C<uintptr_t>.\n";
   | RUInt64 ->
      pr "This call returns a counter.\n";
   | REnum { enum_prefix } ->
      pr "This call returns one of the LIBNBD_%s_* values.\n" enum_prefix;
   | RFlags { flag_prefix } ->
      pr "This call returns a bitmask of LIBNBD_%s_* values.\n" flag_prefix;
  );
  pr "\n";

  pr "=head1 ERRORS\n";
  pr "\n";
  if may_set_error then (
    let value = match errcode with
      | Some value -> value
      | None -> assert false in
    pr "On error C<%s> is returned.\n" value;
    pr "\n";
    pr "Refer to L<libnbd(3)/ERROR HANDLING>\n";
    pr "for how to get further details of the error.\n"
  )
  else
    pr "This function does not fail.\n";
  pr "\n";

  if permitted_states <> [] then (
    pr "=head1 HANDLE STATE\n";
    pr "\n";
    pr "The handle must be\n";
    pr "%s,\n" (permitted_state_text ~fold:true permitted_states);
    pr "otherwise this call will return an error.\n";
    pr "\n"
  );

  pr "=head1 VERSION\n";
  pr "\n";
  pr "This function first appeared in libnbd %d.%d.\n" major minor;
  pr "\n";
  pr "If you need to test if this function is available at compile time\n";
  pr "check if the following macro is defined:\n";
  pr "\n";
  pr " #define LIBNBD_HAVE_NBD_%s 1\n" (String.uppercase_ascii name);
  pr "\n";

  (match example with
   | None -> ()
   | Some filename ->
      pr "=head1 EXAMPLE\n";
      pr "\n";
      pr "This example is also available as F<%s>\n" filename;
      pr "in the libnbd source code.\n";
      pr "\n";
      let chan = open_in filename in
      (try
         while true do
           let line = input_line chan in
           let len = String.length line in
           if len > 60 then
             failwithf "%s: %s: line too long in example" name filename;
           if len = 0 then pr "\n" else pr " %s\n" line
         done
       with End_of_file -> ()
      );
      close_in chan;
      pr "\n"
  );

  let () =
    pr "=head1 SEE ALSO\n";
    pr "\n";
    let other_links = extract_links longdesc in
    let std_links = [MainPageLink; Link "create"] in
    let links = see_also @ other_links @ std_links in
    let links = sort_uniq_links links in
    List.iter verify_link links;
    let links = List.map pod_of_link links in
    let comma = ref false in
    List.iter (
      fun pod ->
        if !comma then pr ",\n"; comma := true;
        pr "%s" pod
    ) links;
    pr ".\n\n" in

  pr "\
=head1 AUTHORS

Eric Blake

Richard W.M. Jones

=head1 COPYRIGHT

Copyright (C) 2019-2021 Red Hat Inc.
"
