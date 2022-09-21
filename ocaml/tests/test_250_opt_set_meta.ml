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
  (* First process, with structured replies. Get into negotiating state. *)
  let nbd = NBD.create () in
  NBD.set_opt_mode nbd true;
  NBD.connect_command nbd
                      ["nbdkit"; "-s"; "--exit-with-parent"; "-v";
                       "memory"; "size=1M"];

  (* No contexts negotiated yet; can_meta should be error if any requested *)
  let sr = NBD.get_structured_replies_negotiated nbd in
  assert sr;
  let m = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert (not m);
  NBD.add_meta_context nbd NBD.context_base_allocation;
  (try
     let _ = NBD.can_meta_context nbd NBD.context_base_allocation in
     assert false
   with
     NBD.Error (errstr, errno) -> ()
  );

  (* FIXME: Once nbd_opt_structured_reply exists, check that set before
   * SR fails server-side, then enable SR for rest of process.
   *)

  (* nbdkit does not match wildcard for SET, even though it does for LIST *)
  count := 0;
  seen := false;
  NBD.clear_meta_contexts nbd;
  NBD.add_meta_context nbd "base:";
  let r = NBD.opt_set_meta_context nbd (f 42) in
  assert (r = !count);
  assert (r = 0);
  assert (not !seen);
  let m = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert (not m);

  (* Negotiating with no contexts is not an error, but selects nothing *)
  count := 0;
  seen := false;
  NBD.clear_meta_contexts nbd;
  let r = NBD.opt_set_meta_context nbd (f 42) in
  assert (r = 0);
  assert (r = !count);
  assert (not !seen);
  let m = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert (not m);

  (* Request 2 with expectation of 1; with set_request_meta_context off *)
  count := 0;
  seen := false;
  NBD.add_meta_context nbd "x-nosuch:context";
  NBD.add_meta_context nbd NBD.context_base_allocation;
  NBD.set_request_meta_context nbd false;
  let r = NBD.opt_set_meta_context nbd (f 42) in
  assert (r = 1);
  assert (r = !count);
  assert !seen;
  let m = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert m;

  (* Transition to transmission phase; our last set should remain active *)
  NBD.clear_meta_contexts nbd;
  NBD.add_meta_context nbd "x-nosuch:context";
  NBD.opt_go nbd;
  let m = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert m;

  (* Now too late to set; but should not lose earlier state *)
  count := 0;
  seen := false;
  (try
     let _ = NBD.opt_set_meta_context nbd (f 42) in
     assert false
   with
     NBD.Error (errstr, errno) -> ()
  );
  assert (0 = !count);
  assert (not !seen);
  let s = NBD.get_size nbd in
  assert (s = 1048576_L);
  let m = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert m;

  NBD.shutdown nbd;

  (* Second process, this time without structured replies server-side. *)
  let nbd = NBD.create () in
  NBD.set_opt_mode nbd true;
  NBD.add_meta_context nbd NBD.context_base_allocation;
  NBD.connect_command nbd
                      ["nbdkit"; "-s"; "--exit-with-parent"; "-v";
                       "memory"; "size=1M"; "--no-sr"];
  let sr = NBD.get_structured_replies_negotiated nbd in
  assert (not sr);

  (* Expect server-side failure here *)
  count := 0;
  seen := false;
  NBD.add_meta_context nbd "base:";
  (try
     let _ = NBD.opt_set_meta_context nbd (f 42) in
     assert false
   with
     NBD.Error (errstr, errno) -> ()
  );
  assert (0 = !count);
  assert (not !seen);
  (try
     let _ = NBD.can_meta_context nbd NBD.context_base_allocation in
     assert false
   with
     NBD.Error (errstr, errno) -> ()
  );

  (* Even though can_meta fails after failed SET, it returns 0 after go *)
  NBD.opt_go nbd;
  let m = NBD.can_meta_context nbd NBD.context_base_allocation in
  assert (not m);

  NBD.shutdown nbd

let () = Gc.compact ()
