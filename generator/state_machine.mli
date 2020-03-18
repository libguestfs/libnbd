(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* nbd client library in userspace: state machine definition
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

(** The state machine.
 *
 * Each state has some associated C code which is called when
 * the state is entered, or when the state is re-entered because
 * of an external event.  That code is not in this file, it's
 * in [generator/states*.c].
 *
 * Each handle starts in the top level START state.
 *
 * When you enter a state, the associated C code for that state
 * runs.  If the C code calls SET_NEXT_STATE and returns 0 then
 * the connection enters the next state without blocking.  If the
 * C code calls SET_NEXT_STATE_AND_BLOCK and returns 0 then the
 * connection blocks, but will resume with the code for the next
 * state on the next external event.  If the C code does _not_
 * call either macro but returns 0, the state machine is blocked
 * and will not be re-entered until an external event happens
 * (see below), where the same C code will be executed again on
 * re-entry.  If the C code calls returns -1 after using
 * set_error(), then the state machine blocks and the caller
 * should report failure; the next external event will resume the
 * state machine according to whether SET_NEXT_STATE was used.
 *
 * There are various end states such as CLOSED and DEAD.  These
 * are not special in relation to the above state transition rules,
 * it's just that they have no way to move to another state.  However,
 * the DEAD state expects that set_error() was used in the previous
 * state, and will return -1 itself after performing cleanup actions;
 * the earlier state that wants to transition to DEAD should return 0
 * rather than -1, so as not to bypass this cleanup.
 *
 * An external event is something like the file descriptor being
 * ready to read or write, or the main program calling a function
 * such as [nbd_aio_connect].  Possible external events, and the
 * next state resulting, are listed in the states table below.
 *
 * An empty string [""] for an external event’s next state means
 * the same state is re-entered.  The same C code for the state
 * will be run again.
 *
 * States can be grouped hierarchically.  States can be referred
 * to by an absolute path from the top level, such as ".DEAD",
 * or by a relative path from the current level, such as "CONNECT"
 * (another state at the same level), "REPLY.START" (a state in
 * a sub-group), or "^FINISH_COMMAND" (a state in the level above
 * the current one).  When entering a group you must enter at the
 * START state.  When leaving a group and going to a higher level
 * in the state tree there is no restriction on the next state.
 *)

type external_event =
  | NotifyRead                (** fd becomes ready to read *)
  | NotifyWrite               (** fd becomes ready to write *)
  | CmdCreate                 (** [nbd_create] function called *)
  | CmdConnectSockAddr        (** [nbd_aio_connect] function called *)
  | CmdConnectTCP             (** [nbd_aio_connect_tcp] *)
  | CmdConnectCommand         (** [nbd_aio_connect_command] *)
  | CmdConnectSA              (** [nbd_aio_connect_systemd_socket_activation]*)
  | CmdConnectSocket          (** [nbd_aio_connect_socket] *)
  | CmdIssue                  (** issuing an NBD command *)
val all_external_events : external_event list
val string_of_external_event : external_event -> string

type location = string * int  (** source location: file, line number *)
val noloc : location
val string_of_location : location -> string
val line_directive_of_location : location -> string

type state = {
  (** The state name (without prefix).  If this has the special name
      "START" then it is the start state of the current group.  Each
      group can only have one start state. *)
  name : string;

  comment : string;             (** comment about the state *)

  (** Possible transitions from this state to a next state.  The
      external events are coded into the state table below.  The
      internal transitions are parsed out of the C code. *)
  external_events : (external_event * string) list;

  (** After flattening the state machine, the generator fills
      in the extra fields in [state.parsed]. *)
  mutable parsed : parsed_state;
}

and parsed_state = {
  (** The hierarchy group prefix.  For states in the top level
      state machine this is an empty list.  For states in the
      next level down this is a single element, and so on. *)
  prefix : string list;

  (** Hierarchical state name, like "NEWSTYLE.OPT_STARTTLS.CHECK_REPLY"
      for use in debug messages etc. *)
  display_name : string;

  (** The STATE_* enum used in the generated C code. *)
  state_enum : string;

  (** The C code implementing this state. *)
  loc : location;
  code : string;

  (** Internal transitions, parsed out of the C code. *)
  internal_transitions : state list;

  (** External events after resolving them to the destination states. *)
  events : (external_event * state) list;
}

(** The type of the hierarchical state machine. *)
type state_machine = state_group list
and state_group =
  | Group of string * state_machine (** string is name/prefix of the group *)
  | State of state

val state_machine : state_machine
