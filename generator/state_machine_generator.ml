(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* nbd client library in userspace: generate the code for the state machine
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

open State_machine
open Utils

let c_string_of_external_event = function
  | NotifyRead -> "notify_read"
  | NotifyWrite -> "notify_write"
  | CmdCreate -> "cmd_create"
  | CmdConnectSockAddr -> "cmd_connect_sockaddr"
  | CmdConnectTCP -> "cmd_connect_tcp"
  | CmdConnectCommand -> "cmd_connect_command"
  | CmdConnectSA -> "cmd_connect_sa"
  | CmdConnectSocket -> "cmd_connect_socket"
  | CmdIssue -> "cmd_issue"

(* Find a state in the state machine hierarchy by path.  The [path]
 * parameter is a list like [["READY"]] or [["MAGIC"; "START"]].
 *)
let find_state path =
  let rec find sm = function
    | [] -> raise Not_found
    | [n] ->
       (* Find a state leaf node. *)
       let rec loop = function
         | [] -> raise Not_found
         | State ({ name } as ret) :: _ when n = name -> ret
         | _ :: rest -> loop rest
       in
       loop sm
    | g :: path ->
       (* Find a state machine group. *)
       let rec loop = function
         | [] -> raise Not_found
         | Group (name, group) :: _ when g = name -> find group path
         | _ :: rest -> loop rest
       in
       loop sm
  in
  try (find state_machine path : state)
  with Not_found ->
       failwithf "find_state: ‘%s’ not found" (String.concat "." path)

let dot_rex = Str.regexp "\\."

(* Resolve a stringified path to a state.
 *
 * [prefix] parameter is the current prefix.  We resolve paths
 * relative to this.
 *
 * Stringified paths can be:
 * ["STATE"] => state relative to current level
 * ["GROUP.STATE"] => state below group at current level (to any depth)
 * [".TOP"] => state at the top level
 * ["^UP"] => state at a level above this one
 *)
let rec resolve_state prefix str =
  let len = String.length str in
  if len >= 1 && String.sub str 0 1 = "." then
    resolve_state [] (String.sub str 1 (len-1))
  else if len >= 1 && String.sub str 0 1 = "^" then (
    let parent =
      match List.rev prefix with
      | [] -> failwithf "resolve_state: %s (^) used from top level group" str
      | _ :: rest -> List.rev rest in
    resolve_state parent (String.sub str 1 (len-1))
  )
  else (
    let path = Str.split_delim dot_rex str in
    find_state (prefix @ path)
  )

(* Flatten the state machine hierarchy.  This sets the [parsed.prefix],
 * [parsed.state_enum], [parsed.events] fields in the state.
 *)
let states : state list =
  let rec flatten prefix = function
    | [] -> []
    | State st :: rest ->
       st.parsed <-
         { st.parsed with
           prefix = prefix;
           display_name = (
             match prefix with
             | [] -> st.name
             | prefix -> String.concat "." prefix ^ "." ^ st.name
           );
           state_enum = (
             let path = String.concat "" (List.map ((^) "_") prefix) in
             "STATE" ^ path ^ "_" ^ st.name
           );
           events = (
             List.map (
               fun (ev, str) ->
                 (* In external_events,
                  * special string [""] means current state.
                  *)
                 if str = "" then ev, st
                 else ev, resolve_state prefix str
             ) st.external_events
           )
         };
       st :: flatten prefix rest
    | Group (name, group) :: rest ->
       let states = flatten (prefix @ [name]) group in
       states @ flatten prefix rest
  in
  flatten [] state_machine

(* Read and parse the state machine C code. *)
let state_machine_prologue =
  let parse_states_file filename =
    let chan = open_in filename in
    let lines = ref [] in
    let lineno = ref 1 in
    (try while true do
           let line = input_line chan in
           let loc : location = filename, !lineno in
           incr lineno;
           lines := (loc, line) :: !lines
         done
     with End_of_file -> ());
    close_in chan;
    (* Note this list is initially in reverse order. *)
    let lines = !lines in

    (* The last line in the file must have a particular form, check
     * and remove.
     *)
    if List.length lines = 0 ||
         snd (List.hd lines) <> "} /* END STATE MACHINE */" then
      failwithf "%s: unexpected file ending" filename;
    let lines = List.tl lines in
    let lines = List.rev lines in

    (* Find the start of the state machine and split the list into
     * the prologue and the list of state code fragments.
     *)
    let rec loop acc = function
      | [] -> failwithf "%s: could not find state machine" filename
      | (_, "STATE_MACHINE {") :: lines -> ((filename, 1), acc), lines
      | (_, line) :: lines -> loop (acc ^ line ^ "\n") lines
    in
    let prologue, lines = loop "" lines in

    let statecodes = ref [] in
    let curr_state = ref None in
    let rex = Str.regexp "^ \\([A-Z0-9][A-Z0-9_\\.]*\\):$" in
    List.iter (
      fun (loc, line) ->
        if Str.string_match rex line 0 then ( (* new case *)
          (match !curr_state with
           | None -> ()
           | Some state -> statecodes := state :: !statecodes);
          curr_state := Some (Str.matched_group 1 line, "", loc);
        )
        else (
          (match !curr_state with
           | None ->
              failwithf "%s: missing label" (string_of_location loc)
           | Some (name, code, loc) ->
              curr_state := Some (name, code ^ "\n" ^ line, loc)
          )
        );
      ) lines;
    (match !curr_state with
     | None -> ()
     | Some state -> statecodes := state :: !statecodes);
    let statecodes = List.rev !statecodes in

    prologue, statecodes
  in

  (* Read all the input files, called [generator/states*.c] *)
  let files = List.sort compare (Array.to_list (Sys.readdir "generator")) in
  let files = List.filter (
    fun filename ->
      let len = String.length filename in
      len >= 8 && String.sub filename 0 6 = "states" &&
        String.sub filename (len-2) 2 = ".c"
  ) files in
  let files = List.map ((^) "generator/") files in
  let files = List.map parse_states_file files in

  (* Mash together the prologues and the state codes. *)
  let prologue =
    String.concat "" (
      List.map (
        fun ((loc, prologue), _) ->
          line_directive_of_location loc ^ "\n" ^ prologue ^ "\n"
       ) files
     ) in
  let statecodes = List.concat (List.map snd files) in

  (* Resolve the state names in the code to paths. *)
  let statecodes =
    List.map (
      fun (name, code, loc) ->
        let path = Str.split_delim dot_rex name in
        let state = find_state path in
        state, code, loc
    ) statecodes in

  (* Parse the state code fragments to get internal state
   * transitions, marked by "%STATE".
   *)
  let rex = Str.regexp "%\\([\\^\\.]*[A-Z0-9][A-Z0-9_\\.]*\\)" in
  List.iter (
    fun (state, code, loc) ->
      let code = Str.full_split rex code in
      let code =
        List.map (
          function
          | Str.Delim str ->
             Str.Delim (String.sub str 1 (String.length str - 1))
          | (Str.Text _) as c -> c
      ) code in

      (* Create the list of internal transitions. *)
      state.parsed <-
        { state.parsed with
          internal_transitions = (
            filter_map (
              function
              | Str.Delim str ->
                 let next_state = resolve_state state.parsed.prefix str in
                 Some next_state
              | Str.Text _ -> None
            ) code
        )
        };

      (* Create the final C code fragment. *)
      state.parsed <-
        { state.parsed with
          loc = loc;
          code =
            String.concat "" (
              List.map (
                function
                | Str.Delim str ->
                   let next_state = resolve_state state.parsed.prefix str in
                   next_state.parsed.state_enum
                | Str.Text text -> text
                ) code
            )
        }
  ) statecodes;

  prologue

(* Verify state transitions are permitted. *)
let () =
  let verify_state_transition from_state to_state =
    let from_prefix = from_state.parsed.prefix
    and to_prefix = to_state.parsed.prefix in
    (* Going from state to state within the same group is always allowed. *)
    if from_prefix = to_prefix then
      ()
    (* Going upwards to any state is always allowed. *)
    else if List.length from_prefix > List.length to_prefix then
      ()
    (* When going downwards (even into an adjacent tree) you must
     * always enter a group at the START state.
     *)
    else if to_state.name <> "START" then (
      failwithf "non-permitted state transition: %s.%s -> %s.%s"
                (String.concat "." from_prefix) from_state.name
                (String.concat "." to_prefix) to_state.name
    )
  in
  List.iter (
    fun ({ parsed = { internal_transitions; events } } as state) ->
      List.iter (verify_state_transition state) internal_transitions;
      List.iter (fun (_, next_state) -> verify_state_transition state next_state) events
  ) states

(* Write the state machine code. *)
let generate_lib_states_h () =
  generate_header ~extra_sources:["generator/states*.c"] CStyle;
  pr "enum state {\n";
  List.iter (
    fun ({ comment; parsed = { display_name; state_enum } }) ->
      pr "  /* %s: %s */\n" display_name comment;
      pr "  %s,\n" state_enum;
      pr "\n";
  ) states;
  pr "};\n";
  pr "\n";
  pr "/* These correspond to the external events in generator/generator. */\n";
  pr "enum external_event {\n";
  List.iter (
    fun e -> pr "  %s,\n" (c_string_of_external_event e)
  ) all_external_events;
  pr "};\n";
  pr "\n";
  pr "/* State groups. */\n";
  pr "enum state_group {\n";
  pr "  GROUP_TOP,\n";
  let rec loop prefix = function
    | [] -> ()
    | State _ :: rest ->
       loop prefix rest
    | Group (name, group) :: rest ->
       let enum =
         "GROUP" ^ String.concat "" (List.map ((^) "_") prefix) ^ "_" ^ name in
       pr "  %s,\n" enum;
       loop (prefix @ [name]) group;
       loop prefix rest
  in
  loop [] state_machine;
  pr "};\n";
  pr "\n";
  pr "/* State transitions defined in states.c. */\n";
  List.iter (
    fun { parsed = { state_enum } } ->
      pr "extern int nbd_internal_enter_%s (struct nbd_handle *h, bool *blocked);\n" state_enum;
  ) states

let generate_lib_states_c () =
  generate_header ~extra_sources:["generator/states*.c"] CStyle;

  pr "%s\n" state_machine_prologue;
  pr "\n";
  pr "#define SET_NEXT_STATE(s) (*blocked = false, *next_state = (s))\n";
  pr "#define SET_NEXT_STATE_AND_BLOCK(s) (*next_state = (s))\n";

  (* The state machine C code fragments. *)
  List.iter (
    fun { comment; parsed = { display_name; state_enum; loc; code } } ->
      pr "\n";
      pr "/* %s: %s */\n" display_name comment;
      pr "static int\n";
      pr "enter_%s (struct nbd_handle *h,\n" state_enum;
      pr "             enum state *next_state,\n";
      pr "             bool *blocked)\n";
      pr "{\n";
      if code <> "" then (
        pr "%s\n" (line_directive_of_location loc);
        pr "%s\n" code
      )
      else
        pr "  return 0;\n";
      pr "}\n";
      pr "\n";
      pr "int\n";
      pr "nbd_internal_enter_%s (struct nbd_handle *h, bool *blocked)\n"
        state_enum;
      pr "{\n";
      pr "  int r;\n";
      pr "  enum state next_state = %s;\n" state_enum;
      pr "\n";
      pr "  r = enter_%s (h, &next_state, blocked);\n" state_enum;
      pr "  if (get_next_state (h) != next_state) {\n";
      pr "    debug (h, \"transition: %%s -> %%s\",\n";
      pr "           \"%s\",\n" display_name;
      pr "           nbd_internal_state_short_string (next_state));\n";
      pr "    set_next_state (h, next_state);\n";
      pr "  }\n";
      pr "  return r;\n";
      pr "}\n";
  ) states

let generate_lib_states_run_c () =
  generate_header ~extra_sources:["generator/states*.c"] CStyle;

  pr "#include <config.h>\n";
  pr "\n";
  pr "#include <stdio.h>\n";
  pr "#include <stdlib.h>\n";
  pr "#include <errno.h>\n";
  pr "#include <assert.h>\n";
  pr "\n";
  pr "#include \"libnbd.h\"\n";
  pr "#include \"internal.h\"\n";
  pr "\n";
  pr "/* Run the state machine based on an external event until it would block. */\n";
  pr "int\n";
  pr "nbd_internal_run (struct nbd_handle *h, enum external_event ev)\n";
  pr "{\n";
  pr "  int r;\n";
  pr "  bool blocked;\n";
  pr "\n";
  pr "  /* Validate and handle the external event. */\n";
  pr "  switch (get_next_state (h))\n";
  pr "  {\n";
  List.iter (
    fun ({ parsed = { display_name; state_enum; events } } as state) ->
      pr "  case %s:\n" state_enum;
      if events <> [] then (
        pr "    switch (ev)\n";
        pr "    {\n";
        List.iter (
          fun (e, next_state) ->
            pr "    case %s:\n" (c_string_of_external_event e);
            if state != next_state then (
              pr "      set_next_state (h, %s);\n" next_state.parsed.state_enum;
              pr "      debug (h, \"event %%s: %%s -> %%s\",\n";
              pr "             \"%s\", \"%s\", \"%s\");\n"
                 (string_of_external_event e)
                 display_name next_state.parsed.display_name;
            );
            pr "      goto ok;\n";
        ) events;
        pr "    default: ; /* nothing, silence GCC warning */\n";
        pr "    }\n";
      );
      pr "    break;\n";
  ) states;
  pr "  }\n";
  pr "\n";
  pr "  set_error (EINVAL, \"external event %%d is invalid in state %%s\",\n";
  pr "             ev, nbd_internal_state_short_string (get_next_state (h)));\n";
  pr "  return -1;\n";
  pr "\n";
  pr " ok:\n";
  pr "  do {\n";
  pr "    blocked = true;\n";
  pr "\n";
  pr "    /* Run a single step. */\n";
  pr "    switch (get_next_state (h))\n";
  pr "    {\n";
  List.iter (
    fun { parsed = { state_enum } } ->
      pr "    case %s:\n" state_enum;
      pr "      r = nbd_internal_enter_%s (h, &blocked);\n" state_enum;
      pr "      break;\n"
  ) states;
  pr "    default:\n";
  pr "      abort (); /* Should never happen, but keeps GCC happy. */\n";
  pr "    }\n";
  pr "\n";
  pr "    if (r == -1) {\n";
  pr "      assert (nbd_get_error () != NULL);\n";
  pr "      return -1;\n";
  pr "    }\n";
  pr "  } while (!blocked);\n";
  pr "  return 0;\n";
  pr "}\n";
  pr "\n";

  pr "/* Returns whether in the given state read or write would be valid.\n";
  pr " * NB: is_locked = false, may_set_error = false.\n";
  pr " */\n";
  pr "int\n";
  pr "nbd_internal_aio_get_direction (enum state state)\n";
  pr "{\n";
  pr "  int r = 0;\n";
  pr "\n";
  pr "  switch (state)\n";
  pr "  {\n";
  List.iter (
    fun ({ parsed = { state_enum; events } }) ->
      pr "  case %s:\n" state_enum;
      List.iter (
        fun (e, _) ->
          match e with
          | NotifyRead ->  pr "    r |= LIBNBD_AIO_DIRECTION_READ;\n"
          | NotifyWrite -> pr "    r |= LIBNBD_AIO_DIRECTION_WRITE;\n"
          | CmdCreate
          | CmdConnectSockAddr
          | CmdConnectTCP
          | CmdConnectCommand | CmdConnectSA | CmdConnectSocket
          | CmdIssue -> ()
      ) events;
      pr "    break;\n";
  ) states;
  pr "  }\n";
  pr "\n";
  pr "  return r;\n";
  pr "}\n";
  pr "\n";

  pr "/* Other functions associated with the state machine. */\n";
  pr "const char *\n";
  pr "nbd_internal_state_short_string (enum state state)\n";
  pr "{\n";
  pr "  switch (state)\n";
  pr "  {\n";
  List.iter (
    fun ({ parsed = { display_name; state_enum } }) ->
      pr "  case %s:\n" state_enum;
      pr "    return \"%s\";\n" display_name
  ) states;
  pr "  }\n";
  pr "\n";
  pr "  /* This function is only used for debug messages, and\n";
  pr "   * this should never happen.\n";
  pr "   */\n";
  pr "  return \"UNKNOWN!\";\n";
  pr "}\n";
  pr "\n";

  pr "const char *\n";
  pr "nbd_unlocked_connection_state (struct nbd_handle *h)\n";
  pr "{\n";
  pr "  switch (get_next_state (h))\n";
  pr "  {\n";
  List.iter (
    fun ({ comment; parsed = { display_name; state_enum } }) ->
      pr "  case %s:\n" state_enum;
      pr "    return \"%s\" \": \"\n" display_name;
      pr "           \"%s\";\n" comment;
      pr "\n";
  ) states;
  pr "  }\n";
  pr "\n";
  pr "  return NULL;\n";
  pr "}\n";
  pr "\n";

  pr "/* Map a state to its group name. */\n";
  pr "enum state_group\n";
  pr "nbd_internal_state_group (enum state state)\n";
  pr "{\n";
  pr "  switch (state) {\n";
  List.iter (
    fun ({ parsed = { prefix; state_enum } }) ->
      pr "  case %s:\n" state_enum;
      if prefix = [] then
        pr "    return GROUP_TOP;\n"
      else
        pr "    return GROUP%s;\n"
           (String.concat "" (List.map ((^) "_") prefix))
  ) states;
  pr "  default:\n";
  pr "    abort (); /* Should never happen, but keeps GCC happy. */\n";
  pr "  }\n";
  pr "}\n";
  pr "\n";

  pr "/* Map a state group to its parent group. */\n";
  pr "enum state_group\n";
  pr "nbd_internal_state_group_parent (enum state_group group)\n";
  pr "{\n";
  pr "  switch (group) {\n";
  pr "  case GROUP_TOP:\n";
  pr "    return GROUP_TOP;\n";
  let rec loop prefix = function
    | [] -> ()
    | State _ :: rest ->
       loop prefix rest
    | Group (name, group) :: rest ->
       let enum =
         "GROUP" ^ String.concat "" (List.map ((^) "_") prefix) ^ "_" ^ name in
       pr "  case %s:\n" enum;
       if prefix = [] then
         pr "    return GROUP_TOP;\n"
       else (
         let parent = "GROUP" ^ String.concat "" (List.map ((^) "_") prefix) in
         pr "    return %s;\n" parent
       );
       loop (prefix @ [name]) group;
       loop prefix rest
  in
  loop [] state_machine;
  pr "  default:\n";
  pr "    abort (); /* Should never happen, but keeps GCC happy. */\n";
  pr "  }\n";
  pr "};\n"
