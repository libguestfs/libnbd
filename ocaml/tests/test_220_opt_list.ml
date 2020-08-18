(* libnbd OCaml test case
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

let () =
  (* Require new-enough nbdkit *)
  let cmd = "nbdkit sh --dump-plugin | grep -q has_list_exports=1" in
  if Sys.command cmd <> 0 then (
    exit 77
  );

  let nbd = NBD.create () in
  NBD.set_opt_mode nbd true;
  NBD.connect_command nbd
                      ["nbdkit"; "-s"; "--exit-with-parent"; "-v";
                       "sh"; script];

  (* First pass: server fails NBD_OPT_LIST *)
  exports := [];
  ( try
      let _ = NBD.opt_list nbd (f 42) in
      assert (false)
    with
      NBD.Error (errstr, errno) -> ()
  );
  assert (!exports = []);

  (* Second pass: server advertises 'a' and 'b' *)
  exports := [];
  let count = NBD.opt_list nbd (f 42) in
  assert (count = 2);
  assert (!exports = [ "a"; "b" ]);

  (* Third pass: server advertises empty list *)
  exports := [];
  let count = NBD.opt_list nbd (f 42) in
  assert (count = 0);
  assert (!exports = []);

  (* Final pass: server advertises 'a' *)
  exports := [];
  let count = NBD.opt_list nbd (f 42) in
  assert (count = 1);
  assert (!exports = [ "a" ]);

  NBD.opt_abort nbd

let () = Gc.compact ()
