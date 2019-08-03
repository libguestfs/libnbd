open Printf

let () =
  let nbd = NBD.create () in
  NBD.add_meta_context nbd "base:allocation";
  NBD.connect_command nbd
                      ["nbdkit"; "-s"; "--exit-with-parent"; "memory"; "size=128K"];

  (* Write some sectors. *)
  let data_sector = Bytes.make 512 'a' in
  let zero_sector = Bytes.make 512 '\000' in
  NBD.pwrite nbd data_sector 0_L;
  NBD.pwrite nbd zero_sector 32768_L;
  NBD.pwrite nbd data_sector 65536_L;

  (* Read the extents and print them. *)
  let size = NBD.get_size nbd in
  NBD.block_status nbd size 0_L (
    fun meta _ entries err ->
      printf "err=%d\n" !err;
      if meta = "base:allocation" then (
        printf "index\tlength\tflags\n";
        for i = 0 to Array.length entries / 2 - 1 do
          let flags =
            match entries.(i*2+1) with
            | 0_l -> "data"
            | 1_l -> "hole"
            | 2_l -> "zero"
            | 3_l -> "hole+zero"
            | i -> sprintf "unknown (%ld)" i in
          printf "%d:\t%ld\t%s\n" i entries.(i*2) flags
        done
      );
      0
  )
