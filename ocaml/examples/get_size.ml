(* This example is similar to nbdinfo --size <URI>.  It connects to an
 * NBD URI and queries the size of the default export.  To try it out
 * do this from the top build directory:
 *
 * nbdkit -U - null 1G --run './run ocaml/examples/get_size.opt $uri'
 *)

open Printf

let () =
  if Array.length Sys.argv <> 2 then
    failwith "usage: get_size URI";
  let uri = Sys.argv.(1) in

  NBD.with_handle (
    fun nbd ->
      (* Connect to the NBD URI. *)
      NBD.connect_uri nbd uri;

      let size = NBD.get_size nbd in
      printf "uri = %s size = %Ld\n" uri size
  )
