(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* nbd client library in userspace: the API
 * Copyright (C) 2013-2022 Red Hat Inc.
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

open Utils

type call = {
  args : arg list;
  optargs : optarg list;
  ret : ret;
  shortdesc : string;
  longdesc : string;
  example : string option;
  see_also : link list;
  permitted_states : permitted_state list;
  is_locked : bool;
  may_set_error : bool;
  mutable first_version : int * int;
}
and arg =
| Bool of string
| BytesIn of string * string
| BytesOut of string * string
| BytesPersistIn of string * string
| BytesPersistOut of string * string
| Closure of closure
| Enum of string * enum
| Fd of string
| Flags of string * flags
| Int of string
| Int64 of string
| Path of string
| SizeT of string
| SockAddrAndLen of string * string
| String of string
| StringList of string
| UInt of string
| UInt32 of string
| UInt64 of string
| UIntPtr of string
and optarg =
| OClosure of closure
| OFlags of string * flags * string list option
and ret =
| RBool
| RStaticString
| RErr
| RFd
| RInt
| RInt64
| RCookie
| RSizeT
| RString
| RUInt
| RUIntPtr
| RUInt64
| REnum of enum
| RFlags of flags
and closure = {
  cbname : string;
  cbargs : cbarg list;
}
and cbarg =
| CBArrayAndLen of arg * string
| CBBytesIn of string * string
| CBInt of string
| CBInt64 of string
| CBMutable of arg
| CBString of string
| CBUInt of string
| CBUInt64 of string
and enum = {
  enum_prefix : string;
  enums : (string * int) list
}
and flags = {
  flag_prefix : string;
  guard : string option;
  flags : (string * int) list;
  _unused : unit
}
and permitted_state =
| Created
| Connecting
| Negotiating
| Connected
| Closed | Dead
and link =
| Link of string
| SectionLink of string
| MainPageLink
| ExternalLink of string * int
| URLLink of string

let blocking_connect_call_description = "\n
This call returns when the connection has been made.  By default,
this proceeds all the way to transmission phase, but
L<nbd_set_opt_mode(3)> can be used for manual control over
option negotiation performed before transmission phase."

let async_connect_call_description = "\n
You can check if the connection attempt is still underway by
calling L<nbd_aio_is_connecting(3)>.  If L<nbd_set_opt_mode(3)>
is enabled, the connection is ready for manual option negotiation
once L<nbd_aio_is_negotiating(3)> returns true; otherwise, the
connection attempt will include the NBD handshake, and is ready
for use once L<nbd_aio_is_ready(3)> returns true."

let strict_call_description = "\n
By default, libnbd will reject attempts to use this function with
parameters that are likely to result in server failure, such as
requesting an unknown command flag.  The L<nbd_set_strict_mode(3)>
function can be used to alter which scenarios should await a server
reply rather than failing fast."

let non_blocking_test_call_description = "\n
This call does not block, because it returns data that is saved in
the handle from the NBD protocol handshake."

(* Closures. *)
let chunk_closure = {
  cbname = "chunk";
  cbargs = [ CBBytesIn ("subbuf", "count");
             CBUInt64 "offset"; CBUInt "status";
             CBMutable (Int "error") ]
}
let completion_closure = {
  cbname = "completion";
  cbargs = [ CBMutable (Int "error") ]
}
let debug_closure = {
  cbname = "debug";
  cbargs = [ CBString "context"; CBString "msg" ]
}
let extent_closure = {
  cbname = "extent";
  cbargs = [ CBString "metacontext";
             CBUInt64 "offset";
             CBArrayAndLen (UInt32 "entries",
                            "nr_entries");
             CBMutable (Int "error") ]
}
let list_closure = {
  cbname = "list";
  cbargs = [ CBString "name"; CBString "description" ]
}
let context_closure = {
  cbname = "context";
  cbargs = [ CBString "name" ]
}
let all_closures = [ chunk_closure; completion_closure;
                     debug_closure; extent_closure; list_closure;
                     context_closure ]

(* Enums. *)
let tls_enum = {
  enum_prefix = "TLS";
  enums = [
    "DISABLE", 0;
    "ALLOW",   1;
    "REQUIRE", 2;
  ]
}
let block_size_enum = {
  enum_prefix = "SIZE";
  enums = [
    "MINIMUM",   0;
    "PREFERRED", 1;
    "MAXIMUM",   2;
  ]
}
let all_enums = [ tls_enum; block_size_enum ]

(* Flags. See also Constants below. *)
let default_flags = { flag_prefix = ""; guard = None; flags = [];
                      _unused = () }

let cmd_flags = {
  default_flags with
  flag_prefix = "CMD_FLAG";
  guard = Some "((h->strict & LIBNBD_STRICT_FLAGS) || flags > UINT16_MAX)";
  flags = [
    "FUA",       1 lsl 0;
    "NO_HOLE",   1 lsl 1;
    "DF",        1 lsl 2;
    "REQ_ONE",   1 lsl 3;
    "FAST_ZERO", 1 lsl 4;
  ]
}
let handshake_flags = {
  default_flags with
  flag_prefix = "HANDSHAKE_FLAG";
  flags = [
    "FIXED_NEWSTYLE", 1 lsl 0;
    "NO_ZEROES",      1 lsl 1;
  ]
}
let strict_flags = {
  default_flags with
  flag_prefix = "STRICT";
  flags = [
    "COMMANDS",       1 lsl 0;
    "FLAGS",          1 lsl 1;
    "BOUNDS",         1 lsl 2;
    "ZERO_SIZE",      1 lsl 3;
    "ALIGN",          1 lsl 4;
  ]
}
let allow_transport_flags = {
  default_flags with
  flag_prefix = "ALLOW_TRANSPORT";
  flags = [
    "TCP",   1 lsl 0;
    "UNIX",  1 lsl 1;
    "VSOCK", 1 lsl 2;
  ]
}
let shutdown_flags = {
  default_flags with
  flag_prefix = "SHUTDOWN";
  flags = [
    "ABANDON_PENDING", 1 lsl 16;
  ]
}
let all_flags = [ cmd_flags; handshake_flags; strict_flags;
                  allow_transport_flags; shutdown_flags ]

let default_call = { args = []; optargs = []; ret = RErr;
                     shortdesc = ""; longdesc = ""; example = None;
                     see_also = [];
                     permitted_states = [];
                     is_locked = true; may_set_error = true;
                     first_version = (0, 0) }

(* Calls.
 *
 * The first parameter [struct nbd_handle *nbd] is implicit.
 *)
let handle_calls = [
  "set_debug", {
    default_call with
    args = [ Bool "debug" ]; ret = RErr;
    shortdesc = "set or clear the debug flag";
    longdesc = "\
Set or clear the debug flag.  When debugging is enabled,
debugging messages from the library are printed to stderr,
unless a debugging callback has been defined too
(see L<nbd_set_debug_callback(3)>) in which case they are
sent to that function.  This flag defaults to false on
newly created handles, except if C<LIBNBD_DEBUG=1> is
set in the environment in which case it defaults to true.";
    see_also = [Link "set_handle_name"; Link "set_debug_callback"];
  };

  "get_debug", {
    default_call with
    args = []; ret = RBool;
    may_set_error = false;
    shortdesc = "return the state of the debug flag";
    longdesc = "\
Return the state of the debug flag on this handle.";
    see_also = [Link "set_debug"];
  };

  "set_debug_callback", {
    default_call with
    args = [ Closure debug_closure ];
    ret = RErr;
    shortdesc = "set the debug callback";
    longdesc = "\
Set the debug callback.  This function is called when the library
emits debug messages, when debugging is enabled on a handle.  The
callback parameters are C<user_data> passed to this function, the
name of the libnbd function emitting the debug message (C<context>),
and the message itself (C<msg>).  If no debug callback is set on
a handle then messages are printed on C<stderr>.

The callback should not call C<nbd_*> APIs on the same handle since it can
be called while holding the handle lock and will cause a deadlock.";
};

  "clear_debug_callback", {
    default_call with
    args = [];
    ret = RErr;
    shortdesc = "clear the debug callback";
    longdesc = "\
Remove the debug callback if one was previously associated
with the handle (with L<nbd_set_debug_callback(3)>).  If no
callback was associated this does nothing.";
};

  "stats_bytes_sent", {
    default_call with
    args = []; ret = RUInt64;
    may_set_error = false;
    shortdesc = "statistics of bytes sent over connection so far";
    longdesc = "\
Return the number of bytes that the client has sent to the server.

This tracks the plaintext bytes utilized by the NBD protocol; it
may differ from the number of bytes actually sent over the
connection, particularly when TLS is in use.";
    see_also = [Link "stats_chunks_sent";
                Link "stats_bytes_received"; Link "stats_chunks_received"];
  };

  "stats_chunks_sent", {
    default_call with
    args = []; ret = RUInt64;
    may_set_error = false;
    shortdesc = "statistics of chunks sent over connection so far";
    longdesc = "\
Return the number of chunks that the client has sent to the
server, where a chunk is a group of bytes delineated by a magic
number that cannot be further subdivided without breaking the
protocol.

This number does not necessarily relate to the number of API
calls made, nor to the number of TCP packets sent over the
connection.";
    see_also = [Link "stats_bytes_sent";
                Link "stats_bytes_received"; Link "stats_chunks_received";
                Link "set_strict_mode"];
  };

  "stats_bytes_received", {
    default_call with
    args = []; ret = RUInt64;
    may_set_error = false;
    shortdesc = "statistics of bytes received over connection so far";
    longdesc = "\
Return the number of bytes that the client has received from the server.

This tracks the plaintext bytes utilized by the NBD protocol; it
may differ from the number of bytes actually received over the
connection, particularly when TLS is in use.";
    see_also = [Link "stats_chunks_received";
                Link "stats_bytes_sent"; Link "stats_chunks_sent"];
  };

  "stats_chunks_received", {
    default_call with
    args = []; ret = RUInt64;
    may_set_error = false;
    shortdesc = "statistics of chunks received over connection so far";
    longdesc = "\
Return the number of chunks that the client has received from the
server, where a chunk is a group of bytes delineated by a magic
number that cannot be further subdivided without breaking the
protocol.

This number does not necessarily relate to the number of API
calls made, nor to the number of TCP packets received over the
connection.";
    see_also = [Link "stats_bytes_received";
                Link "stats_bytes_sent"; Link "stats_chunks_sent";
                Link "get_structured_replies_negotiated"];
  };

  "set_handle_name", {
    default_call with
    args = [ String "handle_name" ]; ret = RErr;
    shortdesc = "set the handle name";
    longdesc = "\
Handles have a name which is unique within the current process.
The handle name is used in debug output.

Handle names are normally generated automatically and have the
form C<\"nbd1\">, C<\"nbd2\">, etc., but you can optionally use
this call to give the handles a name which is meaningful for
your application to make debugging output easier to understand.";
    see_also = [Link "get_handle_name"];
  };

  "get_handle_name", {
    default_call with
    args = []; ret = RString;
    shortdesc = "get the handle name";
    longdesc = "\
Get the name of the handle.  If it was previously set by calling
L<nbd_set_handle_name(3)> then this returns the name that was set.
Otherwise it will return a generic name like C<\"nbd1\">,
C<\"nbd2\">, etc.";
    see_also = [Link "set_handle_name"];
  };

  "set_private_data", {
    default_call with
    args = [ UIntPtr "private_data" ]; ret = RUIntPtr;
    is_locked = false; may_set_error = false;
    shortdesc = "set the per-handle private data";
    longdesc = "\
Handles contain a private data field for applications to use
for any purpose.

When calling libnbd from C, the type of this field is C<uintptr_t> so
it can be used to store an unsigned integer or a pointer.

In non-C bindings it can be used to store an unsigned integer.

This function sets the value of this field and returns the old value
(or 0 if it was not previously set).";
    see_also = [Link "get_private_data"];
  };

  "get_private_data", {
    default_call with
    args = []; ret = RUIntPtr;
    is_locked = false; may_set_error = false;
    shortdesc = "get the per-handle private data";
    longdesc = "\
Return the value of the private data field set previously
by a call to L<nbd_set_private_data(3)>
(or 0 if it was not previously set).";
    see_also = [Link "set_private_data"];
  };

  "set_export_name", {
    default_call with
    args = [ String "export_name" ]; ret = RErr;
    permitted_states = [ Created; Negotiating ];
    shortdesc = "set the export name";
    longdesc = "\
For servers which require an export name or can serve different
content on different exports, set the C<export_name> to
connect to.  The default is the empty string C<\"\">.

This is only relevant when connecting to servers using the
newstyle protocol as the oldstyle protocol did not support
export names.  The NBD protocol limits export names to
4096 bytes, but servers may not support the full length.
The encoding of export names is always UTF-8.

When option mode is not in use, the export name must be set
before beginning a connection.  However, when L<nbd_set_opt_mode(3)>
has enabled option mode, it is possible to change the export
name prior to L<nbd_opt_go(3)>.  In particular, the use of
L<nbd_opt_list(3)> during negotiation can be used to determine
a name the server is likely to accept, and L<nbd_opt_info(3)> can
be used to learn details about an export before connecting.

This call may be skipped if using L<nbd_connect_uri(3)> to connect
to a URI that includes an export name.";
    see_also = [Link "get_export_name"; Link "connect_uri";
                Link "set_opt_mode"; Link "opt_go"; Link "opt_list";
                Link "opt_info"];
  };

  "get_export_name", {
    default_call with
    args = []; ret = RString;
    shortdesc = "get the export name";
    longdesc = "\
Get the export name associated with the handle.  This is the name
that libnbd requests; see L<nbd_get_canonical_export_name(3)> for
determining if the server has a different canonical name for the
given export (most common when requesting the default export name
of an empty string C<\"\">)";
    see_also = [Link "set_export_name"; Link "connect_uri";
                Link "get_canonical_export_name"];
  };

  "set_request_block_size", {
    default_call with
    args = [Bool "request"]; ret = RErr;
    permitted_states = [ Created; Negotiating ];
    shortdesc = "control whether NBD_OPT_GO requests block size";
    longdesc = "\
By default, when connecting to an export, libnbd requests that the
server report any block size restrictions.  The NBD protocol states
that a server may supply block sizes regardless of whether the client
requests them, and libnbd will report those block sizes (see
L<nbd_get_block_size(3)>); conversely, if a client does not request
block sizes, the server may reject the connection instead of dealing
with a client sending unaligned requests.  This function makes it
possible to test server behavior by emulating older clients.

Note that even when block size is requested, the server is not
obligated to provide any.  Furthermore, if block sizes are provided
(whether or not the client requested them), libnbd enforces alignment
to those sizes unless L<nbd_set_strict_mode(3)> is used to bypass
client-side safety checks.";
    see_also = [Link "get_request_block_size"; Link "set_full_info";
                Link "get_block_size"; Link "set_strict_mode"];
  };

  "get_request_block_size", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [];
    shortdesc = "see if NBD_OPT_GO requests block size";
    longdesc = "\
Return the state of the block size request flag on this handle.";
    see_also = [Link "set_request_block_size"];
  };

  "set_full_info", {
    default_call with
    args = [Bool "request"]; ret = RErr;
    permitted_states = [ Created; Negotiating ];
    shortdesc = "control whether NBD_OPT_GO requests extra details";
    longdesc = "\
By default, when connecting to an export, libnbd only requests the
details it needs to service data operations.  The NBD protocol says
that a server can supply optional information, such as a canonical
name of the export (see L<nbd_get_canonical_export_name(3)>) or
a description of the export (see L<nbd_get_export_description(3)>),
but that a hint from the client makes it more likely for this
extra information to be provided.  This function controls whether
libnbd will provide that hint.

Note that even when full info is requested, the server is not
obligated to reply with all information that libnbd requested.
Similarly, libnbd will ignore any optional server information that
libnbd has not yet been taught to recognize.  Furthermore, the
hint to request block sizes is independently controlled via
L<nbd_set_request_block_size(3)>.";
    see_also = [Link "get_full_info"; Link "get_canonical_export_name";
                Link "get_export_description"; Link "set_request_block_size"];
  };

  "get_full_info", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [];
    shortdesc = "see if NBD_OPT_GO requests extra details";
    longdesc = "\
Return the state of the full info request flag on this handle.";
    see_also = [Link "set_full_info"];
  };

  "get_canonical_export_name", {
    default_call with
    args = []; ret = RString;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "return the canonical export name, if the server has one";
    longdesc = "\
The NBD protocol permits a server to report an optional canonical
export name, which may differ from the client's request (as set by
L<nbd_set_export_name(3)> or L<nbd_connect_uri(3)>).  This function
accesses any name returned by the server; it may be the same as
the client request, but is more likely to differ when the client
requested a connection to the default export name (an empty string
C<\"\">).

Some servers are unlikely to report a canonical name unless the
client specifically hinted about wanting it, via L<nbd_set_full_info(3)>.";
    example = Some "examples/server-flags.c";
    see_also = [Link "set_full_info"; Link "get_export_name";
                Link "opt_info"];
  };

  "get_export_description", {
    default_call with
    args = []; ret = RString;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "return the export description, if the server has one";
    longdesc = "\
The NBD protocol permits a server to report an optional export
description.  This function reports any description returned by
the server.

Some servers are unlikely to report a description unless the
client specifically hinted about wanting it, via L<nbd_set_full_info(3)>.
For L<qemu-nbd(8)>, a description is set with I<-D>.";
    example = Some "examples/server-flags.c";
    see_also = [Link "set_full_info"; Link "opt_info"];
  };

  "set_tls", {
    default_call with
    args = [Enum ("tls", tls_enum)]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "enable or require TLS (authentication and encryption)";
    longdesc = "\
Enable or require TLS (authenticated and encrypted connections) to the
NBD server.  The possible settings are:

=over 4

=item C<LIBNBD_TLS_DISABLE>

Disable TLS.  (The default setting, unless using L<nbd_connect_uri(3)> with
a URI that requires TLS)

=item C<LIBNBD_TLS_ALLOW>

Enable TLS if possible.

This option is insecure (or best effort) in that in some cases
it will fall back to an unencrypted and/or unauthenticated
connection if TLS could not be established.  Use
C<LIBNBD_TLS_REQUIRE> below if the connection must be
encrypted.

Some servers will drop the connection if TLS fails
so fallback may not be possible.

=item C<LIBNBD_TLS_REQUIRE>

Require an encrypted and authenticated TLS connection.
Always fail to connect if the connection is not encrypted
and authenticated.

=back

As well as calling this you may also need to supply
the path to the certificates directory (L<nbd_set_tls_certificates(3)>),
the username (L<nbd_set_tls_username(3)>) and/or
the Pre-Shared Keys (PSK) file (L<nbd_set_tls_psk_file(3)>).  For now,
when using L<nbd_connect_uri(3)>, any URI query parameters related to
TLS are not handled automatically.  Setting the level higher than
zero will fail if libnbd was not compiled against gnutls; you can
test whether this is the case with L<nbd_supports_tls(3)>.";
    example = Some "examples/encryption.c";
    see_also = [SectionLink "ENCRYPTION AND AUTHENTICATION";
                Link "get_tls"; Link "get_tls_negotiated"];
  };

  "get_tls", {
    default_call with
    args = []; ret = REnum tls_enum;
    may_set_error = false;
    shortdesc = "get the TLS request setting";
    longdesc = "\
Get the TLS request setting.

B<Note:> If you want to find out if TLS was actually negotiated
on a particular connection use L<nbd_get_tls_negotiated(3)> instead.";
    see_also = [Link "set_tls"; Link "get_tls_negotiated"];
  };

  "get_tls_negotiated", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "find out if TLS was negotiated on a connection";
    longdesc = "\
After connecting you may call this to find out if the
connection is using TLS.

This is only really useful if you set the TLS request mode
to C<LIBNBD_TLS_ALLOW> (see L<nbd_set_tls(3)>), because in this
mode we try to use TLS but fall back to unencrypted if it was
not available.  This function will tell you if TLS was
negotiated or not.

In C<LIBNBD_TLS_REQUIRE> mode (the most secure) the connection
would have failed if TLS could not be negotiated, and in
C<LIBNBD_TLS_DISABLE> mode TLS is not tried.";
    see_also = [Link "set_tls"; Link "get_tls"];
  };

  "set_tls_certificates", {
    default_call with
    args = [Path "dir"]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "set the path to the TLS certificates directory";
    longdesc = "\
Set the path to the TLS certificates directory.  If not
set and TLS is used then a compiled in default is used.
For root this is C</etc/pki/libnbd/>.  For non-root this is
C<$HOME/.pki/libnbd> and C<$HOME/.config/pki/libnbd>.  If
none of these directories can be found then the system
trusted CAs are used.

This function may be called regardless of whether TLS is
supported, but will have no effect unless L<nbd_set_tls(3)>
is also used to request or require TLS.";
    see_also = [Link "set_tls"];
  };

