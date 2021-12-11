(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* libnbd OCaml test case
 * Copyright (C) 2013-2020 Red Hat Inc.
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
  NBD.set_export_name nbd "name";
  let name = NBD.get_export_name nbd in
  assert (name = "name");
  NBD.set_full_info nbd true;
  let info = NBD.get_full_info nbd in
  assert (info = true);
  (try
     NBD.set_tls nbd (NBD.TLS.UNKNOWN 3);
     assert (false)
   with
     NBD.Error _ -> ()
  );
  let tls = NBD.get_tls nbd in
  assert (tls = NBD.TLS.DISABLE);
  if NBD.supports_tls nbd then (
    NBD.set_tls nbd NBD.TLS.ALLOW;
    let tls = NBD.get_tls nbd in
    assert (tls = NBD.TLS.ALLOW);
  );
  NBD.set_request_structured_replies nbd false;
  let sr = NBD.get_request_structured_replies nbd in
  assert (sr = false);
  (try
     NBD.set_handshake_flags nbd [ NBD.HANDSHAKE_FLAG.UNKNOWN 2 ];
     assert false
   with
     NBD.Error _ -> ()
  );
  let flags = NBD.get_handshake_flags nbd in
  assert (flags = NBD.HANDSHAKE_FLAG.mask);
  NBD.set_handshake_flags nbd [];
  let flags = NBD.get_handshake_flags nbd in
  assert (flags = []);
  NBD.set_opt_mode nbd true;
  let opt = NBD.get_opt_mode nbd in
  assert (opt = true)

let () = Gc.compact ()
