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

open Printf

type external_event =
  | NotifyRead
  | NotifyWrite
  | CmdCreate
  | CmdConnectSockAddr
  | CmdConnectTCP
  | CmdConnectCommand
  | CmdConnectSA
  | CmdConnectSocket
  | CmdIssue

let all_external_events =
  [NotifyRead; NotifyWrite;
   CmdCreate;
   CmdConnectSockAddr; CmdConnectTCP;
   CmdConnectCommand; CmdConnectSA; CmdConnectSocket;
   CmdIssue]

let string_of_external_event = function
  | NotifyRead -> "NotifyRead"
  | NotifyWrite -> "NotifyWrite"
  | CmdCreate -> "CmdCreate"
  | CmdConnectSockAddr -> "CmdConnectSockAddr"
  | CmdConnectTCP -> "CmdConnectTCP"
  | CmdConnectCommand -> "CmdConnectCommand"
  | CmdConnectSA -> "CmdConnectSA"
  | CmdConnectSocket -> "CmdConnectSocket"
  | CmdIssue -> "CmdIssue"

type location = string * int
let noloc = ("", 0)

type state = {
  name : string;
  comment : string;
  external_events : (external_event * string) list;
  mutable parsed : parsed_state;
}
and parsed_state = {
  prefix : string list;
  display_name : string;
  state_enum : string;
  loc : location;
  code : string;
  internal_transitions : state list;
  events : (external_event * state) list;
}

let default_state = { name = ""; comment = ""; external_events = [];
                      parsed = { prefix = []; display_name = "";
                                 state_enum = ""; loc = noloc; code = "";
                                 internal_transitions = []; events = [] } }

type state_machine = state_group list
and state_group =
  | Group of string * state_machine
  | State of state

(* Top level state machine. *)
let rec state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Handle after being initially created";
    external_events = [ CmdCreate, "";
                        CmdConnectSockAddr, "CONNECT.START";
                        CmdConnectTCP, "CONNECT_TCP.START";
                        CmdConnectCommand, "CONNECT_COMMAND.START";
                        CmdConnectSA, "CONNECT_SA.START";
                        CmdConnectSocket, "MAGIC.START" ];
  };

  Group ("CONNECT", connect_state_machine);
  Group ("CONNECT_TCP", connect_tcp_state_machine);
  Group ("CONNECT_COMMAND", connect_command_state_machine);
  Group ("CONNECT_SA", connect_sa_state_machine);

  Group ("MAGIC", magic_state_machine);
  Group ("OLDSTYLE", oldstyle_state_machine);
  Group ("NEWSTYLE", newstyle_state_machine);

  State {
    default_state with
    name = "READY";
    comment = "Connection is ready to process NBD commands";
    external_events = [ CmdIssue, "ISSUE_COMMAND.START";
                        NotifyRead, "REPLY.START" ];
  };

  Group ("ISSUE_COMMAND", issue_command_state_machine);
  Group ("REPLY", reply_state_machine);

  State {
    default_state with
    name = "DEAD";
    comment = "Connection is in an unrecoverable error state, can only be closed";
  };

  State {
    default_state with
    name = "CLOSED";
    comment = "Connection is closed";
  };
]

(* State machine implementing [nbd_aio_connect]. *)
and connect_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Initial call to connect(2) on the socket";
    external_events = [ NotifyWrite, "CONNECTING" ];
  };

  State {
    default_state with
    name = "CONNECTING";
    comment = "Connecting to the remote server";
    external_events = [ NotifyWrite, "" ];
  };
]

(* State machine implementing [nbd_aio_connect_tcp]. *)
and connect_tcp_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Connect to a remote TCP server";
    external_events = [];
  };

  State {
    default_state with
    name = "CONNECT";
    comment = "Initial call to connect(2) on a TCP socket";
    external_events = [ NotifyWrite, "CONNECTING" ];
  };

  State {
    default_state with
    name = "CONNECTING";
    comment = "Connecting to the remote server over a TCP socket";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "NEXT_ADDRESS";
    comment = "Connecting to the next address over a TCP socket";
    external_events = [];
  };
]

(* State machine implementing [nbd_aio_connect_command]. *)
and connect_command_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Connect to a subprocess";
    external_events = [];
  };
]

(* State machine implementing [nbd_aio_connect_systemd_socket_activation]. *)
and connect_sa_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Connect to a subprocess with systemd socket activation";
    external_events = [];
  };
]

