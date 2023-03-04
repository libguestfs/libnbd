(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* libnbd OCaml test case
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
 *)

open Printf

let script =
  try
    let srcdir = Sys.getenv "srcdir" in
    sprintf "%s/../../tests/opt-list.sh" srcdir
  with
    Not_found -> failwith "error: srcdir is not defined"

let exports = ref []
let f user_data name desc =
  assert (user_data = 42);
  assert (desc = "");
  exports := !exports @ [name];
  0

let conn mode body expect =
  exports := [];
  let nbd = NBD.create () in
  NBD.set_opt_mode nbd true;
  let mode = sprintf "mode=%d" mode in
  NBD.connect_command nbd
                      ["nbdkit"; "-s"; "--exit-with-parent"; "-v";
                       "sh"; script; mode];
  body nbd;
  assert (!exports = expect);
  NBD.opt_abort nbd

let () =
  (* Require new-enough nbdkit *)
  let cmd = "nbdkit sh --dump-plugin | grep -q has_list_exports=1" in
  if Sys.command cmd <> 0 then (
    exit 77
  );

  (* First pass: server fails NBD_OPT_LIST *)
  conn 0 (
    fun (nbd) ->
    try
      let _ = NBD.opt_list nbd (f 42) in
      assert false
    with
      NBD.Error (errstr, errno) -> ()
    ) [];

  (* Second pass: server advertises 'a' and 'b' *)
  conn 1 (
    fun (nbd) ->
    let count = NBD.opt_list nbd (f 42) in
    assert (count = 2);
    ) [ "a"; "b" ];

  (* Third pass: server advertises empty list *)
  conn 2 (
    fun (nbd) ->
    let count = NBD.opt_list nbd (f 42) in
    assert (count = 0);
    ) [];

  (* Final pass: server advertises 'a' *)
  conn 3 (
    fun (nbd) ->
    let count = NBD.opt_list nbd (f 42) in
    assert (count = 1);
    ) [ "a" ]

let () = Gc.compact ()
