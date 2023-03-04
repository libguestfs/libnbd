(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* nbd client library in userspace: state machine definition
 * Copyright Red Hat
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

(* For explanation see generator/README.state-machine.md *)

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
  loc : Utils.location;
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