(* Can't implement this because we need a way to return string that
   can be NULL.
  "get_tls_certificates", {
    default_call with
    args = []; ret = RString;
    shortdesc = "get the current TLS certificates directory";
    longdesc = "\
Get the current TLS directory.";
    see_also = [Link "set_tls_certificates"];
  };
*)

  "set_tls_verify_peer", {
    default_call with
    args = [Bool "verify"]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "set whether we verify the identity of the server";
    longdesc = "\
Set this flag to control whether libnbd will verify the identity
of the server from the server's certificate and the certificate
authority.  This defaults to true when connecting to TCP servers
using TLS certificate authentication, and false otherwise.

This function may be called regardless of whether TLS is
supported, but will have no effect unless L<nbd_set_tls(3)>
is also used to request or require TLS.";
    see_also = [Link "set_tls"; Link "get_tls_verify_peer"];
  };

  "get_tls_verify_peer", {
    default_call with
    args = []; ret = RBool;
    may_set_error = false;
    shortdesc = "get whether we verify the identity of the server";
    longdesc = "\
Get the verify peer flag.";
    see_also = [Link "set_tls_verify_peer"];
  };

  "set_tls_username", {
    default_call with
    args = [String "username"]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "set the TLS username";
    longdesc = "\
Set the TLS client username.  This is used
if authenticating with PSK over TLS is enabled.
If not set then the local username is used.

This function may be called regardless of whether TLS is
supported, but will have no effect unless L<nbd_set_tls(3)>
is also used to request or require TLS.";
    example = Some "examples/encryption.c";
    see_also = [Link "get_tls_username"; Link "set_tls"];
  };

  "get_tls_username", {
    default_call with
    args = []; ret = RString;
    shortdesc = "get the current TLS username";
    longdesc = "\
Get the current TLS username.";
    see_also = [Link "set_tls_username"];
  };

  "set_tls_psk_file", {
    default_call with
    args = [Path "filename"]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "set the TLS Pre-Shared Keys (PSK) filename";
    longdesc = "\
Set the TLS Pre-Shared Keys (PSK) filename.  This is used
if trying to authenticate to the server using with a pre-shared
key.  There is no default so if this is not set then PSK
authentication cannot be used to connect to the server.

This function may be called regardless of whether TLS is
supported, but will have no effect unless L<nbd_set_tls(3)>
is also used to request or require TLS.";
    example = Some "examples/encryption.c";
    see_also = [Link "set_tls"];
  };

