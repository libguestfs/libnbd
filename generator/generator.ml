(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* nbd client library in userspace: generator
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

open Unix
open Printf

open State_machine
open State_machine_generator
open Utils

let () =
  if not (Sys.file_exists "lib/handle.c") then
    failwith "Wrong directory!  Don't run this script by hand."

(* Write the output files. *)
let () =
  output_to "lib/states.h" State_machine_generator.generate_lib_states_h;
  output_to "lib/states.c" State_machine_generator.generate_lib_states_c;
  output_to "lib/states-run.c"
    State_machine_generator.generate_lib_states_run_c;

  output_to "lib/libnbd.syms" C.generate_lib_libnbd_syms;
  output_to "include/libnbd.h" C.generate_include_libnbd_h;
  output_to "lib/unlocked.h" C.generate_lib_unlocked_h;
  output_to "lib/api.c" C.generate_lib_api_c;
  output_to "docs/Makefile.inc" C.generate_docs_Makefile_inc;
  output_to "docs/api-links.pod" C.generate_docs_api_links_pod;
  output_to "docs/api-flag-links.pod" C.generate_docs_api_flag_links_pod;
  List.iter (
    fun (name, call) ->
      output_to (sprintf "docs/nbd_%s.pod" name)
                (C.generate_docs_nbd_pod name call)
  ) API.handle_calls;

  output_to "python/methods.h" Python.generate_python_methods_h;
  output_to "python/libnbdmod.c" Python.generate_python_libnbdmod_c;
  output_to "python/methods.c" Python.generate_python_methods_c;
  output_to "python/nbd.py" Python.generate_python_nbd_py;

  output_to "ocaml/NBD.mli" OCaml.generate_ocaml_nbd_mli;
  output_to "ocaml/NBD.ml" OCaml.generate_ocaml_nbd_ml;
  output_to "ocaml/nbd-c.c" OCaml.generate_ocaml_nbd_c;

  output_to "golang/src/libguestfs.org/libnbd/bindings.go"
    GoLang.generate_golang_bindings_go;
  output_to "golang/src/libguestfs.org/libnbd/closures.go"
    GoLang.generate_golang_closures_go;
  output_to "golang/src/libguestfs.org/libnbd/wrappers.go"
    GoLang.generate_golang_wrappers_go;
  output_to "golang/src/libguestfs.org/libnbd/wrappers.h"
    GoLang.generate_golang_wrappers_h;
