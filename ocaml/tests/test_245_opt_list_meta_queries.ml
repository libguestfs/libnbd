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

  (* First pass: empty query should give at least "base:allocation". *)
  count := 0;
  seen := false;
  NBD.add_meta_context nbd "x-nosuch:";
  let r = NBD.opt_list_meta_context_queries nbd [] (f 42) in
  assert (r = !count);
  assert (r >= 1);
  assert !seen;

  (* Second pass: bogus query has no response. *)
  count := 0;
  seen := false;
  NBD.clear_meta_contexts nbd;
  let r = NBD.opt_list_meta_context_queries nbd ["x-nosuch:"] (f 42) in
  assert (r = 0);
  assert (r = !count);
  assert (not !seen);

  (* Third pass: specific query should have one match. *)
  count := 0;
  seen := false;
  let r = NBD.opt_list_meta_context_queries nbd [
              "x-nosuch:"; NBD.context_base_allocation ] (f 42) in
  assert (r = 1);
  assert (r = !count);
  assert !seen;

  NBD.opt_abort nbd

let () = Gc.compact ()