(* Parse initial magic string from the server. *)
and magic_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Prepare to receive the magic identification from remote";
    external_events = [];
  };

  State {
    default_state with
    name = "RECV_MAGIC";
    comment = "Receive initial magic identification from remote";
    external_events = [ NotifyRead, "" ];
  };

  State {
    default_state with
    name = "CHECK_MAGIC";
    comment = "Check magic and version sent by remote";
  };
]

(* Oldstyle handshake. *)
and oldstyle_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Prepare to receive remainder of oldstyle header";
    external_events = [];
  };

  State {
    default_state with
    name = "RECV_REMAINING";
    comment = "Receive remainder of oldstyle header";
    external_events = [ NotifyRead, "" ];
  };

  State {
    default_state with
    name = "CHECK";
    comment = "Check oldstyle header";
    external_events = [];
  };
]

(* Fixed newstyle handshake. *)
and newstyle_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Prepare to receive newstyle gflags from remote";
    external_events = [];
  };

  State {
    default_state with
    name = "RECV_GFLAGS";
    comment = "Receive newstyle gflags from remote";
    external_events = [ NotifyRead, "" ];
  };

  State {
    default_state with
    name = "CHECK_GFLAGS";
    comment = "Check global flags sent by remote";
  };

  State {
    default_state with
    name = "SEND_CFLAGS";
    comment = "Send newstyle client flags to remote";
    external_events = [ NotifyWrite, "" ];
  };

  (* Options.  These state groups are always entered unconditionally,
   * in this order.  The START state in each group will check if the
   * state needs to run and skip to the next state in the list if not.
   *)
  Group ("OPT_STARTTLS", newstyle_opt_starttls_state_machine);
  Group ("OPT_STRUCTURED_REPLY", newstyle_opt_structured_reply_state_machine);
  Group ("OPT_SET_META_CONTEXT", newstyle_opt_set_meta_context_state_machine);
  Group ("OPT_GO", newstyle_opt_go_state_machine);
  Group ("OPT_EXPORT_NAME", newstyle_opt_export_name_state_machine);

  (* When option parsing has successfully finished negotiation
   * it will jump to this state for final steps before moving to
   * the %READY state.
   *)
  State {
    default_state with
    name = "FINISHED";
    comment = "Finish off newstyle negotiation";
  };
]

(* Fixed newstyle NBD_OPT_STARTTLS option. *)
and newstyle_opt_starttls_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Try to send newstyle NBD_OPT_STARTTLS to upgrade to TLS";
    external_events = [];
  };

  State {
    default_state with
    name = "SEND";
    comment = "Send newstyle NBD_OPT_STARTTLS to upgrade to TLS";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "RECV_REPLY";
    comment = "Receive newstyle NBD_OPT_STARTTLS reply";
    external_events = [ NotifyRead, "" ];
  };

  State {
    default_state with
    name = "RECV_REPLY_PAYLOAD";
    comment = "Receive any newstyle NBD_OPT_STARTTLS reply payload";
    external_events = [ NotifyRead, "" ];
  };

  State {
    default_state with
    name = "CHECK_REPLY";
    comment = "Check newstyle NBD_OPT_STARTTLS reply";
    external_events = [];
  };

  State {
    default_state with
    name = "TLS_HANDSHAKE_READ";
    comment = "TLS handshake (reading)";
    external_events = [ NotifyRead, "" ];
  };

  State {
    default_state with
    name = "TLS_HANDSHAKE_WRITE";
    comment = "TLS handshake (writing)";
    external_events = [ NotifyWrite, "" ];
  };
]

(* Fixed newstyle NBD_OPT_STRUCTURED_REPLY option. *)
and newstyle_opt_structured_reply_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Try to negotiate newstyle NBD_OPT_STRUCTURED_REPLY";
    external_events = [];
  };

  State {
    default_state with
    name = "SEND";
    comment = "Send newstyle NBD_OPT_STRUCTURED_REPLY negotiation request";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "RECV_REPLY";
    comment = "Receive newstyle NBD_OPT_STRUCTURED_REPLY option reply";
    external_events = [ NotifyRead, "" ];
  };

  State {
    default_state with
    name = "RECV_REPLY_PAYLOAD";
    comment = "Receive any newstyle NBD_OPT_STRUCTURED_REPLY reply payload";
    external_events = [ NotifyRead, "" ];
  };

  State {
    default_state with
    name = "CHECK_REPLY";
    comment = "Check newstyle NBD_OPT_STRUCTURED_REPLY option reply";
    external_events = [];
  };
]

