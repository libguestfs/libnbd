(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* nbd client library in userspace: utilities
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

open Printf
open Unix

let failwithf fs = ksprintf failwith fs

let rec filter_map f = function
  | [] -> []
  | x :: xs ->
      match f x with
      | Some y -> y :: filter_map f xs
      | None -> filter_map f xs

(* group_by [1, "foo"; 2, "bar"; 2, "baz"; 2, "biz"; 3, "boo"; 4, "fizz"]
 * - : (int * string list) list =
 * [(1, ["foo"]); (2, ["bar"; "baz"; "biz"]); (3, ["boo"]); (4, ["fizz"])]
 *)
let rec group_by = function
| [] -> []
| (day1, x1) :: (day2, x2) :: rest when day1 = day2 ->
   let rest = group_by ((day2, x2) :: rest) in
   let day, xs = List.hd rest in
   (day, x1 :: xs) :: List.tl rest
| (day, x) :: rest ->
   (day, [x]) :: group_by rest

let uniq ?(cmp = compare) xs =
  let rec loop acc = function
    | [] -> acc
    | [x] -> x :: acc
    | x :: (y :: _ as xs) when cmp x y = 0 ->
       loop acc xs
    | x :: (y :: _ as xs) ->
       loop (x :: acc) xs
  in
  List.rev (loop [] xs)

(* This is present in OCaml 4.04, so we can remove it when
 * we depend on OCaml >= 4.04.
 *)
let sort_uniq ?(cmp = compare) xs =
  let xs = List.sort cmp xs in
  let xs = uniq ~cmp xs in
  xs

let is_prefix str prefix =
  let n = String.length prefix in
  String.length str >= n && String.sub str 0 n = prefix

let rec find s sub =
  let len = String.length s in
  let sublen = String.length sub in
  let rec loop i =
    if i <= len-sublen then (
      let rec loop2 j =
        if j < sublen then (
          if s.[i+j] = sub.[j] then loop2 (j+1)
          else -1
        ) else
          i (* found *)
      in
      let r = loop2 0 in
      if r = -1 then loop (i+1) else r
    ) else
      -1 (* not found *)
  in
  loop 0

let rec split sep str =
  let len = String.length sep in
  let seplen = String.length str in
  let i = find str sep in
  if i = -1 then str, ""
  else (
    String.sub str 0 i, String.sub str (i + len) (seplen - i - len)
  )

and nsplit sep str =
  if find str sep = -1 then
    [str]
  else (
    let s1, s2 = split sep str in
    s1 :: nsplit sep s2
  )

let char_mem c str = String.contains str c

let span str accept =
  let len = String.length str in
  let rec loop i =
    if i >= len then len
    else if char_mem (String.unsafe_get str i) accept then loop (i+1)
    else i
  in
  loop 0

let cspan str reject =
  let len = String.length str in
  let rec loop i =
    if i >= len then len
    else if char_mem (String.unsafe_get str i) reject then i
    else loop (i+1)
  in
  loop 0

(* Last output column. *)
let col = ref 0

type chan = NoOutput | OutChannel of out_channel | Buffer of Buffer.t
let chan = ref NoOutput
let pr fs =
  ksprintf (
    fun str ->
      (* Maintain the current output column.  We can simply do this
       * by counting backwards from the end of the string until we
       * reach a \n (or the beginning).  The number we count is
       * the new column.  This only works for 7 bit ASCII but
       * that's enough for what we need this for.
       *)
      let rec loop i acc =
        if i >= 0 then (
          if String.unsafe_get str i = '\n' then acc
          else loop (i-1) (acc+1)
        )
        else !col + acc
      in
      col := loop (String.length str - 1) 0;
      match !chan with
      | NoOutput -> failwithf "use ‘output_to’ to set output"
      | OutChannel chan -> output_string chan str
      | Buffer b -> Buffer.add_string b str
  ) fs

let pr_wrap ?(maxcol = 76) c code =
  (* Save the current output channel and replace it with a
   * temporary buffer while running ‘code’.  Then we wrap the
   * buffer and write it to the restored channel.
   *)
  let old_chan = !chan in
  let wrapping_col = !col in
  let b = Buffer.create 1024 in
  chan := Buffer b;
  let exn = try code (); None with exn -> Some exn in
  chan := old_chan;
  col := wrapping_col;
  (match exn with None -> () | Some exn -> raise exn);

  let lines = nsplit "\n" (Buffer.contents b) in
  match lines with
  | [] -> ()
  | line :: rest ->
     let fields = nsplit (String.make 1 c) line in
     let maybe_wrap field =
       if !col > wrapping_col && !col + String.length field >= maxcol then (
         pr "\n";
         for i = 0 to wrapping_col-1 do pr " " done;
         match span field " \t" with
         | 0 -> field
         | i -> String.sub field i (String.length field - i)
       )
       else field
     in
     let rec loop = function
       | [] -> ()
       | f :: [] -> let f = maybe_wrap f in pr "%s" f;
       | f :: fs -> let f = maybe_wrap f in pr "%s%c" f c; loop fs
     in
     loop fields;

     (* There should really only be one line in the buffer, but
      * if there are multiple apply wrapping to only the first one.
      *)
     pr "%s" (String.concat "\n" rest)

type comment_style =
  | CStyle | CPlusPlusStyle | HashStyle | OCamlStyle | HaskellStyle
  | PODCommentStyle

let generate_header ?(extra_sources = []) comment_style =
  let inputs = "generator/generator" :: extra_sources in
  let c = match comment_style with
    | CStyle ->         pr "/* "; " *"
    | CPlusPlusStyle -> pr "// "; "//"
    | HashStyle ->      pr "# ";  "#"
    | OCamlStyle ->     pr "(* "; " *"
    | HaskellStyle ->   pr "{- "; "  "
    | PODCommentStyle -> pr "=begin comment\n\n "; "" in
  pr "NBD client library in userspace\n";
  pr "%s WARNING: THIS FILE IS GENERATED FROM\n" c;
  pr "%s %s\n" c (String.concat " " inputs);
  pr "%s ANY CHANGES YOU MAKE TO THIS FILE WILL BE LOST.\n" c;
  pr "%s\n" c;
  pr "%s Copyright (C) 2013-2020 Red Hat Inc.\n" c;
  pr "%s\n" c;
  pr "%s This library is free software; you can redistribute it and/or\n" c;
  pr "%s modify it under the terms of the GNU Lesser General Public\n" c;
  pr "%s License as published by the Free Software Foundation; either\n" c;
  pr "%s version 2 of the License, or (at your option) any later version.\n" c;
  pr "%s\n" c;
  pr "%s This library is distributed in the hope that it will be useful,\n" c;
  pr "%s but WITHOUT ANY WARRANTY; without even the implied warranty of\n" c;
  pr "%s MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU\n" c;
  pr "%s Lesser General Public License for more details.\n" c;
  pr "%s\n" c;
  pr "%s You should have received a copy of the GNU Lesser General Public\n" c;
  pr "%s License along with this library; if not, write to the Free Software\n" c;
  pr "%s Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA\n" c;
  (match comment_style with
   | CStyle -> pr " */\n"
   | CPlusPlusStyle
   | HashStyle -> ()
   | OCamlStyle -> pr " *)\n"
   | HaskellStyle -> pr "-}\n"
   | PODCommentStyle -> pr "\n=end comment\n"
  );
  pr "\n"

let quote = Filename.quote

let files_equal n1 n2 =
  let cmd = sprintf "cmp -s %s %s" (quote n1) (quote n2) in
  match Sys.command cmd with
  | 0 -> true
  | 1 -> false
  | i -> failwithf "%s: failed with error code %d" cmd i

let output_to filename k =
  let filename_new = filename ^ ".new" in
  let c = open_out filename_new in
  chan := OutChannel c;
  k ();
  close_out c;
  chan := NoOutput;

  (* Is the new file different from the current file? *)
  if Sys.file_exists filename && files_equal filename filename_new then
    unlink filename_new                 (* same, so skip it *)
  else (
    (* different, overwrite old one *)
    (try chmod filename 0o644 with Unix_error _ -> ());
    rename filename_new filename;
    chmod filename 0o444;
    printf "written %s\n%!" filename;
  )

(* Convert POD fragments into plain text.
 *
 * For man pages and Perl documentation we can simply use the POD
 * directly, and that is the best solution.  However for other
 * programming languages we have to convert the POD fragments to
 * plain text by running it through pod2text.
 *
 * The problem is that pod2text is very slow so we must cache
 * the converted fragments to disk.
 *
 * Increment the version in the filename whenever the cache
 * type changes.
 *)

type cache_key = string (* longdesc *)
type cache_value = string list (* list of plain text lines *)

let (cache : (cache_key, cache_value) Hashtbl.t), save_cache =
  let cachefile = "generator/generator-cache.v1" in
  let cache =
    try
      let chan = open_in cachefile in
      let ret = input_value chan in
      close_in chan;
      ret
    with _ ->
      printf "Regenerating the cache, this could take a little while ...\n%!";
      Hashtbl.create 13 in
  let save_cache () =
    let chan = open_out cachefile in
    output_value chan cache;
    close_out chan
  in
  cache, save_cache

let pod2text longdesc =
  let key : cache_key = longdesc in
  try Hashtbl.find cache key
  with Not_found ->
    let filename, chan = Filename.open_temp_file "pod2text" ".tmp" in
    fprintf chan "=encoding utf8\n\n";
    fprintf chan "=head1 NAME\n\n%s\n" longdesc;
    close_out chan;
    let cmd = sprintf "pod2text -w 60 %s" (quote filename) in
    let chan = open_process_in cmd in
    let lines = ref [] in
    let rec loop i =
      let line = input_line chan in
      if i = 1 then (* discard first line of output *)
        loop (i+1)
      else (
        lines := line :: !lines;
        loop (i+1)
      ) in
    let lines : cache_value =
      try loop 1 with End_of_file -> List.rev !lines in
    unlink filename;
    (match close_process_in chan with
     | WEXITED 0 -> ()
     | WEXITED i ->
        failwithf "pod2text: process exited with non-zero status (%d)" i
     | WSIGNALED i | WSTOPPED i ->
        failwithf "pod2text: process signalled or stopped by signal %d" i
    );
    Hashtbl.add cache key lines;
    save_cache ();
    lines
