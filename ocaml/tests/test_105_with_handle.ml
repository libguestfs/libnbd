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

exception Test

let () =
  NBD.with_handle (fun nbd -> ());

  (try
     ignore (NBD.with_handle (fun nbd -> raise Test));
     assert false
   with Test -> () (* expected *)
      | exn -> failwith (Printexc.to_string exn)
  );

  (* Were two handles created above?
   * XXX How to test if close was called twice?
   *)
  let h = NBD.get_handle_name (NBD.create ()) in
  assert (h = "nbd3")

let () = Gc.compact ()
