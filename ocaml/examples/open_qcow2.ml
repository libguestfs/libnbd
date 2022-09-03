(* Open a qcow2 file using an external NBD server (qemu-nbd). *)

open Printf

let () =
  if Array.length Sys.argv <> 2 then
    failwith "usage: open_qcow2 file.qcow2";
  let filename = Sys.argv.(1) in

  NBD.with_handle (
    fun nbd ->
      (* Run qemu-nbd as a subprocess using
       * systemd socket activation.
       *)
      let args = [ "qemu-nbd"; "-f"; "qcow2"; filename ] in
      NBD.connect_systemd_socket_activation nbd args;

      (* Read the first sector of raw data. *)
      let buf = Bytes.create 512 and offset = 0_L in
      NBD.pread nbd buf offset;

      (* Hexdump it. *)
      let chan = Unix.open_process_out "hexdump -C" in
      output_bytes chan buf;
      ignore (Unix.close_process_out chan)

      (* The qemu-nbd server will exit when we close the
       * handle (implicitly closed by NBD.with_handle).
       *)
  )
