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

let () =
  let nbd = NBD.create () in

  (* Pre-connection, stats start out at 0 *)
  let bs0 = NBD.stats_bytes_sent nbd in
  let cs0 = NBD.stats_chunks_sent nbd in
  let br0 = NBD.stats_bytes_received nbd in
  let cr0 = NBD.stats_chunks_received nbd in
  assert (bs0 = 0L);
  assert (cs0 = 0L);
  assert (br0 = 0L);
  assert (cr0 = 0L);

  (* Connection performs handshaking, which increments stats.
   * The number of bytes/chunks here may grow over time as more features get
   * automatically negotiated, so merely check that they are non-zero.
   *)
  NBD.connect_command nbd ["nbdkit"; "-s"; "--exit-with-parent"; "null"];

  let bs1 = NBD.stats_bytes_sent nbd in
  let cs1 = NBD.stats_chunks_sent nbd in
  let br1 = NBD.stats_bytes_received nbd in
  let cr1 = NBD.stats_chunks_received nbd in
  assert (cs1 > 0L);
  assert (bs1 > cs1);
  assert (cr1 > 0L);
  assert (br1 > cr1);

  (* A flush command should be one chunk out, one chunk back (even if
   * structured replies are in use)
   *)
  NBD.flush nbd;

  let bs2 = NBD.stats_bytes_sent nbd in
  let cs2 = NBD.stats_chunks_sent nbd in
  let br2 = NBD.stats_bytes_received nbd in
  let cr2 = NBD.stats_chunks_received nbd in
  assert (bs2 = (Int64.add bs1 28L));
  assert (cs2 = (Int64.succ cs1));
  assert (br2 = (Int64.add br1 16L));  (* assumes nbdkit uses simple reply *)
  assert (cr2 = (Int64.succ cr1));

  (* Stats are still readable after the connection closes; we don't know if
   * the server sent reply bytes to our NBD_CMD_DISC, so don't insist on it.
   *)
  NBD.shutdown nbd;

  let bs3 = NBD.stats_bytes_sent nbd in
  let cs3 = NBD.stats_chunks_sent nbd in
  let br3 = NBD.stats_bytes_received nbd in
  let cr3 = NBD.stats_chunks_received nbd in
  let fudge = if cr2 = cr3 then 0L else 1L in
  assert (bs3 > bs2);
  assert (cs3 = (Int64.succ cs2));
  assert (br3 >= br2);
  assert (cr3 = (Int64.add cr2 fudge))

let () = Gc.compact ()
