(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* libnbd OCaml test case
 * Copyright Red Hat
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

let () =
  let nbd = NBD.create () in

  try
    (* This will always throw an exception because the handle is not
     * connected.
     *)
    NBD.pread nbd (Bytes.create 512) 0_L
  with
    NBD.Error (errstr, errno) ->
      printf "string = %s\n" errstr;
      printf "errno = %s\n"
        (match errno with None -> "None" | Some err -> Unix.error_message err);
      Gc.compact ();
      exit 0 ;;

(* If we reach here then we didn't catch the exception above. *)
exit 1
