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
  let r = NBD.opt_list_meta_context nbd (f 42) in
  assert (r = !count);
  assert (r >= 1);
  assert !seen;
  let max = !count in

  (* Second pass: bogus query has no response. *)
  count := 0;
  seen := false;
  NBD.add_meta_context nbd "x-nosuch:";
  let r = NBD.opt_list_meta_context nbd (f 42) in
  assert (r = 0);
  assert (r = !count);
  assert (not !seen);

  (* Third pass: specific query should have one match. *)
  count := 0;
  seen := false;
  NBD.add_meta_context nbd NBD.context_base_allocation;
  let c = NBD.get_nr_meta_contexts nbd in
  assert (c = 2);
  let n = NBD.get_meta_context nbd 1 in
  assert (n = NBD.context_base_allocation);
  let r = NBD.opt_list_meta_context nbd (f 42) in
  assert (r = 1);
  assert (r = !count);
  assert !seen;

  (* Fourth pass: opt_list_meta_context is stateless, so it should
   * not wipe status learned during opt_info
   *)
  count := 0;
  seen := false;
  (try
     let _ = NBD.can_meta_context nbd NBD.context_base_allocation in
     assert false
   with
     NBD.Error (errstr, errno) -> ()
  );
  (try
     let _ = NBD.get_size nbd in
     assert false
   with
     NBD.Error (errstr, errno) -> ()
  );
  NBD.opt_info nbd;
  let s = NBD.get_size nbd in
  assert (s = 1048576_L);
  let m = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert m;
  NBD.clear_meta_contexts nbd;
  NBD.add_meta_context nbd "x-nosuch:";
  let r = NBD.opt_list_meta_context nbd (f 42) in
  assert (r = 0);
  assert (r = !count);
  assert (not !seen);
  let s = NBD.get_size nbd in
  assert (s = 1048576_L);
  let m = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert m;

  (* Final pass: "base:" query should get at least "base:allocation" *)
  count := 0;
  seen := false;
  NBD.add_meta_context nbd "base:";
  let r = NBD.opt_list_meta_context nbd (f 42) in
  assert (r >= 1);
  assert (r <= max);
  assert (r = !count);
  assert !seen;

  NBD.opt_abort nbd;

  (* Repeat but this time without structured replies. Deal gracefully
   * with older servers that don't allow the attempt.
   *)
  let nbd = NBD.create () in
  NBD.set_opt_mode nbd true;
  NBD.set_request_structured_replies nbd false;
  NBD.connect_command nbd
                      ["nbdkit"; "-s"; "--exit-with-parent"; "-v";
                       "memory"; "size=1M"];
  let bytes = NBD.stats_bytes_sent nbd in
  (try
     count := 0;
     seen := false;
     let r = NBD.opt_list_meta_context nbd (f 42) in
     assert (r = !count);
     assert (r >= 1);
     assert !seen
   with
     NBD.Error (errstr, errno) ->
       assert (NBD.stats_bytes_sent nbd > bytes);
       printf "ignoring failure from old server %s\n" errstr
  );

  (* FIXME: Once nbd_opt_structured_reply exists, use it here and retry. *)

  NBD.opt_abort nbd

let () = Gc.compact ()