(* Fixed newstyle NBD_OPT_SET_META_CONTEXT option. *)
and newstyle_opt_set_meta_context_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Try to negotiate newstyle NBD_OPT_SET_META_CONTEXT";
    external_events = [];
  };

  State {
    default_state with
    name = "SEND";
    comment = "Send newstyle NBD_OPT_SET_META_CONTEXT";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "SEND_EXPORTNAMELEN";
    comment = "Send newstyle NBD_OPT_SET_META_CONTEXT export name length";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "SEND_EXPORTNAME";
    comment = "Send newstyle NBD_OPT_SET_META_CONTEXT export name";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "SEND_NRQUERIES";
    comment = "Send newstyle NBD_OPT_SET_META_CONTEXT number of queries";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "PREPARE_NEXT_QUERY";
    comment = "Prepare to send newstyle NBD_OPT_SET_META_CONTEXT query";
    external_events = [];
  };

  State {
    default_state with
    name = "SEND_QUERYLEN";
    comment = "Send newstyle NBD_OPT_SET_META_CONTEXT query length";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "SEND_QUERY";
    comment = "Send newstyle NBD_OPT_SET_META_CONTEXT query";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "PREPARE_FOR_REPLY";
    comment = "Prepare to receive newstyle NBD_OPT_SET_META_CONTEXT option reply";
    external_events = [];
  };

  State {
    default_state with
    name = "RECV_REPLY";
    comment = "Receive newstyle NBD_OPT_SET_META_CONTEXT option reply";
    external_events = [ NotifyRead, "" ];
  };

  State {
    default_state with
    name = "RECV_REPLY_PAYLOAD";
    comment = "Receive newstyle NBD_OPT_SET_META_CONTEXT option reply payload";
    external_events = [ NotifyRead, "" ];
  };

  State {
    default_state with
    name = "CHECK_REPLY";
    comment = "Check newstyle NBD_OPT_SET_META_CONTEXT option reply";
    external_events = [];
  };
]

(* Fixed newstyle NBD_OPT_GO option. *)
and newstyle_opt_go_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Try to send newstyle NBD_OPT_GO to end handshake";
    external_events = [];
  };

  State {
    default_state with
    name = "SEND";
    comment = "Send newstyle NBD_OPT_GO to end handshake";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "SEND_EXPORTNAMELEN";
    comment = "Send newstyle NBD_OPT_GO export name length";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "SEND_EXPORT";
    comment = "Send newstyle NBD_OPT_GO export name";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "SEND_NRINFOS";
    comment = "Send newstyle NBD_OPT_GO number of infos";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "RECV_REPLY";
    comment = "Receive newstyle NBD_OPT_GO reply";
    external_events = [ NotifyRead, "" ];
  };

  State {
    default_state with
    name = "RECV_REPLY_PAYLOAD";
    comment = "Receive newstyle NBD_OPT_GO reply payload";
    external_events = [ NotifyRead, "" ];
  };

  State {
    default_state with
    name = "CHECK_REPLY";
    comment = "Check newstyle NBD_OPT_GO reply";
    external_events = [];
  };
]

(* Newstyle NBD_OPT_EXPORT_NAME option. *)
and newstyle_opt_export_name_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Try to send newstyle NBD_OPT_EXPORT_NAME to end handshake";
    external_events = [];
  };

  State {
    default_state with
    name = "SEND";
    comment = "Send newstyle NBD_OPT_EXPORT_NAME to end handshake";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "SEND_EXPORT";
    comment = "Send newstyle NBD_OPT_EXPORT_NAME export name";
    external_events = [ NotifyWrite, "" ];
  };

  State {
    default_state with
    name = "RECV_REPLY";
    comment = "Receive newstyle NBD_OPT_EXPORT_NAME reply";
    external_events = [ NotifyRead, "" ];
  };

  State {
    default_state with
    name = "CHECK_REPLY";
    comment = "Check newstyle NBD_OPT_EXPORT_NAME reply";
    external_events = [];
  };
]

