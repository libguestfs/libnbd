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
  NBD.set_opt_mode nbd true;
  NBD.connect_command nbd
                      ["nbdkit"; "-s"; "--exit-with-parent"; "-v"; "null"];
  let proto = NBD.get_protocol nbd in
    assert (proto = "newstyle-fixed");
  let sr = NBD.get_structured_replies_negotiated nbd in
    assert (sr);
  NBD.opt_abort nbd;
  let closed = NBD.aio_is_closed nbd in
    assert (closed);
  try
    let _ = NBD.get_size nbd in
      assert (false)
  with
    NBD.Error (errstr, errno) ->
      ()

let () = Gc.compact ()