(* Can't implement this because we need a way to return string that
   can be NULL.
  "get_tls_psk_file", {
    default_call with
    args = []; ret = RString;
    shortdesc = "get the current TLS PSK filename";
    longdesc = "\
Get the current TLS PSK filename.";
    see_also = [Link "set_tls_psk_file"];
  };
*)

  "set_request_structured_replies", {
    default_call with
    args = [Bool "request"]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "control use of structured replies";
    longdesc = "\
By default, libnbd tries to negotiate structured replies with the
server, as this protocol extension must be in use before
L<nbd_can_meta_context(3)> or L<nbd_can_df(3)> can return true.  However,
for integration testing, it can be useful to clear this flag
rather than find a way to alter the server to fail the negotiation
request.";
    see_also = [Link "get_request_structured_replies";
                Link "set_handshake_flags"; Link "set_strict_mode";
                Link "get_structured_replies_negotiated";
                Link "can_meta_context"; Link "can_df"];
  };

  "get_request_structured_replies", {
    default_call with
    args = []; ret = RBool;
    may_set_error = false;
    shortdesc = "see if structured replies are attempted";
    longdesc = "\
Return the state of the request structured replies flag on this
handle.

B<Note:> If you want to find out if structured replies were actually
negotiated on a particular connection use
L<nbd_get_structured_replies_negotiated(3)> instead.";
    see_also = [Link "set_request_structured_replies";
                Link "get_structured_replies_negotiated"];
  };

  "get_structured_replies_negotiated", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "see if structured replies are in use";
    longdesc = "\
After connecting you may call this to find out if the connection is
using structured replies.";
    see_also = [Link "set_request_structured_replies";
                Link "get_request_structured_replies";
                Link "get_protocol"];
  };

  "set_handshake_flags", {
    default_call with
    args = [ Flags ("flags", handshake_flags) ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "control use of handshake flags";
    longdesc = "\
By default, libnbd tries to negotiate all possible handshake flags
that are also supported by the server, since omitting a handshake
flag can prevent the use of other functionality such as TLS encryption
or structured replies.  However, for integration testing, it can be
useful to reduce the set of flags supported by the client to test that
a particular server can handle various clients that were compliant to
older versions of the NBD specification.

The C<flags> argument is a bitmask, including zero or more of the
following handshake flags:

=over 4

=item C<LIBNBD_HANDSHAKE_FLAG_FIXED_NEWSTYLE> = 1

The server gracefully handles unknown option requests from the
client, rather than disconnecting.  Without this flag, a client
cannot safely request to use extensions such as TLS encryption or
structured replies, as the request may cause an older server to
drop the connection.

=item C<LIBNBD_HANDSHAKE_FLAG_NO_ZEROES> = 2

If the client is forced to use C<NBD_OPT_EXPORT_NAME> instead of
the preferred C<NBD_OPT_GO>, this flag allows the server to send
fewer all-zero padding bytes over the connection.

=back

For convenience, the constant C<LIBNBD_HANDSHAKE_FLAG_MASK> is
available to describe all flags supported by this build of libnbd.
Future NBD extensions may add further flags, which in turn may
be enabled by default in newer libnbd.  As such, when attempting
to disable only one specific bit, it is wiser to first call
L<nbd_get_handshake_flags(3)> and modify that value, rather than
blindly setting a constant value.";
    see_also = [Link "get_handshake_flags";
                Link "set_request_structured_replies"];
  };

  "get_handshake_flags", {
    default_call with
    args = []; ret = RFlags handshake_flags;
    may_set_error = false;
    shortdesc = "see which handshake flags are supported";
    longdesc = "\
Return the state of the handshake flags on this handle.  When the
handle has not yet completed a connection (see L<nbd_aio_is_created(3)>),
this returns the flags that the client is willing to use, provided
the server also advertises those flags.  After the connection is
ready (see L<nbd_aio_is_ready(3)>), this returns the flags that were
actually agreed on between the server and client.  If the NBD
protocol defines new handshake flags, then the return value from
a newer library version may include bits that were undefined at
the time of compilation.";
    see_also = [Link "set_handshake_flags";
                Link "get_protocol"; Link "set_strict_mode";
                Link "aio_is_created"; Link "aio_is_ready"];
  };

  "set_pread_initialize", {
    default_call with
    args = [Bool "request"]; ret = RErr;
    shortdesc = "control whether libnbd pre-initializes read buffers";
    longdesc = "\
By default, libnbd will pre-initialize the contents of a buffer
passed to calls such as L<nbd_pread(3)> to all zeroes prior to
checking for any other errors, so that even if a client application
passed in an uninitialized buffer but fails to check for errors, it
will not result in a potential security risk caused by an accidental
leak of prior heap contents (see CVE-2022-0485 in
L<libnbd-security(3)> for an example of a security hole in an
application built against an earlier version of libnbd that lacked
consistent pre-initialization).  However, for a client application
that has audited that an uninitialized buffer is never dereferenced,
or which performs its own pre-initialization, libnbd's sanitization
efforts merely pessimize performance (although the time spent in
pre-initialization may pale in comparison to time spent waiting on
network packets).

Calling this function with C<request> set to false tells libnbd to
skip the buffer initialization step in read commands.";
    see_also = [Link "get_pread_initialize";
                Link "set_strict_mode";
                Link "pread"; Link "pread_structured"; Link "aio_pread";
                Link "aio_pread_structured"];
  };

  "get_pread_initialize", {
    default_call with
    args = []; ret = RBool;
    may_set_error = false;
    shortdesc = "see whether libnbd pre-initializes read buffers";
    longdesc = "\
Return whether libnbd performs a pre-initialization of a buffer passed
to L<nbd_pread(3)> and similar to all zeroes, as set by
L<nbd_set_pread_initialize(3)>.";
    see_also = [Link "set_pread_initialize";
                Link "set_strict_mode";
                Link "pread"; Link "pread_structured"; Link "aio_pread";
                Link "aio_pread_structured"];
  };

  "set_strict_mode", {
    default_call with
    args = [ Flags ("flags", strict_flags) ]; ret = RErr;
    shortdesc = "control how strictly to follow NBD protocol";
    longdesc = "\
By default, libnbd tries to detect requests that would trigger
undefined behavior in the NBD protocol, and rejects them client
side without causing any network traffic, rather than risking
undefined server behavior.  However, for integration testing, it
can be handy to relax the strictness of libnbd, to coerce it into
sending such requests over the network for testing the robustness
of the server in dealing with such traffic.

The C<flags> argument is a bitmask, including zero or more of the
following strictness flags:

=over 4

=item C<LIBNBD_STRICT_COMMANDS> = 1

If set, this flag rejects client requests that do not comply with the
set of advertised server flags (for example, attempting a write on
a read-only server, or attempting to use C<LIBNBD_CMD_FLAG_FUA> when
L<nbd_can_fua(3)> returned false).  If clear, this flag relies on the
server to reject unexpected commands.

=item C<LIBNBD_STRICT_FLAGS> = 2

If set, this flag rejects client requests that attempt to set a
command flag not recognized by libnbd (those outside of
C<LIBNBD_CMD_FLAG_MASK>), or a flag not normally associated with
a command (such as using C<LIBNBD_CMD_FLAG_FUA> on a read command).
If clear, all flags are sent on to the server, even if sending such
a flag may cause the server to change its reply in a manner that
confuses libnbd, perhaps causing deadlock or ending the connection.

Flags that are known by libnbd as associated with a given command
(such as C<LIBNBD_CMD_FLAG_DF> for L<nbd_pread_structured(3)> gated
by L<nbd_can_df(3)>) are controlled by C<LIBNBD_STRICT_COMMANDS>
instead.

Note that the NBD protocol only supports 16 bits of command flags,
even though the libnbd API uses C<uint32_t>; bits outside of the
range permitted by the protocol are always a client-side error.

=item C<LIBNBD_STRICT_BOUNDS> = 3

If set, this flag rejects client requests that would exceed the export
bounds without sending any traffic to the server.  If clear, this flag
relies on the server to detect out-of-bounds requests.

=item C<LIBNBD_STRICT_ZERO_SIZE> = 4

If set, this flag rejects client requests with length 0.  If clear,
this permits zero-length requests to the server, which may produce
undefined results.

=item C<LIBNBD_STRICT_ALIGN> = 5

If set, and the server provided minimum block sizes (see
L<nbd_get_block_size(3)>, this flag rejects client requests that
do not have length and offset aligned to the server's minimum
requirements.  If clear, unaligned requests are sent to the server,
where it is up to the server whether to honor or reject the request.

=back

For convenience, the constant C<LIBNBD_STRICT_MASK> is available to
describe all strictness flags supported by this build of libnbd.
Future versions of libnbd may add further flags, which are likely
to be enabled by default for additional client-side filtering.  As
such, when attempting to relax only one specific bit while keeping
remaining checks at the client side, it is wiser to first call
L<nbd_get_strict_mode(3)> and modify that value, rather than
blindly setting a constant value.";
    see_also = [Link "get_strict_mode"; Link "set_handshake_flags";
                Link "stats_bytes_sent"; Link "stats_bytes_received"];
  };

  "get_strict_mode", {
    default_call with
    args = []; ret = RFlags strict_flags;
    may_set_error = false;
    shortdesc = "see which strictness flags are in effect";
    longdesc = "\
Return flags indicating which protocol strictness items are being
enforced locally by libnbd rather than the server.  The return value
from a newer library version may include bits that were undefined at
the time of compilation.";
    see_also = [Link "set_strict_mode"];
  };

  "set_opt_mode", {
    default_call with
    args = [Bool "enable"]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "control option mode, for pausing during option negotiation";
    longdesc = "\
Set this flag to true in order to request that a connection command
C<nbd_connect_*> will pause for negotiation options rather than
proceeding all the way to the ready state, when communicating with a
newstyle server.  This setting has no effect when connecting to an
oldstyle server.

When option mode is enabled, you have fine-grained control over which
options are negotiated, compared to the default of the server
negotiating everything on your behalf using settings made before
starting the connection.  To leave the mode and proceed on to the
ready state, you must use L<nbd_opt_go(3)> successfully; a failed
L<nbd_opt_go(3)> returns to the negotiating state to allow a change of
export name before trying again.  You may also use L<nbd_opt_abort(3)>
to end the connection without finishing negotiation.";
    example = Some "examples/list-exports.c";
    see_also = [Link "get_opt_mode"; Link "aio_is_negotiating";
                Link "opt_abort"; Link "opt_go"; Link "opt_list";
                Link "opt_info"; Link "opt_list_meta_context";
                Link "aio_connect"];
  };

  "get_opt_mode", {
    default_call with
    args = []; ret = RBool;
    may_set_error = false;
    shortdesc = "return whether option mode was enabled";
    longdesc = "\
Return true if option negotiation mode was enabled on this handle.";
    see_also = [Link "set_opt_mode"];
  };

  "opt_go", {
    default_call with
    args = []; ret = RErr;
    permitted_states = [ Negotiating ];
    shortdesc = "end negotiation and move on to using an export";
    longdesc = "\
Request that the server finish negotiation and move on to serving the
export previously specified by the most recent L<nbd_set_export_name(3)>
or L<nbd_connect_uri(3)>.  This can only be used if
L<nbd_set_opt_mode(3)> enabled option mode.

If this fails, the server may still be in negotiation, where it is
possible to attempt another option such as a different export name;
although older servers will instead have killed the connection.";
    example = Some "examples/list-exports.c";
    see_also = [Link "set_opt_mode"; Link "aio_opt_go"; Link "opt_abort";
                Link "set_export_name"; Link "connect_uri"; Link "opt_info"];
  };

  "opt_abort", {
    default_call with
    args = []; ret = RErr;
    permitted_states = [ Negotiating ];
    shortdesc = "end negotiation and close the connection";
    longdesc = "\
Request that the server finish negotiation, gracefully if possible, then
close the connection.  This can only be used if L<nbd_set_opt_mode(3)>
enabled option mode.";
    example = Some "examples/list-exports.c";
    see_also = [Link "set_opt_mode"; Link "aio_opt_abort"; Link "opt_go"];
  };

  "opt_list", {
    default_call with
    args = [ Closure list_closure ]; ret = RInt;
    permitted_states = [ Negotiating ];
    shortdesc = "request the server to list all exports during negotiation";
    longdesc = "\
Request that the server list all exports that it supports.  This can
only be used if L<nbd_set_opt_mode(3)> enabled option mode.

The C<list> function is called once per advertised export, with any
C<user_data> passed to this function, and with C<name> and C<description>
supplied by the server.  Many servers omit descriptions, in which
case C<description> will be an empty string.  Remember that it is not
safe to call L<nbd_set_export_name(3)> from within the context of the
callback function; rather, your code must copy any C<name> needed for
later use after this function completes.  At present, the return value
of the callback is ignored, although a return of -1 should be avoided.

For convenience, when this function succeeds, it returns the number
of exports that were advertised by the server.

Not all servers understand this request, and even when it is understood,
the server might intentionally send an empty list to avoid being an
information leak, may encounter a failure after delivering partial
results, or may refuse to answer more than one query per connection
in the interest of avoiding negotiation that does not resolve.  Thus,
this function may succeed even when no exports are reported, or may
fail but have a non-empty list.  Likewise, the NBD protocol does not
specify an upper bound for the number of exports that might be
advertised, so client code should be aware that a server may send a
lengthy list.

For L<nbd-server(1)> you will need to allow clients to make
list requests by adding C<allowlist=true> to the C<[generic]>
section of F</etc/nbd-server/config>.  For L<qemu-nbd(8)>, a
description is set with I<-D>.";
    example = Some "examples/list-exports.c";
    see_also = [Link "set_opt_mode"; Link "aio_opt_list"; Link "opt_go";
                Link "set_export_name"];
  };

  "opt_info", {
    default_call with
    args = []; ret = RErr;
    permitted_states = [ Negotiating ];
    shortdesc = "request the server for information about an export";
    longdesc = "\
Request that the server supply information about the export name
previously specified by the most recent L<nbd_set_export_name(3)>
or L<nbd_connect_uri(3)>.  This can only be used if
L<nbd_set_opt_mode(3)> enabled option mode.

If successful, functions like L<nbd_is_read_only(3)> and
L<nbd_get_size(3)> will report details about that export.  In
general, if L<nbd_opt_go(3)> is called next, that call will
likely succeed with the details remaining the same, although this
is not guaranteed by all servers.

Not all servers understand this request, and even when it is
understood, the server might fail the request even when a
corresponding L<nbd_opt_go(3)> would succeed.";
    see_also = [Link "set_opt_mode"; Link "aio_opt_info"; Link "opt_go";
                Link "set_export_name"];
  };

  "opt_list_meta_context", {
    default_call with
    args = [ Closure context_closure ]; ret = RInt;
    permitted_states = [ Negotiating ];
    shortdesc = "list available meta contexts, using implicit query list";
    longdesc = "\
Request that the server list available meta contexts associated with
the export previously specified by the most recent
L<nbd_set_export_name(3)> or L<nbd_connect_uri(3)>, and with a
list of queries from prior calls to L<nbd_add_meta_context(3)>
(see L<nbd_opt_list_meta_context_queries(3)> if you want to supply
an explicit query list instead).  This can only be used if
L<nbd_set_opt_mode(3)> enabled option mode.

The NBD protocol allows a client to decide how many queries to ask
the server.  Rather than taking that list of queries as a parameter
to this function, libnbd reuses the current list of requested meta
contexts as set by L<nbd_add_meta_context(3)>; you can use
L<nbd_clear_meta_contexts(3)> to set up a different list of queries.
When the list is empty, a server will typically reply with all
contexts that it supports; when the list is non-empty, the server
will reply only with supported contexts that match the client's
request.  Note that a reply by the server might be encoded to
represent several feasible contexts within one string, rather than
multiple strings per actual context name that would actually succeed
during L<nbd_opt_go(3)>; so it is still necessary to use
L<nbd_can_meta_context(3)> after connecting to see which contexts
are actually supported.

The C<context> function is called once per server reply, with any
C<user_data> passed to this function, and with C<name> supplied by
the server.  Remember that it is not safe to call
L<nbd_add_meta_context(3)> from within the context of the
callback function; rather, your code must copy any C<name> needed for
later use after this function completes.  At present, the return value
of the callback is ignored, although a return of -1 should be avoided.

For convenience, when this function succeeds, it returns the number
of replies returned by the server.

Not all servers understand this request, and even when it is understood,
the server might intentionally send an empty list because it does not
support the requested context, or may encounter a failure after
delivering partial results.  Thus, this function may succeed even when
no contexts are reported, or may fail but have a non-empty list.  Likewise,
the NBD protocol does not specify an upper bound for the number of
replies that might be advertised, so client code should be aware that
a server may send a lengthy list.";
    see_also = [Link "set_opt_mode"; Link "aio_opt_list_meta_context";
                Link "opt_list_meta_context_queries";
                Link "add_meta_context"; Link "clear_meta_contexts";
                Link "opt_go"; Link "set_export_name"];
  };

  "opt_list_meta_context_queries", {
    default_call with
    args = [ StringList "queries"; Closure context_closure ]; ret = RInt;
    permitted_states = [ Negotiating ];
    shortdesc = "list available meta contexts, using explicit query list";
    longdesc = "\
Request that the server list available meta contexts associated with
the export previously specified by the most recent
L<nbd_set_export_name(3)> or L<nbd_connect_uri(3)>, and with an
explicit list of queries provided as a parameter (see
L<nbd_opt_list_meta_context(3)> if you want to reuse an
implicit query list instead).  This can only be used if
L<nbd_set_opt_mode(3)> enabled option mode.

The NBD protocol allows a client to decide how many queries to ask
the server.  For this function, the list is explicit in the C<queries>
parameter.  When the list is empty, a server will typically reply with all
contexts that it supports; when the list is non-empty, the server
will reply only with supported contexts that match the client's
request.  Note that a reply by the server might be encoded to
represent several feasible contexts within one string, rather than
multiple strings per actual context name that would actually succeed
during L<nbd_opt_go(3)>; so it is still necessary to use
L<nbd_can_meta_context(3)> after connecting to see which contexts
are actually supported.

The C<context> function is called once per server reply, with any
C<user_data> passed to this function, and with C<name> supplied by
the server.  Remember that it is not safe to call
L<nbd_add_meta_context(3)> from within the context of the
callback function; rather, your code must copy any C<name> needed for
later use after this function completes.  At present, the return value
of the callback is ignored, although a return of -1 should be avoided.

For convenience, when this function succeeds, it returns the number
of replies returned by the server.

Not all servers understand this request, and even when it is understood,
the server might intentionally send an empty list because it does not
support the requested context, or may encounter a failure after
delivering partial results.  Thus, this function may succeed even when
no contexts are reported, or may fail but have a non-empty list.  Likewise,
the NBD protocol does not specify an upper bound for the number of
replies that might be advertised, so client code should be aware that
a server may send a lengthy list.";
    see_also = [Link "set_opt_mode"; Link "aio_opt_list_meta_context_queries";
                Link "opt_list_meta_context";
                Link "opt_go"; Link "set_export_name"];
  };

  "add_meta_context", {
    default_call with
    args = [ String "name" ]; ret = RErr;
    permitted_states = [ Created; Negotiating ];
    shortdesc = "ask server to negotiate metadata context";
    longdesc = "\
During connection libnbd can negotiate zero or more metadata
contexts with the server.  Metadata contexts are features (such
as C<\"base:allocation\">) which describe information returned
by the L<nbd_block_status(3)> command (for C<\"base:allocation\">
this is whether blocks of data are allocated, zero or sparse).

This call adds one metadata context to the list to be negotiated.
You can call it as many times as needed.  The list is initially
empty when the handle is created; you can check the contents of
the list with L<nbd_get_nr_meta_contexts(3)> and
L<nbd_get_meta_context(3)>, or clear it with
L<nbd_clear_meta_contexts(3)>.

The NBD protocol limits meta context names to 4096 bytes, but
servers may not support the full length.  The encoding of meta
context names is always UTF-8.

Not all servers support all metadata contexts.  To learn if a context
was actually negotiated, call L<nbd_can_meta_context(3)> after
connecting.

The single parameter is the name of the metadata context,
for example C<LIBNBD_CONTEXT_BASE_ALLOCATION>.
B<E<lt>libnbd.hE<gt>> includes defined constants beginning with
C<LIBNBD_CONTEXT_> for some well-known contexts, but you are free
to pass in other contexts.

Other metadata contexts are server-specific, but include
C<\"qemu:dirty-bitmap:...\"> and C<\"qemu:allocation-depth\"> for
qemu-nbd (see qemu-nbd I<-B> and I<-A> options).";
    see_also = [Link "block_status"; Link "can_meta_context";
                Link "get_nr_meta_contexts"; Link "get_meta_context";
                Link "clear_meta_contexts"];
  };

  "get_nr_meta_contexts", {
    default_call with
    args = []; ret = RSizeT;
    shortdesc = "return the current number of requested meta contexts";
    longdesc = "\
During connection libnbd can negotiate zero or more metadata
contexts with the server.  Metadata contexts are features (such
as C<\"base:allocation\">) which describe information returned
by the L<nbd_block_status(3)> command (for C<\"base:allocation\">
this is whether blocks of data are allocated, zero or sparse).

This command returns how many meta contexts have been added to
the list to request from the server via L<nbd_add_meta_context(3)>.
The server is not obligated to honor all of the requests; to see
what it actually supports, see L<nbd_can_meta_context(3)>.";
    see_also = [Link "block_status"; Link "can_meta_context";
                Link "add_meta_context"; Link "get_meta_context";
                Link "clear_meta_contexts"];
  };

  "get_meta_context", {
    default_call with
    args = [ SizeT "i" ]; ret = RString;
    shortdesc = "return the i'th meta context request";
    longdesc = "\
During connection libnbd can negotiate zero or more metadata
contexts with the server.  Metadata contexts are features (such
as C<\"base:allocation\">) which describe information returned
by the L<nbd_block_status(3)> command (for C<\"base:allocation\">
this is whether blocks of data are allocated, zero or sparse).

This command returns the i'th meta context request, as added by
L<nbd_add_meta_context(3)>, and bounded by
L<nbd_get_nr_meta_contexts(3)>.";
    see_also = [Link "block_status"; Link "can_meta_context";
                Link "add_meta_context"; Link "get_nr_meta_contexts";
                Link "clear_meta_contexts"];
  };

  "clear_meta_contexts", {
    default_call with
    args = []; ret = RErr;
    permitted_states = [ Created; Negotiating ];
    shortdesc = "reset the list of requested meta contexts";
    longdesc = "\
During connection libnbd can negotiate zero or more metadata
contexts with the server.  Metadata contexts are features (such
as C<\"base:allocation\">) which describe information returned
by the L<nbd_block_status(3)> command (for C<\"base:allocation\">
this is whether blocks of data are allocated, zero or sparse).

This command resets the list of meta contexts to request back to
an empty list, for re-population by further use of
L<nbd_add_meta_context(3)>.  It is primarily useful when option
negotiation mode is selected (see L<nbd_set_opt_mode(3)>), for
altering the list of attempted contexts between subsequent export
queries.";
    see_also = [Link "block_status"; Link "can_meta_context";
                Link "add_meta_context"; Link "get_nr_meta_contexts";
                Link "get_meta_context"; Link "set_opt_mode"];
  };

  "set_uri_allow_transports", {
    default_call with
    args = [ Flags ("mask", allow_transport_flags) ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "set the allowed transports in NBD URIs";
    longdesc = "\
Set which transports are allowed to appear in NBD URIs.  The
default is to allow any transport.

The C<mask> parameter may contain any of the following flags
ORed together:

=over 4

=item C<LIBNBD_ALLOW_TRANSPORT_TCP>

=item C<LIBNBD_ALLOW_TRANSPORT_UNIX>

=item C<LIBNBD_ALLOW_TRANSPORT_VSOCK>

=back

For convenience, the constant C<LIBNBD_ALLOW_TRANSPORT_MASK> is
available to describe all transports recognized by this build of
libnbd.  A future version of the library may add new flags.";
    see_also = [Link "connect_uri"; Link "set_uri_allow_tls"];
  };

  "set_uri_allow_tls", {
    default_call with
    args = [ Enum ("tls", tls_enum) ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "set the allowed TLS settings in NBD URIs";
    longdesc = "\
Set which TLS settings are allowed to appear in NBD URIs.  The
default is to allow either non-TLS or TLS URIs.

The C<tls> parameter can be:

=over 4

=item C<LIBNBD_TLS_DISABLE>

TLS URIs are not permitted, ie. a URI such as C<nbds://...>
will be rejected.

=item C<LIBNBD_TLS_ALLOW>

This is the default.  TLS may be used or not, depending on
whether the URI uses C<nbds> or C<nbd>.

=item C<LIBNBD_TLS_REQUIRE>

TLS URIs are required.  All URIs must use C<nbds>.

=back";
    see_also = [Link "connect_uri"; Link "set_uri_allow_transports"];
  };

  "set_uri_allow_local_file", {
    default_call with
    args = [ Bool "allow" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "set the allowed transports in NBD URIs";
    longdesc = "\
Allow NBD URIs to reference local files.  This is I<disabled>
by default.

Currently this setting only controls whether the C<tls-psk-file>
parameter in NBD URIs is allowed.";
    see_also = [Link "connect_uri"];
  };

  "connect_uri", {
    default_call with
    args = [ String "uri" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to NBD URI";
    longdesc = "\
Connect (synchronously) to an NBD server and export by specifying
the NBD URI.  This call parses the URI and calls
L<nbd_set_export_name(3)> and L<nbd_set_tls(3)> and other
calls as needed, followed by L<nbd_connect_tcp(3)>,
L<nbd_connect_unix(3)> or L<nbd_connect_vsock(3)>.
" ^ blocking_connect_call_description ^ "

=head2 Example URIs supported

=over 4

=item C<nbd://example.com>

Connect over TCP, unencrypted, to C<example.com> port 10809.

=item C<nbds://example.com>

Connect over TCP with TLS, to C<example.com> port 10809.  If
the server does not support TLS then this will fail.

=item C<nbd+unix:///foo?socket=/tmp/nbd.sock>

Connect over the Unix domain socket F</tmp/nbd.sock> to
an NBD server running locally.  The export name is set to C<foo>
(note without any leading C</> character).

=item C<nbds+unix://alice@/?socket=/tmp/nbd.sock&tls-certificates=certs>

Connect over a Unix domain socket, enabling TLS and setting the
path to a directory containing certificates and keys.

=item C<nbd+vsock:///>

In this scenario libnbd is running in a virtual machine.  Connect
over C<AF_VSOCK> to an NBD server running on the hypervisor.

=back

=head2 Supported URI formats

The following schemes are supported in the current version
of libnbd:

=over 4

=item C<nbd:>

Connect over TCP without using TLS.

=item C<nbds:>

Connect over TCP.  TLS is required and the connection
will fail if the server does not support TLS.

=item C<nbd+unix:>

=item C<nbds+unix:>

Connect over a Unix domain socket, without or with TLS
respectively.  The C<socket> parameter is required.

=item C<nbd+vsock:>

=item C<nbds+vsock:>

Connect over the C<AF_VSOCK> transport, without or with
TLS respectively. You can use L<nbd_supports_vsock(3)> to
see if this build of libnbd supports C<AF_VSOCK>.

=back

The authority part of the URI (C<[username@][servername][:port]>)
is parsed depending on the transport.  For TCP it specifies the
server to connect to and optional port number.  For C<+unix>
it should not be present.  For C<+vsock> the server name is the
numeric CID (eg. C<2> to connect to the host), and the optional
port number may be present.  If the C<username> is present it
is used for TLS authentication.

For all transports, an export name may be present, parsed in
accordance with the NBD URI specification.

Finally the query part of the URI can contain:

=over 4

=item B<socket=>F<SOCKET>

Specifies the Unix domain socket to connect on.
Must be present for the C<+unix> transport and must not
be present for the other transports.

=item B<tls-certificates=>F<DIR>

Set the certificates directory.  See L<nbd_set_tls_certificates(3)>.
Note this is not allowed by default - see next section.

=item B<tls-psk-file=>F<PSKFILE>

Set the PSK file.  See L<nbd_set_tls_psk_file(3)>.  Note
this is not allowed by default - see next section.

=back

=head2 Disable URI features

For security reasons you might want to disable certain URI
features.  Pre-filtering URIs is error-prone and should not
be attempted.  Instead use the libnbd APIs below to control
what can appear in URIs.  Note you must call these functions
on the same handle before calling L<nbd_connect_uri(3)> or
L<nbd_aio_connect_uri(3)>.

=over 4

=item TCP, Unix domain socket or C<AF_VSOCK> transports

Default: all allowed

To select which transports are allowed call
L<nbd_set_uri_allow_transports(3)>.

=item TLS

Default: both non-TLS and TLS connections allowed

To force TLS off or on in URIs call
L<nbd_set_uri_allow_tls(3)>.

=item Connect to Unix domain socket in the local filesystem

Default: allowed

To prevent this you must disable the C<+unix> transport
using L<nbd_set_uri_allow_transports(3)>.

=item Read from local files

Default: denied

To allow URIs to contain references to local files
(eg. for parameters like C<tls-psk-file>) call
L<nbd_set_uri_allow_local_file(3)>.

=back

=head2 Overriding the export name

It is possible to override the export name portion of a URI
by using L<nbd_set_opt_mode(3)> to enable option mode,
then using L<nbd_set_export_name(3)> and L<nbd_opt_go(3)>
as part of subsequent negotiation.

=head2 Optional features

This call will fail if libnbd was not compiled with libxml2; you can
test whether this is the case with L<nbd_supports_uri(3)>.

Support for URIs that require TLS will fail if libnbd was not
compiled with gnutls; you can test whether this is the case
with L<nbd_supports_tls(3)>.

=head2 Constructing a URI from an existing connection

See L<nbd_get_uri(3)>.";
    see_also = [URLLink "https://github.com/NetworkBlockDevice/nbd/blob/master/doc/uri.md";
                Link "aio_connect_uri";
                Link "set_export_name"; Link "set_tls";
                Link "set_opt_mode"; Link "get_uri";
                Link "supports_vsock"; Link "supports_uri"];
  };

  "connect_unix", {
    default_call with
    args = [ Path "unixsocket" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to NBD server over a Unix domain socket";
    longdesc = "\
Connect (synchronously) over the named Unix domain socket (C<unixsocket>)
to an NBD server running on the same machine.
" ^ blocking_connect_call_description;
    example = Some "examples/fetch-first-sector.c";
    see_also = [Link "aio_connect_unix"; Link "set_opt_mode"];
  };

  "connect_vsock", {
    default_call with
    args = [ UInt32 "cid"; UInt32 "port" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to NBD server over AF_VSOCK protocol";
    longdesc = "\
Connect (synchronously) over the C<AF_VSOCK> protocol from a
virtual machine to an NBD server, usually running on the host.  The
C<cid> and C<port> parameters specify the server address.  Usually
C<cid> should be C<2> (to connect to the host), and C<port> might be
C<10809> or another port number assigned to you by the host
administrator.

Not all systems support C<AF_VSOCK>; to determine if libnbd was
built on a system with vsock support, see L<nbd_supports_vsock(3)>.
" ^ blocking_connect_call_description;
    see_also = [Link "aio_connect_vsock"; Link "set_opt_mode";
                Link "supports_vsock"];
  };

  "connect_tcp", {
    default_call with
    args = [ String "hostname"; String "port" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to NBD server over a TCP port";
    longdesc = "\
Connect (synchronously) to the NBD server listening on
C<hostname:port>.  The C<port> may be a port name such
as C<\"nbd\">, or it may be a port number as a string
such as C<\"10809\">.
" ^ blocking_connect_call_description;
    see_also = [Link "aio_connect_tcp"; Link "set_opt_mode"];
  };

  "connect_socket", {
    default_call with
    args = [ Fd "sock" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect directly to a connected socket";
    longdesc = "\
Pass a connected socket C<sock> through which libnbd will talk
to the NBD server.

The caller is responsible for creating and connecting this
socket by some method, before passing it to libnbd.

If this call returns without error then socket ownership
is passed to libnbd.  Libnbd will close the socket when the
handle is closed.  The caller must not use the socket in any way.
" ^ blocking_connect_call_description;
    see_also = [Link "aio_connect_socket";
                Link "connect_command"; Link "set_opt_mode";
                ExternalLink ("socket", 7)];
  };

  "connect_command", {
    default_call with
    args = [ StringList "argv" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to NBD server command";
    longdesc = "\
Run the command as a subprocess and connect to it over
stdin/stdout.  This is for use with NBD servers which can
behave like inetd clients, such as L<nbdkit(1)> using
the I<-s>/I<--single> flag, and L<nbd-server(1)> with
port number set to 0.

To run L<qemu-nbd(1)>, use
L<nbd_connect_systemd_socket_activation(3)> instead.

=head2 Subprocess

Libnbd will fork the C<argv> command and pass the NBD socket
to it using file descriptors 0 and 1 (stdin/stdout):

 ┌─────────┬─────────┐    ┌────────────────┐
 │ program │ libnbd  │    │   NBD server   │
 │         │         │    │       (argv)   │
 │         │ socket ╍╍╍╍╍╍╍╍▶ stdin/stdout │
 └─────────┴─────────┘    └────────────────┘

When the NBD handle is closed the server subprocess
is killed.
" ^ blocking_connect_call_description;
    see_also = [Link "aio_connect_command";
                Link "connect_systemd_socket_activation";
                Link "kill_subprocess"; Link "set_opt_mode"];
    example = Some "examples/connect-command.c";
  };

  "connect_systemd_socket_activation", {
    default_call with
    args = [ StringList "argv" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect using systemd socket activation";
    longdesc = "\
Run the command as a subprocess and connect to it using
systemd socket activation.

This is especially useful for running L<qemu-nbd(1)> as
a subprocess of libnbd, for example to use it to open
qcow2 files.

To run nbdkit as a subprocess, this function can be used,
or L<nbd_connect_command(3)>.

To run L<nbd-server(1)> as a subprocess, this function
cannot be used, you must use L<nbd_connect_command(3)>.

=head2 Socket activation

Libnbd will fork the C<argv> command and pass an NBD
socket to it using special C<LISTEN_*> environment
variables (as defined by the systemd socket activation
protocol).

 ┌─────────┬─────────┐    ┌───────────────┐
 │ program │ libnbd  │    │  qemu-nbd or  │
 │         │         │    │  other server │
 │         │ socket ╍╍╍╍╍╍╍╍▶             │
 └─────────┴─────────┘    └───────────────┘

When the NBD handle is closed the server subprocess
is killed.
" ^ blocking_connect_call_description;
    see_also = [Link "aio_connect_systemd_socket_activation";
                Link "connect_command"; Link "kill_subprocess";
                Link "set_opt_mode";
                ExternalLink ("qemu-nbd", 1);
                URLLink "http://0pointer.de/blog/projects/socket-activation.html"];
    example = Some "examples/open-qcow2.c";
  };

  "is_read_only", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "is the NBD export read-only?";
    longdesc = "\
Returns true if the NBD export is read-only; writes and
write-like operations will fail."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls"; Link "opt_info"];
    example = Some "examples/server-flags.c";
  };

  "can_flush", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "does the server support the flush command?";
    longdesc = "\
Returns true if the server supports the flush command
(see L<nbd_flush(3)>, L<nbd_aio_flush(3)>).  Returns false if
the server does not."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls"; Link "opt_info";
                Link "flush"; Link "aio_flush"];
    example = Some "examples/server-flags.c";
  };

  "can_fua", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "does the server support the FUA flag?";
    longdesc = "\
Returns true if the server supports the FUA flag on
certain commands (see L<nbd_pwrite(3)>)."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls"; Link "opt_info"; Link "pwrite";
                Link "zero"; Link "trim"];
    example = Some "examples/server-flags.c";
  };

  "is_rotational", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "is the NBD disk rotational (like a disk)?";
    longdesc = "\
Returns true if the disk exposed over NBD is rotational
(like a traditional floppy or hard disk).  Returns false if
the disk has no penalty for random access (like an SSD or
RAM disk)."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls"; Link "opt_info"];
    example = Some "examples/server-flags.c";
  };

  "can_trim", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "does the server support the trim command?";
    longdesc = "\
Returns true if the server supports the trim command
(see L<nbd_trim(3)>, L<nbd_aio_trim(3)>).  Returns false if
the server does not."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls"; Link "opt_info";
                Link "trim"; Link "aio_trim"];
    example = Some "examples/server-flags.c";
  };

  "can_zero", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "does the server support the zero command?";
    longdesc = "\
Returns true if the server supports the zero command
(see L<nbd_zero(3)>, L<nbd_aio_zero(3)>).  Returns false if
the server does not."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls"; Link "opt_info";
                Link "zero"; Link "aio_zero";
                Link "can_fast_zero"];
    example = Some "examples/server-flags.c";
  };

  "can_fast_zero", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "does the server support the fast zero flag?";
    longdesc = "\
Returns true if the server supports the use of the
C<LIBNBD_CMD_FLAG_FAST_ZERO> flag to the zero command
(see L<nbd_zero(3)>, L<nbd_aio_zero(3)>).  Returns false if
the server does not."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls"; Link "opt_info";
                Link "zero"; Link "aio_zero"; Link "can_zero"];
    example = Some "examples/server-flags.c";
  };

  "can_df", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "does the server support the don't fragment flag to pread?";
    longdesc = "\
Returns true if the server supports structured reads with an
ability to request a non-fragmented read (see L<nbd_pread_structured(3)>,
L<nbd_aio_pread_structured(3)>).  Returns false if the server either lacks
structured reads or if it does not support a non-fragmented read request."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls"; Link "opt_info";
                Link "pread_structured";
                Link "aio_pread_structured"];
    example = Some "examples/server-flags.c";
  };

  "can_multi_conn", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "does the server support multi-conn?";
    longdesc = "\
Returns true if the server supports multi-conn.  Returns
false if the server does not.