(* Sending a command to the server. *)
and issue_command_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Begin issuing a command to the remote server";
    external_events = [];
  };

  State {
    default_state with
    name = "SEND_REQUEST";
    comment = "Sending a request to the remote server";
    external_events = [ NotifyWrite, "";
                        NotifyRead, "PAUSE_SEND_REQUEST" ];
  };

  State {
    default_state with
    name = "PAUSE_SEND_REQUEST";
    comment = "Interrupt send request to receive an earlier command's reply";
    external_events = [];
  };

  State {
    default_state with
    name = "PREPARE_WRITE_PAYLOAD";
    comment = "Prepare the write payload to send to the remote server";
    external_events = [];
  };

  State {
    default_state with
    name = "SEND_WRITE_PAYLOAD";
    comment = "Sending the write payload to the remote server";
    external_events = [ NotifyWrite, "";
                        NotifyRead, "PAUSE_WRITE_PAYLOAD" ];
  };

  State {
    default_state with
    name = "PAUSE_WRITE_PAYLOAD";
    comment = "Interrupt write payload to receive an earlier command's reply";
    external_events = [];
  };

  State {
    default_state with
    name = "SEND_WRITE_SHUTDOWN";
    comment = "Sending write shutdown notification to the remote server";
    external_events = [ NotifyWrite, "";
                        NotifyRead, "PAUSE_WRITE_SHUTDOWN" ];
  };

  State {
    default_state with
    name = "PAUSE_WRITE_SHUTDOWN";
    comment = "Interrupt write shutdown to receive an earlier command's reply";
    external_events = [];
  };

  State {
    default_state with
    name = "FINISH";
    comment = "Finish issuing a command";
    external_events = [];
  };
]

(* Receiving a reply from the server. *)
and reply_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Prepare to receive a reply from the remote server";
    external_events = [];
  };

  State {
    default_state with
    name = "RECV_REPLY";
    comment = "Receive a reply from the remote server";
    external_events = [];
  };

  State {
    default_state with
    name = "CHECK_SIMPLE_OR_STRUCTURED_REPLY";
    comment = "Check if the reply is a simple or structured reply";
    external_events = [];
  };

  Group ("SIMPLE_REPLY", simple_reply_state_machine);
  Group ("STRUCTURED_REPLY", structured_reply_state_machine);

  State {
    default_state with
    name = "FINISH_COMMAND";
    comment = "Finish receiving a command";
    external_events = [];
  };
]

(* Receiving a simple reply from the server. *)
and simple_reply_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Parse a simple reply from the server";
    external_events = [];
  };

  State {
    default_state with
    name = "RECV_READ_PAYLOAD";
    comment = "Receiving the read payload for a simple reply";
    external_events = [];
  };
]

(* Receiving a structured reply from the server. *)
and structured_reply_state_machine = [
  State {
    default_state with
    name = "START";
    comment = "Prepare to receive the remaining part of a structured reply";
    external_events = [];
  };

  State {
    default_state with
    name = "RECV_REMAINING";
    comment = "Receiving the remaining part of a structured reply";
    external_events = [];
  };

  State {
    default_state with
    name = "CHECK";
    comment = "Parse a structured reply from the server";
    external_events = [];
  };

  State {
    default_state with
    name = "RECV_ERROR";
    comment = "Receive a structured reply error header";
    external_events = []
  };

  State {
    default_state with
    name = "RECV_ERROR_MESSAGE";
    comment = "Receive a structured reply error message";
    external_events = [];
  };

  State {
    default_state with
    name = "RECV_ERROR_TAIL";
    comment = "Receive a structured reply error tail";
    external_events = [];
  };

  State {
    default_state with
    name = "RECV_OFFSET_DATA";
    comment = "Receive a structured reply offset-data header";
    external_events = [];
  };

  State {
    default_state with
    name = "RECV_OFFSET_DATA_DATA";
    comment = "Receive a structured reply offset-data block of data";
    external_events = [];
  };

  State {
    default_state with
    name = "RECV_OFFSET_HOLE";
    comment = "Receive a structured reply offset-hole header";
    external_events = [];
  };

  State {
    default_state with
    name = "RECV_BS_ENTRIES";
    comment = "Receive a structured reply block-status payload";
    external_events = [];
  };

  State {
    default_state with
    name = "FINISH";
    comment = "Finish receiving a structured reply";
    external_events = [];
  };
]

let string_of_location (file, lineno) = sprintf "%s:%d" file lineno
let line_directive_of_location (file, lineno) =
  sprintf "#line %d \"%s\"" lineno file
