(* libnbd OCaml test case
 * Copyright (C) 2013-2019 Red Hat Inc.
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

(* NB: OCaml 4.08 has endian functions in the Bytes module which
 * would make this loop much simpler.
 *)
let expected =
  let b = Bytes.create 512 in
  for i = 0 to 512/8-1 do
    let i64 = ref (Int64.of_int (i*8)) in
    for j = 0 to 7 do
      let c = Int64.shift_right_logical !i64 56 in
      let c = Int64.to_int c in
      let c = Char.chr c in
      Bytes.unsafe_set b (i*8+j) c;
      i64 := Int64.shift_left !i64 8
    done
  done;
  b

let f user_data buf2 offset s err =
  assert (!err = 0);
  err := 100;
  if user_data <> 42 then invalid_arg "this should be turned into NBD.Error";
  assert (buf2 = expected);
  assert (offset = 0_L);
  assert (s = Int32.to_int NBD.read_data);
  0

let () =
  let nbd = NBD.create () in
  NBD.connect_command nbd
                      ["nbdkit"; "-s"; "--exit-with-parent"; "-v";
                       "pattern"; "size=512"];
  let buf = Bytes.create 512 in

  NBD.pread_structured nbd buf 0_L (f 42);
  assert (buf = expected);

  let flags = let open NBD.CMD_FLAG in [DF] in
  NBD.pread_structured nbd buf 0_L (f 42) ~flags;
  assert (buf = expected);

  try
    NBD.pread_structured nbd buf 0_L (f 43) ~flags;
    assert false
  with
    NBD.Error (_, errno) ->
      printf "errno = %d\n%!" errno;
      assert (errno = 100)

let () = Gc.compact ()
