(* This example is similar to nbdinfo <URI>.  It connects to an
 * NBD URI and queries various flags and metadata of the default
 * export.
 *
 * To try it out do this from the top build directory:
 *
 * nbdkit -U - null 1G --filter=exportname exportdesc=fixed:foo \
 *     --run './run ocaml/examples/server_flags.opt $uri'
 *)

open Printf

let () =
  if Array.length Sys.argv <> 2 then
    failwith "usage: server_flags URI";
  let uri = Sys.argv.(1) in

  NBD.with_handle (
    fun nbd ->
      (* Request full export description during negotiation. *)
      NBD.set_full_info nbd true;

      (* Connect to the NBD URI. *)
      NBD.connect_uri nbd uri;

      (* Print export information. *)
      (try
         printf "canonical name = %S\n"
           (NBD.get_canonical_export_name nbd)
       with NBD.Error _ -> ()
      );

      (try
         printf "export description = %S\n"
           (NBD.get_export_description nbd)
       with NBD.Error _ -> ()
      );

      (* Print various flags. *)
      let print_flag name fn =
        try printf "%s = %b\n" name (fn nbd)
        with NBD.Error _ -> ()
      in
      print_flag "can_cache" NBD.can_cache;
      print_flag "can_df" NBD.can_df;
      print_flag "can_fast_zero" NBD.can_fast_zero;
      print_flag "can_flush" NBD.can_flush;
      print_flag "can_fua" NBD.can_fua;
      print_flag "can_multi_conn" NBD.can_multi_conn;
      print_flag "can_trim" NBD.can_trim;
      print_flag "can_zero" NBD.can_zero;
      print_flag "is_read_only" NBD.is_read_only;
      print_flag "is_rotational" NBD.is_rotational;
  )
