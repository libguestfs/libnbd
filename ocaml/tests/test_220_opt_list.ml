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
  (* XXX We can't tell the difference *)
  NBD.opt_list nbd;
  let count = NBD.get_nr_list_exports nbd in
  assert (count = 0);

  (* Second pass: server advertises 'a' and 'b' *)
  NBD.opt_list nbd;
  let count = NBD.get_nr_list_exports nbd in
  assert (count = 2);
  let name = NBD.get_list_export_name nbd 0 in
  assert (name = "a");
  let name = NBD.get_list_export_name nbd 1 in
  assert (name = "b");

  (* Third pass: server advertises empty list *)
  NBD.opt_list nbd;
  let count = NBD.get_nr_list_exports nbd in
    assert (count = 0);
  ( try
      let _ = NBD.get_list_export_name nbd 0 in
      assert (false)
    with
      NBD.Error (errstr, errno) -> ()
  );

  (* Final pass: server advertises 'a' *)
  NBD.opt_list nbd;
  let count = NBD.get_nr_list_exports nbd in
  assert (count = 1);
  let name = NBD.get_list_export_name nbd 0 in
  assert (name = "a");

  NBD.opt_abort nbd

let () = Gc.compact ()
