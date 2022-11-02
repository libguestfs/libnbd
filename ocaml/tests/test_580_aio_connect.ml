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

open Unix
open Printf

let () =
  let nbd = NBD.create () in

  (* Unlike other tests, we're going to run nbdkit as a subprocess
   * by hand and have it listening on a randomly named socket
   * that we create.
   *)
  let sock = Filename.temp_file "580-" ".sock" in
  unlink sock;
  let pidfile = Filename.temp_file "580-" ".pid" in
  unlink pidfile;
  let cmd =
    sprintf "nbdkit -U %s -P %s --exit-with-parent memory size=512 &"
      (Filename.quote sock) (Filename.quote pidfile) in
  if Sys.command cmd <> 0 then
    failwith "nbdkit command failed";
  let rec loop i =
    if i > 60 then
      failwith "nbdkit subcommand did not start up";
    if not (Sys.file_exists pidfile) then (
      sleep 1;
      loop (i+1)
    )
  in
  loop 0;

  (* Connect to the subprocess using a Unix.sockaddr. *)
  let sa = ADDR_UNIX sock in
  NBD.aio_connect nbd sa;
  while NBD.aio_is_connecting nbd do
    ignore (NBD.poll nbd 1)
  done;
  assert (NBD.aio_is_ready nbd);
  NBD.close nbd;

  (* Kill the nbdkit subprocess. *)
  let chan = open_in pidfile in
  let pid = int_of_string (input_line chan) in
  kill pid Sys.sigint;

  (* Clean up files. *)
  unlink sock;
  unlink pidfile

let () = Gc.compact ()
