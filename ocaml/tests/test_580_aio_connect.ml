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

(* This test is unusual because we want to run it under nbdkit
 * rather than having the test run nbdkit as a subprocess.
 *
 * Therefore we detect if a $unixsocket parameter is passed
 * in Sys.argv.  If not then we exec nbdkit:
 *   nbdkit -U - ... --run '$argv0 \$unixsocket'
 * If the $unixsocket parameter is present then we run the test.
 *)

open Unix
open Printf

let () =
  let argv0 = Sys.argv.(0) in

  match Array.length Sys.argv with
  | 1 ->                        (* exec nbdkit *)
     let runcmd = sprintf "%s $unixsocket" (Filename.quote argv0) in
     execvp "nbdkit" [| "nbdkit"; "-U"; "-"; "--exit-with-parent"; "-v";
                        "memory"; "size=512";
                        "--run"; runcmd |]

  | 2 ->                        (* run the test *)
     let unixsocket = Sys.argv.(1) in

     (* Connect to the subprocess using a Unix.sockaddr. *)
     NBD.with_handle (
       fun nbd ->
         let sa = ADDR_UNIX unixsocket in
         NBD.aio_connect nbd sa;
         while NBD.aio_is_connecting nbd do
           ignore (NBD.poll nbd 1)
         done;
         assert (NBD.aio_is_ready nbd);
         assert (NBD.get_size nbd = 512_L)
     )

  | _ ->
     failwith (sprintf "%s: unexpected test parameters" argv0)

let () = Gc.compact ()
