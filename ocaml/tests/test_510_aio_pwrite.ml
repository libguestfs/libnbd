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

open Unix

let () =
  let buf = Bytes.make 512 '\000' in
  Bytes.set buf 10 '\001';
  Bytes.set buf 510 '\x55';
  Bytes.set buf 511 '\xAA';

  let datafile, chan =
    Filename.open_temp_file ~mode:[Open_binary] "510" ".data" in
  let fd = descr_of_out_channel chan in
  ftruncate fd 512;
  close_out chan;

  let nbd = NBD.create () in
  NBD.connect_command nbd ["nbdkit"; "-s"; "--exit-with-parent"; "-v";
                           "file"; datafile];

  let buf1 = NBD.Buffer.of_bytes buf in
  let flags = let open NBD.CMD_FLAG in [FUA] in
  let cookie = NBD.aio_pwrite nbd buf1 0_L ~flags in
  while not (NBD.aio_command_completed nbd cookie) do
    ignore (NBD.poll nbd (-1))
  done;

  let buf2 = NBD.Buffer.alloc 512 in
  let cookie = NBD.aio_pread nbd buf2 0_L in
  while not (NBD.aio_command_completed nbd cookie) do
    ignore (NBD.poll nbd (-1))
  done;

  assert (buf = NBD.Buffer.to_bytes buf2);

  let fd = openfile datafile [O_RDONLY] 0 in
  let content = Bytes.create 512 in
  assert (512 = read fd content 0 512);
  close fd;
  assert (buf = content);

  unlink datafile

let () = Gc.compact ()