It is not safe to open multiple handles connecting to the
same server if you will write to the server and the
server does not advertise multi-conn support.  The safe
way to check for this is to open one connection, check
this flag is true, then open further connections as
required."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls"; SectionLink "Multi-conn";
                Link "opt_info"];
    example = Some "examples/server-flags.c";
  };

  "can_cache", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "does the server support the cache command?";
    longdesc = "\
Returns true if the server supports the cache command
(see L<nbd_cache(3)>, L<nbd_aio_cache(3)>).  Returns false if
the server does not."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls"; Link "opt_info";
                Link "cache"; Link "aio_cache"];
    example = Some "examples/server-flags.c";
  };

  "can_meta_context", {
    default_call with
    args = [String "metacontext"]; ret = RBool;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "does the server support a specific meta context?";
    longdesc = "\
Returns true if the server supports the given meta context
(see L<nbd_add_meta_context(3)>).  Returns false if
the server does not.  It is possible for this command to fail if
meta contexts were requested but there is a missing or failed
attempt at NBD_OPT_SET_META_CONTEXT during option negotiation.

The single parameter is the name of the metadata context,
for example C<LIBNBD_CONTEXT_BASE_ALLOCATION>.
B<E<lt>libnbd.hE<gt>> includes defined constants for well-known
namespace contexts beginning with C<LIBNBD_CONTEXT_>, but you
are free to pass in other contexts."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls"; Link "opt_info";
                Link "add_meta_context";
                Link "block_status"; Link "aio_block_status"];
  };

  "get_protocol", {
    default_call with
    args = []; ret = RStaticString;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "return the NBD protocol variant";
    longdesc = "\
Return the NBD protocol variant in use on the connection.  At
the moment this returns one of the strings C<\"oldstyle\">,
C<\"newstyle\"> or C<\"newstyle-fixed\">.  Other strings might
be returned in the future.
Most modern NBD servers use C<\"newstyle-fixed\">.
"
^ non_blocking_test_call_description;
    see_also = [Link "get_handshake_flags";
                Link "get_structured_replies_negotiated";
                Link "get_tls_negotiated";
                Link "get_block_size"];
  };

  "get_size", {
    default_call with
    args = []; ret = RInt64;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "return the export size";
    longdesc = "\
Returns the size in bytes of the NBD export."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Size of the export"; Link "opt_info"];
    example = Some "examples/get-size.c";
  };

  "get_block_size", {
    default_call with
    args = [Enum ("size_type", block_size_enum)]; ret = RInt64;
    permitted_states = [ Negotiating; Connected; Closed ];
    shortdesc = "return a specific server block size constraint";
    longdesc = "\
Returns a specific size constraint advertised by the server, if any.  If
the return is zero, the server did not advertise a constraint.  C<size_type>
must be one of the following constraints:

=over 4

=item C<LIBNBD_SIZE_MINIMUM> = 0

If non-zero, this will be a power of 2 between 1 and 64k; any client
request that is not aligned in length or offset to this size is likely
to fail with C<EINVAL>.  The image size will generally also be a
multiple of this value (if not, the final few bytes are inaccessible
while obeying alignment constraints).  If zero, it is safest to
assume a minimum block size of 512, although many servers support
a minimum block size of 1.  If the server provides a constraint,
then libnbd defaults to honoring that constraint client-side unless
C<LIBNBD_STRICT_ALIGN> is cleared in C<nbd_set_strict_mode(3)>.

=item C<LIBNBD_SIZE_PREFERRED> = 1

If non-zero, this is a power of 2 representing the preferred size for
efficient I/O.  Smaller requests may incur overhead such as
read-modify-write cycles that will not be present when using I/O that
is a multiple of this value.  This value may be larger than the size
of the export.  If zero, using 4k as a preferred block size tends to
give decent performance.

=item C<LIBNBD_SIZE_MAXIMUM> = 2

If non-zero, this represents the maximum length that the server is
willing to handle during L<nbd_pread(3)> or L<nbd_pwrite(3)>.  Other
functions like L<nbd_zero(3)> may still be able to use larger sizes.
Note that this function returns what the server advertised, but libnbd
itself imposes a maximum of 64M.  If zero, some NBD servers will
abruptly disconnect if a transaction involves more than 32M.

=back

Future NBD extensions may result in additional C<size_type> values.
Note that by default, libnbd requests all available block sizes,
but that a server may differ in what sizes it chooses to report
if L<nbd_set_request_block_size(3)> alters whether the client
requests sizes.
"
^ non_blocking_test_call_description;
    see_also = [Link "get_protocol"; Link "set_request_block_size";
                Link "get_size"; Link "opt_info"]
  };

  "pread", {
    default_call with
    args = [ BytesOut ("buf", "count"); UInt64 "offset" ];
    (* We could silently accept flag DF, but it really only makes sense
     * with callbacks, because otherwise there is no observable change
     * except that the server may fail where it would otherwise succeed.
     *)
    optargs = [ OFlags ("flags", cmd_flags, Some []) ];
    ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "read from the NBD server";
    longdesc = "\
Issue a read command to the NBD server for the range starting
at C<offset> and ending at C<offset> + C<count> - 1.  NBD
can only read all or nothing using this call.  The call
returns when the data has been read fully into C<buf> or there is an
error.  See also L<nbd_pread_structured(3)>, if finer visibility is
required into the server's replies, or if you want to use
C<LIBNBD_CMD_FLAG_DF>.

Note that libnbd currently enforces a maximum read buffer of 64MiB,
even if the server would permit a larger buffer in a single transaction;
attempts to exceed this will result in an C<ERANGE> error.  The server
may enforce a smaller limit, which can be learned with
L<nbd_get_block_size(3)>.

The C<flags> parameter must be C<0> for now (it exists for future NBD
protocol extensions).

Note that if this command fails, and L<nbd_get_pread_initialize(3)>
returns true, then libnbd sanitized C<buf>, but it is unspecified
whether the contents of C<buf> will read as zero or as partial results
from the server.  If L<nbd_get_pread_initialize(3)> returns false,
then libnbd did not sanitize C<buf>, and the contents are undefined
on failure."
^ strict_call_description;
    see_also = [Link "aio_pread"; Link "pread_structured";
                Link "get_block_size"; Link "set_strict_mode";
                Link "set_pread_initialize"];
    example = Some "examples/fetch-first-sector.c";
  };

  "pread_structured", {
    default_call with
    args = [ BytesOut ("buf", "count"); UInt64 "offset";
             Closure chunk_closure ];
    optargs = [ OFlags ("flags", cmd_flags, Some ["DF"]) ];
    ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "read from the NBD server";
    longdesc = "\
Issue a read command to the NBD server for the range starting
at C<offset> and ending at C<offset> + C<count> - 1.  The server's
response may be subdivided into chunks which may arrive out of order
before reassembly into the original buffer; the C<chunk> callback
is used for notification after each chunk arrives, and may perform
additional sanity checking on the server's reply. The callback cannot
call C<nbd_*> APIs on the same handle since it holds the handle lock
and will cause a deadlock.  If the callback returns C<-1>, and no
earlier error has been detected, then the overall read command will
fail with any non-zero value stored into the callback's C<error>
parameter (with a default of C<EPROTO>); but any further chunks will
still invoke the callback.

The C<chunk> function is called once per chunk of data received, with
the C<user_data> passed to this function.  The
C<subbuf> and C<count> parameters represent the subset of the original
buffer which has just been populated by results from the server (in C,
C<subbuf> always points within the original C<buf>; but this guarantee
may not extend to other language bindings). The C<offset> parameter
represents the absolute offset at which C<subbuf> begins within the
image (note that this is not the relative offset of C<subbuf> within
the original buffer C<buf>). Changes to C<error> on output are ignored
unless the callback fails. The input meaning of the C<error> parameter
is controlled by the C<status> parameter, which is one of

=over 4

=item C<LIBNBD_READ_DATA> = 1

C<subbuf> was populated with C<count> bytes of data. On input, C<error>
contains the errno value of any earlier detected error, or zero.

=item C<LIBNBD_READ_HOLE> = 2

C<subbuf> represents a hole, and contains C<count> NUL bytes. On input,
C<error> contains the errno value of any earlier detected error, or zero.

=item C<LIBNBD_READ_ERROR> = 3

C<count> is 0, so C<subbuf> is unusable. On input, C<error> contains the
errno value reported by the server as occurring while reading that
C<offset>, regardless if any earlier error has been detected.

=back

Future NBD extensions may permit other values for C<status>, but those
will not be returned to a client that has not opted in to requesting
such extensions. If the server is non-compliant, it is possible for
the C<chunk> function to be called more times than you expect or with
C<count> 0 for C<LIBNBD_READ_DATA> or C<LIBNBD_READ_HOLE>. It is also
possible that the C<chunk> function is not called at all (in
particular, C<LIBNBD_READ_ERROR> is used only when an error is
associated with a particular offset, and not when the server reports a
generic error), but you are guaranteed that the callback was called at
least once if the overall read succeeds. Libnbd does not validate that
the server obeyed the requirement that a read call must not have
overlapping chunks and must not succeed without enough chunks to cover
the entire request.

Note that libnbd currently enforces a maximum read buffer of 64MiB,
even if the server would permit a larger buffer in a single transaction;
attempts to exceed this will result in an C<ERANGE> error.  The server
may enforce a smaller limit, which can be learned with
L<nbd_get_block_size(3)>.

The C<flags> parameter may be C<0> for no flags, or may contain
C<LIBNBD_CMD_FLAG_DF> meaning that the server should not reply with
more than one fragment (if that is supported - some servers cannot do
this, see L<nbd_can_df(3)>). Libnbd does not validate that the server
actually obeys the flag.

Note that if this command fails, and L<nbd_get_pread_initialize(3)>
returns true, then libnbd sanitized C<buf>, but it is unspecified
whether the contents of C<buf> will read as zero or as partial results
from the server.  If L<nbd_get_pread_initialize(3)> returns false,
then libnbd did not sanitize C<buf>, and the contents are undefined
on failure."
^ strict_call_description;
    see_also = [Link "can_df"; Link "pread";
                Link "aio_pread_structured"; Link "get_block_size";
                Link "set_strict_mode"; Link "set_pread_initialize";
                Link "set_request_block_size"];
  };

  "pwrite", {
    default_call with
    args = [ BytesIn ("buf", "count"); UInt64 "offset" ];
    optargs = [ OFlags ("flags", cmd_flags, Some ["FUA"]) ];
    ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "write to the NBD server";
    longdesc = "\
Issue a write command to the NBD server, writing the data in
C<buf> to the range starting at C<offset> and ending at
C<offset> + C<count> - 1.  NBD can only write all or nothing
using this call.  The call returns when the command has been
acknowledged by the server, or there is an error.  Note this will
generally return an error if L<nbd_is_read_only(3)> is true.

Note that libnbd currently enforces a maximum write buffer of 64MiB,
even if the server would permit a larger buffer in a single transaction;
attempts to exceed this will result in an C<ERANGE> error.  The server
may enforce a smaller limit, which can be learned with
L<nbd_get_block_size(3)>.

The C<flags> parameter may be C<0> for no flags, or may contain
C<LIBNBD_CMD_FLAG_FUA> meaning that the server should not
return until the data has been committed to permanent storage
(if that is supported - some servers cannot do this, see
L<nbd_can_fua(3)>)."
^ strict_call_description;
    see_also = [Link "can_fua"; Link "is_read_only";
                Link "aio_pwrite"; Link "get_block_size";
                Link "set_strict_mode"];
    example = Some "examples/reads-and-writes.c";
  };

  "shutdown", {
    default_call with
    args = []; optargs = [ OFlags ("flags", shutdown_flags, None) ];
    ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "disconnect from the NBD server";
    longdesc = "\
Issue the disconnect command to the NBD server.  This is
a nice way to tell the server we are going away, but from the
client's point of view has no advantage over abruptly closing
the connection (see L<nbd_close(3)>).

This function works whether or not the handle is ready for
transmission of commands. If more fine-grained control is
needed, see L<nbd_aio_disconnect(3)>.

The C<flags> argument is a bitmask, including zero or more of the
following shutdown flags:

=over 4

=item C<LIBNBD_SHUTDOWN_ABANDON_PENDING> = 0x10000

If there are any pending requests which have not yet been sent to
the server (see L<nbd_aio_in_flight(3)>), abandon them without
sending them to the server, rather than the usual practice of
issuing those commands before informing the server of the intent
to disconnect.

=back

For convenience, the constant C<LIBNBD_SHUTDOWN_MASK> is available
to describe all shutdown flags recognized by this build of libnbd.
A future version of the library may add new flags.";
    see_also = [Link "close"; Link "aio_disconnect"];
    example = Some "examples/reads-and-writes.c";
  };

  "flush", {
    default_call with
    args = []; optargs = [ OFlags ("flags", cmd_flags, Some []) ]; ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "send flush command to the NBD server";
    longdesc = "\
Issue the flush command to the NBD server.  The function should
return when all write commands which have completed have been
committed to permanent storage on the server.  Note this will
generally return an error if L<nbd_can_flush(3)> is false.

The C<flags> parameter must be C<0> for now (it exists for future NBD
protocol extensions)."
^ strict_call_description;
    see_also = [Link "can_flush"; Link "aio_flush";
                Link "set_strict_mode"];
  };

  "trim", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset" ];
    optargs = [ OFlags ("flags", cmd_flags, Some ["FUA"]) ];
    ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "send trim command to the NBD server";
    longdesc = "\
Issue a trim command to the NBD server, which if supported
by the server causes a hole to be punched in the backing
store starting at C<offset> and ending at C<offset> + C<count> - 1.
The call returns when the command has been acknowledged by the server,
or there is an error.  Note this will generally return an error
if L<nbd_can_trim(3)> is false or L<nbd_is_read_only(3)> is true.

