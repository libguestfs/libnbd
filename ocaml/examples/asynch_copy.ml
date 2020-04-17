open Unix
open Printf

let (+^) = Int64.add
let (-^) = Int64.sub

let bs = 65536_L
let max_reads_in_flight = 16

let dir_is_read dir = dir land (Int32.to_int NBD.aio_direction_read) <> 0
let dir_is_write dir = dir land (Int32.to_int NBD.aio_direction_write) <> 0

(* Copy between two libnbd handles using aynchronous I/O (AIO). *)
let asynch_copy src dst =
  let size = NBD.get_size dst in

  (* This is our reading position in the source. *)
  let soff = ref 0_L in

  (* This callback is called when any pread from the source has
   * completed.  (buf, size) contains the block of data.  This
   * builds up a queue of write commands which are sent on the
   * next iteration of the loop.
   *)
  let writes = ref [] in
  let read_completed buf offset _ =
    (* Get ready to issue a write command. *)
    writes := (buf, offset) :: !writes;
    (* By returning 1 here we auto-retire the pread command. *)
    1
  in

  (* This callback is called when any pwrite to the destination
   * has completed.
   *)
  let write_completed buf _ =
    (* By returning 1 here we auto-retire the pwrite command. *)
    1
  in

  (* The main loop which runs until we have finished reading and
   * there are no more commands in flight.
   *)
  while !soff < size || NBD.aio_in_flight src > 0 || NBD.aio_in_flight dst > 0
  do
    (* If we're able to submit more reads from the source then do so now. *)
    if !soff < size && NBD.aio_in_flight src < max_reads_in_flight then (
      let bs = min bs (size -^ !soff) in
      let buf = NBD.Buffer.alloc (Int64.to_int bs) in
      ignore (NBD.aio_pread src buf !soff
                ~completion:(read_completed buf !soff));
      soff := !soff +^ bs
    );

    (* If there are any write commands waiting to be issued to
     * the destination, send them now.
     *)
    List.iter (
      fun (buf, offset) ->
        (* Note the size of the write is implicitly stored in buf. *)
        ignore (NBD.aio_pwrite dst buf offset
                  ~completion:(write_completed buf))
    ) !writes;
    writes := [];

    let sfd = NBD.aio_get_fd src
    and dfd = NBD.aio_get_fd dst
    and sdir = NBD.aio_get_direction src
    and ddir = NBD.aio_get_direction dst in
    let rfds = if dir_is_read sdir then [sfd] else [] in
    let rfds = if dir_is_read ddir then dfd :: rfds else rfds in
    let wfds = if dir_is_write sdir then [sfd] else [] in
    let wfds = if dir_is_write ddir then dfd :: wfds else wfds in
    let rfds, wfds, _ = select rfds wfds [] (-1.0) in

    (* These can change since we slept in the select, so we must
     * check them again.
     *)
    let sdir = NBD.aio_get_direction src
    and ddir = NBD.aio_get_direction dst in
    if List.mem sfd rfds && dir_is_read sdir then
      NBD.aio_notify_read src
    else if List.mem sfd wfds && dir_is_write sdir then
      NBD.aio_notify_write src
    else if List.mem dfd rfds && dir_is_read ddir then
      NBD.aio_notify_read dst
    else if List.mem dfd wfds && dir_is_write ddir then
      NBD.aio_notify_write dst
  done

let () =
  let src = NBD.create () in
  NBD.set_handle_name src "src";
  let dst = NBD.create () in
  NBD.set_handle_name dst "dst";
  NBD.connect_command src ["nbdkit"; "-s"; "--exit-with-parent"; "-r";
                           "pattern"; "size=512M"];
  NBD.connect_command dst ["nbdkit"; "-s"; "--exit-with-parent";
                           "memory"; "size=512M"];
  asynch_copy src dst

(* This is a good way to find memory leaks or corruption. *)
let () = Gc.compact ()
