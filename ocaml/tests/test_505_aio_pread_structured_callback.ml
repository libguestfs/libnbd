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

let chunk user_data buf2 offset s err =
  assert (!err = 0);
  err := 100;
  assert (user_data = 42);
  assert (buf2 = expected);
  assert (offset = 0_L);
  assert (s = Int32.to_int NBD.read_data)

let callback user_data cookie err =
  if user_data <= 43 then
    assert (!err = 0)
  else
    assert (!err = 100);
  assert (cookie > Int64.of_int (0));
  err := 101;
  assert (user_data = 42)

let () =
  let nbd = NBD.create () in
  NBD.connect_command nbd ["nbdkit"; "-s"; "--exit-with-parent"; "-v";
                           "pattern"; "size=512"];

  (* First try: succeed in both callbacks *)
  let buf = NBD.Buffer.of_bytes (Bytes.create 512) in
  let cookie = NBD.aio_pread_structured_callback nbd buf 0_L (chunk 42) (callback 42) in
  while not (NBD.aio_command_completed nbd cookie) do
    ignore (NBD.poll nbd (-1))
  done;

  let buf = NBD.Buffer.to_bytes buf in

  assert (buf = expected);

  (* Second try: fail only during callback *)
  let buf = NBD.Buffer.of_bytes (Bytes.create 512) in
  let cookie = NBD.aio_pread_structured_callback nbd buf 0_L (chunk 42) (callback 43) in
  try
    while not (NBD.aio_command_completed nbd cookie) do
      ignore (NBD.poll nbd (-1))
    done;
    assert false
  with
    NBD.Error (_, errno) ->
      printf "errno = %d\n%!" errno;
      assert (errno = 101);

  (* Third try: fail during both *)
  let buf = NBD.Buffer.of_bytes (Bytes.create 512) in
  let cookie = NBD.aio_pread_structured_callback nbd buf 0_L (chunk 43) (callback 44) in
  try
    while not (NBD.aio_command_completed nbd cookie) do
      ignore (NBD.poll nbd (-1))
    done;
    assert false
  with
    NBD.Error (_, errno) ->
      printf "errno = %d\n%!" errno;
      assert (errno = 101);

  (* Fourth try: fail only during chunk *)
  let buf = NBD.Buffer.of_bytes (Bytes.create 512) in
  let cookie = NBD.aio_pread_structured_callback nbd buf 0_L (chunk 43) (callback 42) in
  try
    while not (NBD.aio_command_completed nbd cookie) do
      ignore (NBD.poll nbd (-1))
    done;
    assert false
  with
    NBD.Error (_, errno) ->
      printf "errno = %d\n%!" errno;
      assert (errno = 100)