Note that not all servers can support a C<count> of 4GiB or larger.
The NBD protocol does not yet have a way for a client to learn if
the server will enforce an even smaller maximum trim size, although
a future extension may add a constraint visible in
L<nbd_get_block_size(3)>.

The C<flags> parameter may be C<0> for no flags, or may contain
C<LIBNBD_CMD_FLAG_FUA> meaning that the server should not
return until the data has been committed to permanent storage
(if that is supported - some servers cannot do this, see
L<nbd_can_fua(3)>)."
^ strict_call_description;
    see_also = [Link "can_fua"; Link "can_trim"; Link "is_read_only";
                Link "aio_trim"; Link "set_strict_mode"];
  };

  "cache", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset" ];
    optargs = [ OFlags ("flags", cmd_flags, Some []) ];
    ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "send cache (prefetch) command to the NBD server";
    longdesc = "\
Issue the cache (prefetch) command to the NBD server, which
if supported by the server causes data to be prefetched
into faster storage by the server, speeding up a subsequent
L<nbd_pread(3)> call.  The server can also silently ignore
this command.  Note this will generally return an error if
L<nbd_can_cache(3)> is false.

Note that not all servers can support a C<count> of 4GiB or larger.
The NBD protocol does not yet have a way for a client to learn if
the server will enforce an even smaller maximum cache size, although
a future extension may add a constraint visible in
L<nbd_get_block_size(3)>.

