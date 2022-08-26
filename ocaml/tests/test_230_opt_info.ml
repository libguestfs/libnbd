(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* libnbd OCaml test case
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

let script =
  try
    let srcdir = Sys.getenv "srcdir" in
    sprintf "%s/../../tests/opt-info.sh" srcdir
  with
    Not_found -> failwith "error: srcdir is not defined"

let fail_unary f nbd =
  try
    let _ = f nbd in
    assert false
  with
    NBD.Error (errstr, errno) -> ()

let fail_binary f nbd arg =
  try
    let _ = f nbd arg in
    assert false
  with
    NBD.Error (errstr, errno) -> ()

let () =
  let nbd = NBD.create () in
  NBD.set_opt_mode nbd true;
  NBD.connect_command nbd
                      ["nbdkit"; "-s"; "--exit-with-parent"; "-v";
                       "sh"; script];
  NBD.add_meta_context nbd NBD.context_base_allocation;

  (* No size, flags, or meta-contexts yet *)
  fail_unary NBD.get_size nbd;
  fail_unary NBD.is_read_only nbd;
  fail_binary NBD.can_meta_context nbd NBD.context_base_allocation;

  (* info with no prior name gets info on "" *)
  NBD.opt_info nbd;
  let size = NBD.get_size nbd in
  assert (size = 0L);
  let ro = NBD.is_read_only nbd in
  assert ro;
  let meta = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert meta;

  (* changing export wipes out prior info *)
  NBD.set_export_name nbd "b";
  fail_unary NBD.get_size nbd;
  fail_unary NBD.is_read_only nbd;
  fail_binary NBD.can_meta_context nbd NBD.context_base_allocation;

  (* info on something not present fails *)
  NBD.set_export_name nbd "a";
  fail_unary NBD.opt_info nbd;

  (* info for a different export, with automatic meta_context disabled *)
  NBD.set_export_name nbd "b";
  NBD.set_request_meta_context nbd false;
  NBD.opt_info nbd;
  (* idempotent name change is no-op *)
  NBD.set_export_name nbd "b";
  let size = NBD.get_size nbd in
  assert (size = 1L);
  let ro = NBD.is_read_only nbd in
  assert (not ro);
  fail_binary NBD.can_meta_context nbd NBD.context_base_allocation;
  NBD.set_request_meta_context nbd true;

  (* go on something not present *)
  NBD.set_export_name nbd "a";
  fail_unary NBD.opt_go nbd;
  fail_unary NBD.get_size nbd;
  fail_unary NBD.is_read_only nbd;
  fail_binary NBD.can_meta_context nbd NBD.context_base_allocation;

  (* go on a valid export *)
  NBD.set_export_name nbd "good";
  NBD.opt_go nbd;
  let size = NBD.get_size nbd in
  assert (size = 4L);
  let ro = NBD.is_read_only nbd in
  assert ro;
  let meta = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert meta;

  (* now info is no longer valid, but does not wipe data *)
  fail_binary NBD.set_export_name nbd "a";
  let name = NBD.get_export_name nbd in
  assert (name = "good");
  fail_unary NBD.opt_info nbd;
  let size = NBD.get_size nbd in
  assert (size = 4L);
  let meta = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert meta;

  NBD.shutdown nbd;

  (* Another connection. This time, check that SET_META triggered by opt_info
   * persists through nbd_opt_go with set_request_meta_context disabled.
   *)
  let nbd = NBD.create () in
  NBD.set_opt_mode nbd true;
  NBD.connect_command nbd
                      ["nbdkit"; "-s"; "--exit-with-parent"; "-v";
                       "sh"; script];
  NBD.add_meta_context nbd "x-unexpected:bogus";

  fail_binary NBD.can_meta_context nbd NBD.context_base_allocation;
  NBD.opt_info nbd;
  let meta = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert (not meta);
  NBD.set_request_meta_context nbd false;
  (* Adding to the request list now won't matter *)
  NBD.add_meta_context nbd NBD.context_base_allocation;
  NBD.opt_go nbd;
  let meta = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert (not meta);

  NBD.shutdown nbd

let () = Gc.compact ()
