(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* libnbd OCaml test case
 * Copyright (C) 2013-2023 Red Hat Inc.
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

(* Catch debug messages so we know when the handle was really closed. *)
let messages = ref []
let f context msg =
  messages := msg :: !messages;
  0

let () =
  (* Open the handle and then explicitly close it. *)
  let nbd = NBD.create () in
  NBD.set_debug nbd true;
  NBD.set_debug_callback nbd f;
  NBD.close nbd;

  (* Check the messages so we know the handle was closed. *)
  let closing_handle = (=) "closing handle" in
  assert (List.length (List.filter closing_handle !messages) = 1);

  (* Check that an exception is raised if we use any method on the handle. *)
  (try NBD.set_export_name nbd "test"
   with NBD.Closed _ -> (* expected *) ()
      | exn -> failwith ("unexpected exception: " ^ Printexc.to_string exn)
  );

  (try NBD.close nbd
   with NBD.Closed _ -> (* expected *) ()
      | exn -> failwith ("unexpected exception: " ^ Printexc.to_string exn)
  );

  Gc.compact ();

  (* Check the messages so we know the handle was closed. *)
  assert (List.length (List.filter closing_handle !messages) = 1)

let () = Gc.compact ()