The C<flags> parameter must be C<0> for now (it exists for future NBD
protocol extensions)."
^ strict_call_description;
    see_also = [Link "can_cache"; Link "aio_cache";
                Link "set_strict_mode"];
  };

  "zero", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset" ];
    optargs = [ OFlags ("flags", cmd_flags,
                        Some ["FUA"; "NO_HOLE"; "FAST_ZERO"]) ];
    ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "send write zeroes command to the NBD server";
    longdesc = "\
Issue a write zeroes command to the NBD server, which if supported
by the server causes a zeroes to be written efficiently
starting at C<offset> and ending at C<offset> + C<count> - 1.
The call returns when the command has been acknowledged by the server,
or there is an error.  Note this will generally return an error if
L<nbd_can_zero(3)> is false or L<nbd_is_read_only(3)> is true.

Note that not all servers can support a C<count> of 4GiB or larger.
The NBD protocol does not yet have a way for a client to learn if
the server will enforce an even smaller maximum zero size, although
a future extension may add a constraint visible in
L<nbd_get_block_size(3)>.  Also, some servers may permit a larger
zero request only when the C<LIBNBD_CMD_FLAG_FAST_ZERO> is in use.

The C<flags> parameter may be C<0> for no flags, or may contain
C<LIBNBD_CMD_FLAG_FUA> meaning that the server should not
return until the data has been committed to permanent storage
(if that is supported - some servers cannot do this, see
L<nbd_can_fua(3)>), C<LIBNBD_CMD_FLAG_NO_HOLE> meaning that
the server should favor writing actual allocated zeroes over
punching a hole, and/or C<LIBNBD_CMD_FLAG_FAST_ZERO> meaning
that the server must fail quickly if writing zeroes is no
faster than a normal write (if that is supported - some servers
cannot do this, see L<nbd_can_fast_zero(3)>)."
^ strict_call_description;
    see_also = [Link "can_fua"; Link "can_zero"; Link "is_read_only";
                Link "can_fast_zero"; Link "aio_zero";
                Link "set_strict_mode"];
  };

  "block_status", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset"; Closure extent_closure ];
    optargs = [ OFlags ("flags", cmd_flags, Some ["REQ_ONE"]) ];
    ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "send block status command to the NBD server";
    longdesc = "\
Issue the block status command to the NBD server.  If
supported by the server, this causes metadata context
information about blocks beginning from the specified
offset to be returned. The C<count> parameter is a hint: the
server may choose to return less status, or the final block
may extend beyond the requested range. If multiple contexts
are supported, the number of blocks and cumulative length
of those blocks need not be identical between contexts.

Note that not all servers can support a C<count> of 4GiB or larger.
The NBD protocol does not yet have a way for a client to learn if
the server will enforce an even smaller maximum block status size,
although a future extension may add a constraint visible in
L<nbd_get_block_size(3)>.

Depending on which metadata contexts were enabled before
connecting (see L<nbd_add_meta_context(3)>) and which are
supported by the server (see L<nbd_can_meta_context(3)>) this call
returns information about extents by calling back to the
C<extent> function.  The callback cannot call C<nbd_*> APIs on the
same handle since it holds the handle lock and will
cause a deadlock.  If the callback returns C<-1>, and no earlier
error has been detected, then the overall block status command
will fail with any non-zero value stored into the callback's
C<error> parameter (with a default of C<EPROTO>); but any further
contexts will still invoke the callback.

The C<extent> function is called once per type of metadata available,
with the C<user_data> passed to this function.  The C<metacontext>
parameter is a string such as C<\"base:allocation\">.  The C<entries>
array is an array of pairs of integers with the first entry in each
pair being the length (in bytes) of the block and the second entry
being a status/flags field which is specific to the metadata context.
(The number of pairs passed to the function is C<nr_entries/2>.)  The
NBD protocol document in the section about
C<NBD_REPLY_TYPE_BLOCK_STATUS> describes the meaning of this array;
for contexts known to libnbd, B<E<lt>libnbd.hE<gt>> contains constants
beginning with C<LIBNBD_STATE_> that may help decipher the values.
On entry to the callback, the C<error> parameter contains the errno
value of any previously detected error, but even if an earlier error
was detected, the current C<metacontext> and C<entries> are valid.

It is possible for the extent function to be called
more times than you expect (if the server is buggy),
so always check the C<metacontext> field to ensure you
are receiving the data you expect.  It is also possible
that the extent function is not called at all, even for
metadata contexts that you requested.  This indicates
either that the server doesn't support the context
or for some other reason cannot return the data.

