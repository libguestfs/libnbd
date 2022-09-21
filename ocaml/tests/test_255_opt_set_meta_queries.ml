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

let count = ref 0
let seen = ref false
let f user_data name =
  assert (user_data = 42);
  count := !count + 1;
  if name = NBD.context_base_allocation then
    seen := true;
  0

let () =
  (* Get into negotiating state. *)
  let nbd = NBD.create () in
  NBD.set_opt_mode nbd true;
  NBD.connect_command nbd
                      ["nbdkit"; "-s"; "--exit-with-parent"; "-v";
                       "memory"; "size=1M"];

  (* nbdkit does not match wildcard for SET, even though it does for LIST *)
  count := 0;
  seen := false;
  let r = NBD.opt_set_meta_context_queries nbd ["base:"] (f 42) in
  assert (r = !count);
  assert (r = 0);
  assert (not !seen);
  let m = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert (not m);

  (* Negotiating with no contexts is not an error, but selects nothing.
   * An explicit empty list overrides a non-empty implicit list.
   *)
  count := 0;
  seen := false;
  NBD.add_meta_context nbd NBD.context_base_allocation;
  let r = NBD.opt_set_meta_context_queries nbd [] (f 42) in
  assert (r = 0);
  assert (r = !count);
  assert (not !seen);
  let m = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert (not m);

  (* Request 2 with expectation of 1. *)
  count := 0;
  seen := false;
  let r = NBD.opt_set_meta_context_queries nbd
            ["x-nosuch:context"; NBD.context_base_allocation] (f 42) in
  assert (r = 1);
  assert (r = !count);
  assert !seen;
  let m = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert m;

  (* Transition to transmission phase; our last set should remain active *)
  NBD.set_request_meta_context nbd false;
  NBD.opt_go nbd;
  let m = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert m;

  NBD.shutdown nbd

let () = Gc.compact ()