The C<flags> parameter may be C<0> for no flags, or may contain
C<LIBNBD_CMD_FLAG_REQ_ONE> meaning that the server should
return only one extent per metadata context where that extent
does not exceed C<count> bytes; however, libnbd does not
validate that the server obeyed the flag."
^ strict_call_description;
    see_also = [Link "add_meta_context"; Link "can_meta_context";
                Link "aio_block_status"; Link "set_strict_mode"];
  };

  "poll", {
    default_call with
    args = [ Int "timeout" ]; ret = RInt;
    shortdesc = "poll the handle once";
    longdesc = "\
This is a simple implementation of L<poll(2)> which is used
internally by synchronous API calls.  On success, it returns
C<0> if the C<timeout> (in milliseconds) occurs, or C<1> if
the poll completed and the state machine progressed. Set
C<timeout> to C<-1> to block indefinitely (but be careful
that eventual action is actually expected - for example, if
the connection is established but there are no commands in
flight, using an infinite timeout will permanently block).

This function is mainly useful as an example of how you might
integrate libnbd with your own main loop, rather than being
intended as something you would use.";
    example = Some "examples/aio-connect-read.c";
    see_also = [ Link "poll2" ];
  };

  "poll2", {
    default_call with
    args = [Fd "fd"; Int "timeout" ]; ret = RInt;
    shortdesc = "poll the handle once, with fd";
    longdesc = "\
This is the same as L<nbd_poll(3)>, but an additional
file descriptor parameter is passed.  The additional
fd is also polled (using C<POLLIN>).  One use for this is to
wait for an L<eventfd(2)>.";
    see_also = [ Link "poll" ];
  };

  "aio_connect", {
    default_call with
    args = [ SockAddrAndLen ("addr", "addrlen") ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to the NBD server";
    longdesc = "\
Begin connecting to the NBD server.  The C<addr> and C<addrlen>
parameters specify the address of the socket to connect to.
" ^ async_connect_call_description;
    see_also = [ Link "set_opt_mode"; ];
  };

  "aio_connect_uri", {
    default_call with
    args = [ String "uri" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to an NBD URI";
    longdesc = "\
Begin connecting to the NBD URI C<uri>.  Parameters behave as
documented in L<nbd_connect_uri(3)>.
" ^ async_connect_call_description;
    see_also = [ Link "connect_uri"; Link "set_opt_mode";
                 URLLink "https://github.com/NetworkBlockDevice/nbd/blob/master/doc/uri.md"]
  };

  "aio_connect_unix", {
    default_call with
    args = [ Path "unixsocket" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to the NBD server over a Unix domain socket";
    longdesc = "\
Begin connecting to the NBD server over Unix domain socket
(C<unixsocket>).  Parameters behave as documented in
L<nbd_connect_unix(3)>.
" ^ async_connect_call_description;
    example = Some "examples/aio-connect-read.c";
    see_also = [ Link "connect_unix"; Link "set_opt_mode" ];
  };

  "aio_connect_vsock", {
    default_call with
    args = [ UInt32 "cid"; UInt32 "port" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to the NBD server over AF_VSOCK socket";
    longdesc = "\
Begin connecting to the NBD server over the C<AF_VSOCK>
protocol to the server C<cid:port>.  Parameters behave as documented in
L<nbd_connect_vsock(3)>.
" ^ async_connect_call_description;
    see_also = [ Link "connect_vsock"; Link "set_opt_mode" ];
  };

  "aio_connect_tcp", {
    default_call with
    args = [ String "hostname"; String "port" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to the NBD server over a TCP port";
    longdesc = "\
Begin connecting to the NBD server listening on C<hostname:port>.
Parameters behave as documented in L<nbd_connect_tcp(3)>.
" ^ async_connect_call_description;
    see_also = [ Link "connect_tcp"; Link "set_opt_mode" ];
  };

  "aio_connect_socket", {
    default_call with
    args = [ Fd "sock" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect directly to a connected socket";
    longdesc = "\
Begin connecting to the connected socket C<fd>.
Parameters behave as documented in L<nbd_connect_socket(3)>.
" ^ async_connect_call_description;
    see_also = [ Link "connect_socket"; Link "set_opt_mode" ];
  };

  "aio_connect_command", {
    default_call with
    args = [ StringList "argv" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to the NBD server";
    longdesc = "\
Run the command as a subprocess and begin connecting to it over
stdin/stdout.  Parameters behave as documented in
L<nbd_connect_command(3)>.
" ^ async_connect_call_description;
    see_also = [ Link "connect_command"; Link "set_opt_mode" ];
  };

  "aio_connect_systemd_socket_activation", {
    default_call with
    args = [ StringList "argv" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect using systemd socket activation";
    longdesc = "\
Run the command as a subprocess and begin connecting to it using
systemd socket activation.  Parameters behave as documented in
L<nbd_connect_systemd_socket_activation(3)>.
" ^ async_connect_call_description;
    see_also = [ Link "connect_systemd_socket_activation";
                 Link "set_opt_mode" ];
  };

  "aio_opt_go", {
    default_call with
    args = [];
    optargs = [ OClosure completion_closure ];
    ret = RErr;
    permitted_states = [ Negotiating ];
    shortdesc = "end negotiation and move on to using an export";
    longdesc = "\
Request that the server finish negotiation and move on to serving the
export previously specified by the most recent L<nbd_set_export_name(3)>
or L<nbd_connect_uri(3)>.  This can only be used if
L<nbd_set_opt_mode(3)> enabled option mode.

To determine when the request completes, wait for
L<nbd_aio_is_connecting(3)> to return false.  Or supply the optional
C<completion_callback> which will be invoked as described in
L<libnbd(3)/Completion callbacks>, except that it is automatically
retired regardless of return value.  Note that directly detecting
whether the server returns an error (as is done by the return value
of the synchronous counterpart) is only possible with a completion
callback; however it is also possible to indirectly detect an error
when L<nbd_aio_is_negotiating(3)> returns true.";
    see_also = [Link "set_opt_mode"; Link "opt_go"];
  };

  "aio_opt_abort", {
    default_call with
    args = []; ret = RErr;
    permitted_states = [ Negotiating ];
    shortdesc = "end negotiation and close the connection";
    longdesc = "\
Request that the server finish negotiation, gracefully if possible, then
close the connection.  This can only be used if L<nbd_set_opt_mode(3)>
enabled option mode.

To determine when the request completes, wait for
L<nbd_aio_is_connecting(3)> to return false.";
    see_also = [Link "set_opt_mode"; Link "opt_abort"];
  };

  "aio_opt_list", {
    default_call with
    args = [ Closure list_closure ];
    optargs = [ OClosure completion_closure ];
    ret = RErr;
    permitted_states = [ Negotiating ];
    shortdesc = "request the server to list all exports during negotiation";
    longdesc = "\
Request that the server list all exports that it supports.  This can
only be used if L<nbd_set_opt_mode(3)> enabled option mode.

To determine when the request completes, wait for
L<nbd_aio_is_connecting(3)> to return false.  Or supply the optional
C<completion_callback> which will be invoked as described in
L<libnbd(3)/Completion callbacks>, except that it is automatically
retired regardless of return value.  Note that detecting whether the
server returns an error (as is done by the return value of the
synchronous counterpart) is only possible with a completion
callback.";
    see_also = [Link "set_opt_mode"; Link "opt_list"];
  };

  "aio_opt_info", {
    default_call with
    args = [];
    optargs = [ OClosure completion_closure ];
    ret = RErr;
    permitted_states = [ Negotiating ];
    shortdesc = "request the server for information about an export";
    longdesc = "\
Request that the server supply information about the export name
previously specified by the most recent L<nbd_set_export_name(3)>
or L<nbd_connect_uri(3)>.  This can only be used if
L<nbd_set_opt_mode(3)> enabled option mode.

To determine when the request completes, wait for
L<nbd_aio_is_connecting(3)> to return false.  Or supply the optional
C<completion_callback> which will be invoked as described in
L<libnbd(3)/Completion callbacks>, except that it is automatically
retired regardless of return value.  Note that detecting whether the
server returns an error (as is done by the return value of the
synchronous counterpart) is only possible with a completion
callback.";
    see_also = [Link "set_opt_mode"; Link "opt_info"; Link "is_read_only"];
  };

  "aio_opt_list_meta_context", {
    default_call with
    args = [ Closure context_closure ]; ret = RInt;
    optargs = [ OClosure completion_closure ];
    permitted_states = [ Negotiating ];
    shortdesc = "request list of available meta contexts, using implicit query";
    longdesc = "\
Request that the server list available meta contexts associated with
the export previously specified by the most recent
L<nbd_set_export_name(3)> or L<nbd_connect_uri(3)>, and with a
list of queries from prior calls to L<nbd_add_meta_context(3)>
(see L<nbd_aio_opt_list_meta_context_queries(3)> if you want to
supply an explicit query list instead).  This can only be
used if L<nbd_set_opt_mode(3)> enabled option mode.

To determine when the request completes, wait for
L<nbd_aio_is_connecting(3)> to return false.  Or supply the optional
C<completion_callback> which will be invoked as described in
L<libnbd(3)/Completion callbacks>, except that it is automatically
retired regardless of return value.  Note that detecting whether the
server returns an error (as is done by the return value of the
synchronous counterpart) is only possible with a completion
callback.";
    see_also = [Link "set_opt_mode"; Link "opt_list_meta_context";
                Link "aio_opt_list_meta_context_queries"];
  };

  "aio_opt_list_meta_context_queries", {
    default_call with
    args = [ StringList "queries"; Closure context_closure ]; ret = RInt;
    optargs = [ OClosure completion_closure ];
    permitted_states = [ Negotiating ];
    shortdesc = "request list of available meta contexts, using explicit query";
    longdesc = "\
Request that the server list available meta contexts associated with
the export previously specified by the most recent
L<nbd_set_export_name(3)> or L<nbd_connect_uri(3)>, and with an
explicit list of queries provided as a parameter (see
L<nbd_aio_opt_list_meta_context(3)> if you want to reuse an
implicit query list instead).  This can only be
used if L<nbd_set_opt_mode(3)> enabled option mode.

To determine when the request completes, wait for
L<nbd_aio_is_connecting(3)> to return false.  Or supply the optional
C<completion_callback> which will be invoked as described in
L<libnbd(3)/Completion callbacks>, except that it is automatically
retired regardless of return value.  Note that detecting whether the
server returns an error (as is done by the return value of the
synchronous counterpart) is only possible with a completion
callback.";
    see_also = [Link "set_opt_mode"; Link "opt_list_meta_context_queries";
                Link "aio_opt_list_meta_context"];
  };

  "aio_pread", {
    default_call with
    args = [ BytesPersistOut ("buf", "count"); UInt64 "offset" ];
    optargs = [ OClosure completion_closure;
                OFlags ("flags", cmd_flags, Some []) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "read from the NBD server";
    longdesc = "\
Issue a read command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Note that you must ensure C<buf> is valid until the command has
completed.  Furthermore, if the C<error> parameter to
C<completion_callback> is set or if L<nbd_aio_command_completed(3)>
reports failure, and if L<nbd_get_pread_initialize(3)> returns true,
then libnbd sanitized C<buf>, but it is unspecified whether the
contents of C<buf> will read as zero or as partial results from the
server.  If L<nbd_get_pread_initialize(3)> returns false, then
libnbd did not sanitize C<buf>, and the contents are undefined
on failure.

Other parameters behave as documented in L<nbd_pread(3)>."
^ strict_call_description;
    example = Some "examples/aio-connect-read.c";
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "aio_pread_structured"; Link "pread";
                Link "set_strict_mode"; Link "set_pread_initialize"];
  };

  "aio_pread_structured", {
    default_call with
    args = [ BytesPersistOut ("buf", "count"); UInt64 "offset";
             Closure chunk_closure ];
    optargs = [ OClosure completion_closure;
                OFlags ("flags", cmd_flags, Some ["DF"]) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "read from the NBD server";
    longdesc = "\
Issue a read command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Note that you must ensure C<buf> is valid until the command has
completed.  Furthermore, if the C<error> parameter to
C<completion_callback> is set or if L<nbd_aio_command_completed(3)>
reports failure, and if L<nbd_get_pread_initialize(3)> returns true,
then libnbd sanitized C<buf>, but it is unspecified whether the
contents of C<buf> will read as zero or as partial results from the
server.  If L<nbd_get_pread_initialize(3)> returns false, then
libnbd did not sanitize C<buf>, and the contents are undefined
on failure.

Other parameters behave as documented in L<nbd_pread_structured(3)>."
^ strict_call_description;
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "aio_pread"; Link "pread_structured";
                Link "set_strict_mode"; Link "set_pread_initialize"];
  };

  "aio_pwrite", {
    default_call with
    args = [ BytesPersistIn ("buf", "count"); UInt64 "offset" ];
    optargs = [ OClosure completion_closure;
                OFlags ("flags", cmd_flags, Some ["FUA"]) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "write to the NBD server";
    longdesc = "\
Issue a write command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Note that you must ensure C<buf> is valid until the command has
completed.  Other parameters behave as documented in L<nbd_pwrite(3)>."
^ strict_call_description;
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "is_read_only"; Link "pwrite"; Link "set_strict_mode"];
  };

  "aio_disconnect", {
    default_call with
    args = []; optargs = [ OFlags ("flags", cmd_flags, Some []) ]; ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "disconnect from the NBD server";
    longdesc = "\
Issue the disconnect command to the NBD server.  This is
not a normal command because NBD servers are not obliged
to send a reply.  Instead you should wait for
L<nbd_aio_is_closed(3)> to become true on the connection.  Once this
command is issued, you cannot issue any further commands.

Although libnbd does not prevent you from issuing this command while
still waiting on the replies to previous commands, the NBD protocol
recommends that you wait until there are no other commands in flight
(see L<nbd_aio_in_flight(3)>), to give the server a better chance at a
clean shutdown.

The C<flags> parameter must be C<0> for now (it exists for future NBD
protocol extensions).  There is no direct synchronous counterpart;
however, L<nbd_shutdown(3)> will call this function if appropriate.";
    see_also = [Link "aio_in_flight"];
  };

  "aio_flush", {
    default_call with
    args = [];
    optargs = [ OClosure completion_closure;
                OFlags ("flags", cmd_flags, Some []) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "send flush command to the NBD server";
    longdesc = "\
Issue the flush command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Other parameters behave as documented in L<nbd_flush(3)>."
^ strict_call_description;
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "can_flush"; Link "flush"; Link "set_strict_mode"];
  };

  "aio_trim", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset" ];
    optargs = [ OClosure completion_closure;
                OFlags ("flags", cmd_flags, Some ["FUA"]) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "send trim command to the NBD server";
    longdesc = "\
Issue a trim command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Other parameters behave as documented in L<nbd_trim(3)>."
^ strict_call_description;
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "can_trim"; Link "trim"; Link "set_strict_mode"];
  };

  "aio_cache", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset" ];
    optargs = [ OClosure completion_closure;
                OFlags ("flags", cmd_flags, Some []) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "send cache (prefetch) command to the NBD server";
    longdesc = "\
Issue the cache (prefetch) command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Other parameters behave as documented in L<nbd_cache(3)>."
^ strict_call_description;
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "can_cache"; Link "cache"; Link "set_strict_mode"];
  };

  "aio_zero", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset" ];
    optargs = [ OClosure completion_closure;
                OFlags ("flags", cmd_flags,
                        Some ["FUA"; "NO_HOLE"; "FAST_ZERO"]) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "send write zeroes command to the NBD server";
    longdesc = "\
Issue a write zeroes command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Other parameters behave as documented in L<nbd_zero(3)>."
^ strict_call_description;
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "can_zero"; Link "can_fast_zero";
                Link "zero"; Link "set_strict_mode"];
  };

  "aio_block_status", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset"; Closure extent_closure ];
    optargs = [ OClosure completion_closure;
                OFlags ("flags", cmd_flags, Some ["REQ_ONE"]) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "send block status command to the NBD server";
    longdesc = "\
Send the block status command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Other parameters behave as documented in L<nbd_block_status(3)>."
^ strict_call_description;
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "can_meta_context"; Link "block_status";
                Link "set_strict_mode"];
  };

  "aio_get_fd", {
    default_call with
    args = []; ret = RFd;
    shortdesc = "return file descriptor associated with this connection";
    longdesc = "\
Return the underlying file descriptor associated with this
connection.  You can use this to check if the file descriptor
is ready for reading or writing and call L<nbd_aio_notify_read(3)>
or L<nbd_aio_notify_write(3)>.  See also L<nbd_aio_get_direction(3)>.
Do not do anything else with the file descriptor.";
    see_also = [Link "aio_get_direction"];
  };

  "aio_get_direction", {
    default_call with
    args = []; ret = RUInt; is_locked = false; may_set_error = false;
    shortdesc = "return the read or write direction";
    longdesc = "\
Return the current direction of this connection, which means
whether we are next expecting to read data from the server, write
data to the server, or both.  It returns

=over 4

=item 0

We are not expected to interact with the server file descriptor from
the current state. It is not worth attempting to use L<poll(2)>; if
the connection is not dead, then state machine progress must instead
come from some other means such as L<nbd_aio_connect(3)>.

=item C<LIBNBD_AIO_DIRECTION_READ> = 1

We are expected next to read from the server.  If using L<poll(2)>
you would set C<events = POLLIN>.  If C<revents> returns C<POLLIN>
or C<POLLHUP> you would then call L<nbd_aio_notify_read(3)>.

Note that once libnbd reaches L<nbd_aio_is_ready(3)>, this direction is
returned even when there are no commands in flight (see
L<nbd_aio_in_flight(3)>). In a single-threaded use of libnbd, it is not
worth polling until after issuing a command, as otherwise the server
will never wake up the poll. In a multi-threaded scenario, you can
have one thread begin a polling loop prior to any commands, but any
other thread that issues a command will need a way to kick the
polling thread out of poll in case issuing the command changes the
needed polling direction. Possible ways to do this include polling
for activity on a pipe-to-self, or using L<pthread_kill(3)> to send
a signal that is masked except during L<ppoll(2)>.

=item C<LIBNBD_AIO_DIRECTION_WRITE> = 2

We are expected next to write to the server.  If using L<poll(2)>
you would set C<events = POLLOUT>.  If C<revents> returns C<POLLOUT>
you would then call L<nbd_aio_notify_write(3)>.

=item C<LIBNBD_AIO_DIRECTION_BOTH> = 3

We are expected next to either read or write to the server.  If using
L<poll(2)> you would set C<events = POLLIN|POLLOUT>.  If only one of
C<POLLIN> or C<POLLOUT> is returned, then see above.  However, if both
are returned, it is better to call only L<nbd_aio_notify_read(3)>, as
processing the server's reply may change the state of the connection
and invalidate the need to write more commands.

=back";
    see_also = [Link "aio_in_flight"];
  };

  "aio_notify_read", {
    default_call with
    args = []; ret = RErr;
    shortdesc = "notify that the connection is readable";
    longdesc = "\
Send notification to the state machine that the connection
is readable.  Typically this is called after your main loop
has detected that the file descriptor associated with this
connection is readable.";
  };

  "aio_notify_write", {
    default_call with
    args = []; ret = RErr;
    shortdesc = "notify that the connection is writable";
    longdesc = "\
Send notification to the state machine that the connection
is writable.  Typically this is called after your main loop
has detected that the file descriptor associated with this
connection is writable.";
  };

  "aio_is_created", {
    default_call with
    args = []; ret = RBool; is_locked = false; may_set_error = false;
    shortdesc = "check if the connection has just been created";
    longdesc = "\
Return true if this connection has just been created.  This
is the state before the handle has started connecting to a
server.  In this state the handle can start to be connected
by calling functions such as L<nbd_aio_connect(3)>.";
  };

  "aio_is_connecting", {
    default_call with
    args = []; ret = RBool; is_locked = false; may_set_error = false;
    shortdesc = "check if the connection is connecting or handshaking";
    longdesc = "\
Return true if this connection is connecting to the server
or in the process of handshaking and negotiating options
which happens before the handle becomes ready to
issue commands (see L<nbd_aio_is_ready(3)>).";
    see_also = [Link "aio_is_ready"];
  };

  "aio_is_negotiating", {
    default_call with
    args = []; ret = RBool; is_locked = false; may_set_error = false;
    shortdesc = "check if connection is ready to send handshake option";
    longdesc = "\
Return true if this connection is ready to start another option
negotiation command while handshaking with the server.  An option
command will move back to the connecting state (see
L<nbd_aio_is_connecting(3)>).  Note that this state cannot be
reached unless requested by L<nbd_set_opt_mode(3)>, and even then
it only works with newstyle servers; an oldstyle server will skip
straight to L<nbd_aio_is_ready(3)>.";
    see_also = [Link "aio_is_connecting"; Link "aio_is_ready";
                Link "set_opt_mode"];
  };

  "aio_is_ready", {
    default_call with
    args = []; ret = RBool; is_locked = false; may_set_error = false;
    shortdesc = "check if the connection is in the ready state";
    longdesc = "\
Return true if this connection is connected to the NBD server,
the handshake has completed, and the connection is idle or
waiting for a reply.  In this state the handle is ready to
issue commands.";
  };

  "aio_is_processing", {
    default_call with
    args = []; ret = RBool; is_locked = false; may_set_error = false;
    shortdesc = "check if the connection is processing a command";
    longdesc = "\
Return true if this connection is connected to the NBD server,
the handshake has completed, and the connection is processing
commands (either writing out a request or reading a reply).

Note the ready state (L<nbd_aio_is_ready(3)>) is not included.
In the ready state commands may be I<in flight> (the I<server>
is processing them), but libnbd is not processing them.";
  };

  "aio_is_dead", {
    default_call with
    args = []; ret = RBool; is_locked = false; may_set_error = false;
    shortdesc = "check if the connection is dead";
    longdesc = "\
Return true if the connection has encountered a fatal
error and is dead.  In this state the handle may only be closed.
There is no way to recover a handle from the dead state.";
  };

  "aio_is_closed", {
    default_call with
    args = []; ret = RBool; is_locked = false; may_set_error = false;
    shortdesc = "check if the connection is closed";
    longdesc = "\
Return true if the connection has closed.  There is no way to
reconnect a closed connection.  Instead you must close the
whole handle.";
  };

  "aio_command_completed", {
    default_call with
    args = [UInt64 "cookie"]; ret = RBool;
    shortdesc = "check if the command completed";
    longdesc = "\
Return true if the command completed.  If this function returns
true then the command was successful and it has been retired.
Return false if the command is still in flight.  This can also
fail with an error in case the command failed (in this case
the command is also retired).  A command is retired either via
this command, or by using a completion callback which returns C<1>.

The C<cookie> parameter is the positive unique 64 bit cookie
for the command, as returned by a call such as L<nbd_aio_pread(3)>.";
  };

  "aio_peek_command_completed", {
    default_call with
    args = []; ret = RInt64;
    shortdesc = "check if any command has completed";
    longdesc = "\
Return the unique positive 64 bit cookie of the first non-retired but
completed command, C<0> if there are in-flight commands but none of
them are awaiting retirement, or C<-1> on error including when there
are no in-flight commands. Any cookie returned by this function must
still be passed to L<nbd_aio_command_completed(3)> to actually retire
the command and learn whether the command was successful.";
  };

  "aio_in_flight", {
    default_call with
    args = []; ret = RInt;
    permitted_states = [ Connected; Closed; Dead ];
    (* XXX is_locked = false ? *)
    shortdesc = "check how many aio commands are still in flight";
    longdesc = "\
Return the number of in-flight aio commands that are still awaiting a
response from the server before they can be retired.  If this returns
a non-zero value when requesting a disconnect from the server (see
L<nbd_aio_disconnect(3)> and L<nbd_shutdown(3)>), libnbd does not try to
wait for those commands to complete gracefully; if the server strands
commands while shutting down, L<nbd_aio_command_completed(3)> will report
those commands as failed with a status of C<ENOTCONN>.";
    example = Some "examples/aio-connect-read.c";
    see_also = [Link "aio_disconnect"];
  };

  "connection_state", {
    default_call with
    args = []; ret = RStaticString;
    shortdesc = "return string describing the state of the connection";
    longdesc = "\
Returns a descriptive string for the state of the connection.  This
can be used for debugging or troubleshooting, but you should not
rely on the state of connections since it may change in future
versions.";
  };

  "get_package_name", {
    default_call with
    args = []; ret = RStaticString; is_locked = false; may_set_error = false;
    shortdesc = "return the name of the library";
    longdesc = "\
Returns the name of the library, always C<\"libnbd\"> unless
the library was modified with another name at compile time.";
    see_also = [Link "get_version"];
  };

  "get_version", {
    default_call with
    args = []; ret = RStaticString; is_locked = false; may_set_error = false;
    shortdesc = "return the version of the library";
    longdesc = "\
Return the version of libnbd.  This is returned as a string
in the form C<\"major.minor.release\"> where each of major, minor
and release is a small positive integer.  For example:

     minor
       ↓
    \"1.0.3\"
     ↑   ↑
 major   release

=over 4

=item major = 0

The major number was C<0> for the early experimental versions of
libnbd where we still had an unstable API.

=item major = 1

The major number is C<1> for the versions of libnbd with a
long-term stable API and ABI.  It is not anticipated that
major will be any number other than C<1>.

=item minor = 0, 2, ... (even)

The minor number is even for stable releases.

=item minor = 1, 3, ... (odd)

The minor number is odd for development versions.  Note that
new APIs added in a development version remain experimental
and subject to change in that branch until they appear in a stable
release.

=item release

The release number is incremented for each release along a particular
branch.

=back";
    see_also = [Link "get_package_name"];
  };

  "kill_subprocess", {
    default_call with
    args = [ Int "signum" ]; ret = RErr;
    shortdesc = "kill server running as a subprocess";
    longdesc = "\
This call may be used to kill the server running as a subprocess
that was previously created using L<nbd_connect_command(3)>.  You
do not need to use this call.  It is only needed if the server
does not exit when the socket is closed.

The C<signum> parameter is the optional signal number to send
(see L<signal(7)>).  If C<signum> is C<0> then C<SIGTERM> is sent.";
    see_also = [ExternalLink ("signal", 7); Link "connect_command"];
  };

  "supports_tls", {
    default_call with
    args = []; ret = RBool; is_locked = false; may_set_error = false;
    shortdesc = "true if libnbd was compiled with support for TLS";
    longdesc = "\
Returns true if libnbd was compiled with gnutls which is required
to support TLS encryption, or false if not.";
    see_also = [Link "set_tls"];
  };

  "supports_vsock", {
    default_call with
    args = []; ret = RBool; is_locked = false; may_set_error = false;
    shortdesc = "true if libnbd was compiled with support for AF_VSOCK";
    longdesc = "\
Returns true if libnbd was compiled with support for the C<AF_VSOCK>
family of sockets, or false if not.

Note that on the Linux operating system, this returns true if
there is compile-time support, but you may still need runtime
support for some aspects of AF_VSOCK usage; for example, use of
C<VMADDR_CID_LOCAL> as the server name requires that the
I<vsock_loopback> kernel module is loaded.";
    see_also = [Link "connect_vsock"; Link "connect_uri"];
  };

  "supports_uri", {
    default_call with
    args = []; ret = RBool; is_locked = false; may_set_error = false;
    shortdesc = "true if libnbd was compiled with support for NBD URIs";
    longdesc = "\
Returns true if libnbd was compiled with libxml2 which is required
to support NBD URIs, or false if not.";
    see_also = [Link "connect_uri"; Link "aio_connect_uri";
                Link "get_uri"];
  };

  "get_uri", {
    default_call with
    args = []; ret = RString;
    permitted_states = [ Connecting; Negotiating; Connected; Closed; Dead ];
    shortdesc = "construct an NBD URI for a connection";
    longdesc = "\
This makes a best effort attempt to construct an NBD URI which
could be used to connect back to the same server (using
L<nbd_connect_uri(3)>).

In some cases there is not enough information in the handle
to successfully create a URI (eg. if you connected with
L<nbd_connect_socket(3)>).  In such cases the call returns
C<NULL> and further diagnostic information is available
via L<nbd_get_errno(3)> and L<nbd_get_error(3)> as usual.

Even if a URI is returned it is not guaranteed to work, and
it may not be optimal.";
    see_also = [Link "connect_uri"; Link "aio_connect_uri";
                Link "supports_uri"];
  };
]

(* The first stable version that the symbol appeared in, for
 * example (1, 2) if the symbol was added in development cycle
 * 1.1.x and thus the first stable version was 1.2.
 *)
let first_version = [
  "set_debug", (1, 0);
  "get_debug", (1, 0);
  "set_debug_callback", (1, 0);
  "clear_debug_callback", (1, 0);
  "set_handle_name", (1, 0);
  "get_handle_name", (1, 0);
  "set_export_name", (1, 0);
  "get_export_name", (1, 0);
  "set_tls", (1, 0);
  "get_tls", (1, 0);
  "set_tls_certificates", (1, 0);
  "set_tls_verify_peer", (1, 0);
  "get_tls_verify_peer", (1, 0);
  "set_tls_username", (1, 0);
  "get_tls_username", (1, 0);
  "set_tls_psk_file", (1, 0);
  "add_meta_context", (1, 0);
  "connect_uri", (1, 0);
  "connect_unix", (1, 0);
  "connect_tcp", (1, 0);
  "connect_command", (1, 0);
  "is_read_only", (1, 0);
  "can_flush", (1, 0);
  "can_fua", (1, 0);
  "is_rotational", (1, 0);
  "can_trim", (1, 0);
  "can_zero", (1, 0);
  "can_df", (1, 0);
  "can_multi_conn", (1, 0);
  "can_cache", (1, 0);
  "can_meta_context", (1, 0);
  "get_size", (1, 0);
  "pread", (1, 0);
  "pread_structured", (1, 0);
  "pwrite", (1, 0);
  "shutdown", (1, 0);
  "flush", (1, 0);
  "trim", (1, 0);
  "cache", (1, 0);
  "zero", (1, 0);
  "block_status", (1, 0);
  "poll", (1, 0);
  "aio_connect", (1, 0);
  "aio_connect_uri", (1, 0);
  "aio_connect_unix", (1, 0);
  "aio_connect_tcp", (1, 0);
  "aio_connect_command", (1, 0);
  "aio_pread", (1, 0);
  "aio_pread_structured", (1, 0);
  "aio_pwrite", (1, 0);
  "aio_disconnect", (1, 0);
  "aio_flush", (1, 0);
  "aio_trim", (1, 0);
  "aio_cache", (1, 0);
  "aio_zero", (1, 0);
  "aio_block_status", (1, 0);
  "aio_get_fd", (1, 0);
  "aio_get_direction", (1, 0);
  "aio_notify_read", (1, 0);
  "aio_notify_write", (1, 0);
  "aio_is_created", (1, 0);
  "aio_is_connecting", (1, 0);
  "aio_is_ready", (1, 0);
  "aio_is_processing", (1, 0);
  "aio_is_dead", (1, 0);
  "aio_is_closed", (1, 0);
  "aio_command_completed", (1, 0);
  "aio_peek_command_completed", (1, 0);
  "aio_in_flight", (1, 0);
  "connection_state", (1, 0);
  "get_package_name", (1, 0);
  "get_version", (1, 0);
  "kill_subprocess", (1, 0);
  "supports_tls", (1, 0);
  "supports_uri", (1, 0);

  (* Added in 1.1.x development cycle, will be stable and supported in 1.2. *)
  "can_fast_zero", (1, 2);
  "set_request_structured_replies", (1, 2);
  "get_request_structured_replies", (1, 2);
  "get_structured_replies_negotiated", (1, 2);
  "get_tls_negotiated", (1, 2);
  "get_protocol", (1, 2);
  "set_handshake_flags", (1, 2);
  "get_handshake_flags", (1, 2);
  "connect_systemd_socket_activation", (1, 2);
  "aio_connect_systemd_socket_activation", (1, 2);
  "connect_socket", (1, 2);
  "aio_connect_socket", (1, 2);
  "connect_vsock", (1, 2);
  "aio_connect_vsock", (1, 2);
  "set_uri_allow_transports", (1, 2);
  "set_uri_allow_tls", (1, 2);
  "set_uri_allow_local_file", (1, 2);

  (* Added in 1.3.x development cycle, will be stable and supported in 1.4. *)
  "get_block_size", (1, 4);
  "set_full_info", (1, 4);
  "get_full_info", (1, 4);
  "get_canonical_export_name", (1, 4);
  "get_export_description", (1, 4);
  "set_opt_mode", (1, 4);
  "get_opt_mode", (1, 4);
  "aio_is_negotiating", (1, 4);
  "opt_go", (1, 4);
  "opt_abort", (1, 4);
  "opt_list", (1, 4);
  "opt_info", (1, 4);
  "aio_opt_go", (1, 4);
  "aio_opt_abort", (1, 4);
  "aio_opt_list", (1, 4);
  "aio_opt_info", (1, 4);

  (* Added in 1.5.x development cycle, will be stable and supported in 1.6. *)
  "set_strict_mode", (1, 6);
  "get_strict_mode", (1, 6);
  "get_nr_meta_contexts", (1, 6);
  "get_meta_context", (1, 6);
  "clear_meta_contexts", (1, 6);
  "opt_list_meta_context", (1, 6);
  "aio_opt_list_meta_context", (1, 6);

  (* Added in 1.7.x development cycle, will be stable and supported in 1.8. *)
  "set_private_data", (1, 8);
  "get_private_data", (1, 8);
  "get_uri", (1, 8);

  (* Added in 1.11.x development cycle, will be stable and supported in 1.12. *)
  "set_pread_initialize", (1, 12);
  "get_pread_initialize", (1, 12);
  "set_request_block_size", (1, 12);
  "get_request_block_size", (1, 12);

  (* Added in 1.15.x development cycle, will be stable and supported in 1.16. *)
  "poll2", (1, 16);
  "supports_vsock", (1, 16);
  "stats_bytes_sent", (1, 16);
  "stats_chunks_sent", (1, 16);
  "stats_bytes_received", (1, 16);
  "stats_chunks_received", (1, 16);
  "opt_list_meta_context_queries", (1, 16);
  "aio_opt_list_meta_context_queries", (1, 16);

  (* These calls are proposed for a future version of libnbd, but
   * have not been added to any released version so far.
  "get_tls_certificates", (1, ??);
  "get_tls_psk_file", (1, ??);
   *)
]

(* Constants, etc. See also Enums and Flags above. *)
let constants = [
  "AIO_DIRECTION_READ",  1;
  "AIO_DIRECTION_WRITE", 2;
  "AIO_DIRECTION_BOTH",  3;

  "READ_DATA",           1;
  "READ_HOLE",           2;
  "READ_ERROR",          3;
]

let metadata_namespaces = [
  "base", [ "allocation", [
    "STATE_HOLE", 1 lsl 0;
    "STATE_ZERO", 1 lsl 1;
  ] ];
]

let pod_of_link = function
  | Link page -> sprintf "L<nbd_%s(3)>" page
  | SectionLink anchor -> sprintf "L<libnbd(3)/%s>" anchor
  | MainPageLink -> "L<libnbd(3)>"
  | ExternalLink (page, section) -> sprintf "L<%s(%d)>" page section
  | URLLink url -> sprintf "L<%s>" url

let verify_link =
  let pages = List.map fst handle_calls in
  function
  | Link "create" | Link "close"
  | Link "get_error" | Link "get_errno" -> ()
  | Link page ->
     if not (List.mem page pages) then
       failwithf "verify_link: page nbd_%s does not exist" page
  | SectionLink _ -> (* XXX Could search libnbd(3) for headings. *) ()
  | MainPageLink -> ()
  | ExternalLink (page, section) -> ()
  | URLLink url -> (* XXX Could check URL is well formed. *) ()

let sort_uniq_links links =
  let score = function
    | Link page -> 0, page
    | SectionLink anchor -> 1, anchor
    | MainPageLink -> 2, ""
    | ExternalLink (page, section) -> 3, sprintf "%s(%d)" page section
    | URLLink url -> 4, url
  in
  let cmp link1 link2 = compare (score link1) (score link2) in
  sort_uniq ~cmp links

let extract_links =
  let link_rex = Str.regexp "L<\\([a-z0-9_]+\\)(\\([0-9]\\))>" in
  fun pod ->
    let rec loop acc i =
      let i = try Some (Str.search_forward link_rex pod i)
              with Not_found -> None in
      match i with
      | None -> acc
      | Some i ->
         let page = Str.matched_group 1 pod in
         let section = int_of_string (Str.matched_group 2 pod) in
         let link =
           if is_prefix page "nbd_" then (
             let n = String.length page in
             Link (String.sub page 4 (n-4))
           )
           else if page = "libnbd" && section = 3 then
             MainPageLink
           else
             ExternalLink (page, section) in
         let acc = link :: acc in
         loop acc (i+1)
    in
    loop [] 0

(* Check the API definition. *)
let () =
  (* Check functions using may_set_error. *)
  List.iter (
    function
    (* !may_set_error is incompatible with permitted_states != []
     * because an incorrect state will result in set_error being
     * called by the generated wrapper.
     *)
    | name, { permitted_states = (_::_); may_set_error = false } ->
       failwithf "%s: if may_set_error is false, permitted_states must be empty (any permitted state)"
                 name

    (* may_set_error = true is incompatible with RUInt*, REnum, and RFlags
     * because these calls cannot return an error indication.
     *)
    | name, { ret = RUInt; may_set_error = true }
    | name, { ret = RUIntPtr; may_set_error = true }
    | name, { ret = RUInt64; may_set_error = true }
    | name, { ret = REnum _; may_set_error = true }
    | name, { ret = RFlags _; may_set_error = true } ->
       failwithf "%s: if ret is RUInt/REnum/RFlags, may_set_error must be false" name

    (* !may_set_error is incompatible with certain parameters because
     * we have to do a NULL-check on those which may return an error.
     *)
    | name, { args; may_set_error = false }
         when List.exists
                (function
                 | Closure _ | Enum _ | Flags _ | String _ -> true
                 | _ -> false) args ->
       failwithf "%s: if args contains Closure/Enum/Flags/String parameter, may_set_error must be false" name

    (* !may_set_error is incompatible with certain optargs too.
     *)
    | name, { optargs; may_set_error = false }
         when List.exists
                (function
                 | OFlags _ -> true
                 | _ -> false) optargs ->
       failwithf "%s: if optargs contains an OFlags parameter, may_set_error must be false" name

    | _ -> ()
  ) handle_calls;

  (* first_version must be (0, 0) in handle_calls (we will modify it). *)
  List.iter (
    function
    | (_, { first_version = (0, 0) }) -> ()
    | (name, _) ->
        failwithf "%s: first_version field must not be set in handle_calls table" name
  ) handle_calls;

  (* Check every entry in first_version corresponds 1-1 with handle_calls. *)
  let () =
    let fv_names = List.sort compare (List.map fst first_version) in
    let hc_names = List.sort compare (List.map fst handle_calls) in
    if fv_names <> hc_names then (
      eprintf "first_version names:\n";
      List.iter (eprintf "\t%s\n") fv_names;
      eprintf "handle_calls names:\n";
      List.iter (eprintf "\t%s\n") hc_names;
      failwithf "first_version and handle_calls are not a 1-1 mapping.  You probably forgot to add a new API to the first_version table."
    ) in

  (* Check and update first_version field in handle_calls. *)
  List.iter (
    function
    | (name, entry) ->
       let major, minor = List.assoc name first_version in
       (* First stable version must be 1.x where x is even. *)
       if major <> 1 then
         failwithf "%s: first_version must be 1.x" name;
       if minor mod 2 <> 0 then
         failwithf "%s: first_version must refer to a stable release" name;
       entry.first_version <- (major, minor)
  ) handle_calls;

  (* Because of the way we use completion free callbacks to
   * free persistent buffers in non-C languages, any function
   * with a BytesPersistIn/Out parameter must have only one.
   * And it must have an OClosure completion optarg.
   *)
  List.iter (
    fun (name, { args; optargs }) ->
      let is_persistent_buffer_arg = function
        | BytesPersistIn _ | BytesPersistOut _ -> true
        | _ -> false
      and is_oclosure_completion = function
        | OClosure { cbname = "completion" } -> true
        | _ -> false
      in
      if List.exists is_persistent_buffer_arg args then (
        let bpargs = List.filter is_persistent_buffer_arg args in
        if List.length bpargs >= 2 then
          failwithf "%s: multiple BytesPersistIn/Out params not supported"
            name;
        if not (List.exists is_oclosure_completion optargs) then
          failwithf "%s: functions with BytesPersistIn/Out arg must have completion callback"
            name
      )
  ) handle_calls
