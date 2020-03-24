(* hey emacs, this is OCaml code: -*- tuareg -*- *)
(* nbd client library in userspace: the API
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
| SockAddrAndLen of string * string
| String of string
| StringList of string
| UInt of string
| UInt32 of string
| UInt64 of string
and optarg =
| OClosure of closure
| OFlags of string * flags
and ret =
| RBool
| RStaticString
| RErr
| RFd
| RInt
| RInt64
| RCookie
| RString
| RUInt
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
  flags : (string * int) list
}
and permitted_state =
| Created
| Connecting
| Connected
| Closed | Dead
and link =
| Link of string
| SectionLink of string
| MainPageLink
| ExternalLink of string * int
| URLLink of string

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
let all_closures = [ chunk_closure; completion_closure;
                     debug_closure; extent_closure ]

(* Enums. *)
let tls_enum = {
  enum_prefix = "TLS";
  enums = [
    "DISABLE", 0;
    "ALLOW",   1;
    "REQUIRE", 2;
  ]
}
let all_enums = [ tls_enum ]

(* Flags. See also Constants below. *)
let cmd_flags = {
  flag_prefix = "CMD_FLAG";
  flags = [
    "FUA",       1 lsl 0;
    "NO_HOLE",   1 lsl 1;
    "DF",        1 lsl 2;
    "REQ_ONE",   1 lsl 3;
    "FAST_ZERO", 1 lsl 4;
  ]
}
let handshake_flags = {
  flag_prefix = "HANDSHAKE_FLAG";
  flags = [
    "FIXED_NEWSTYLE", 1 lsl 0;
    "NO_ZEROES",      1 lsl 1;
    ]
}
let allow_transport_flags = {
  flag_prefix = "ALLOW_TRANSPORT";
  flags = [
    "TCP",   1 lsl 0;
    "UNIX",  1 lsl 1;
    "VSOCK", 1 lsl 2;
  ]
}
let all_flags = [ cmd_flags; handshake_flags; allow_transport_flags ]

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
  };

  "set_export_name", {
    default_call with
    args = [ String "export_name" ]; ret = RErr;
    permitted_states = [ Created ];
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

This call may be skipped if using L<nbd_connect_uri(3)> to connect
to a URI that includes an export name.";
  see_also = [Link "get_export_name"; Link "connect_uri"];
  };

  "get_export_name", {
    default_call with
    args = []; ret = RString;
    shortdesc = "get the export name";
    longdesc = "\
Get the export name associated with the handle.";
  see_also = [Link "set_export_name"; Link "connect_uri"];
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
    args = []; ret = RInt;
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
    permitted_states = [ Connected; Closed ];
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
  };

  "get_tls_verify_peer", {
    default_call with
    args = []; ret = RBool;
    may_set_error = false;
    shortdesc = "get whether we verify the identity of the server";
    longdesc = "\
Get the verify peer flag.";
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
C<nbd_can_meta_context> or C<nbd_can_df> can return true.  However,
for integration testing, it can be useful to clear this flag
rather than find a way to alter the server to fail the negotiation
request.";
    see_also = [Link "get_request_structured_replies";
                Link "set_handshake_flags";
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
C<nbd_get_structured_replies_negotiated> instead.";
    see_also = [Link "set_request_structured_replies";
                Link "get_structured_replies_negotiated"];
  };

  "get_structured_replies_negotiated", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Connected; Closed ];
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
that are also supported by the server; since omitting a handshake
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

Future NBD extensions may add further flags.
";
    see_also = [Link "get_handshake_flags";
                Link "set_request_structured_replies"];
  };

  "get_handshake_flags", {
    default_call with
    args = []; ret = RUInt;
    may_set_error = false;
    shortdesc = "see which handshake flags are supported";
    longdesc = "\
Return the state of the handshake flags on this handle.  When the
handle has not yet completed a connection (see C<nbd_aio_is_created>),
this returns the flags that the client is willing to use, provided
the server also advertises those flags.  After the connection is
ready (see C<nbd_aio_is_ready>), this returns the flags that were
actually agreed on between the server and client.  If the NBD
protocol defines new handshake flags, then the return value from
a newer library version may include bits that were undefined at
the time of compilation.";
    see_also = [Link "set_handshake_flags";
                Link "get_protocol";
                Link "aio_is_created"; Link "aio_is_ready"];
  };

  "add_meta_context", {
    default_call with
    args = [ String "name" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "ask server to negotiate metadata context";
    longdesc = "\
During connection libnbd can negotiate zero or more metadata
contexts with the server.  Metadata contexts are features (such
as C<\"base:allocation\">) which describe information returned
by the L<nbd_block_status(3)> command (for C<\"base:allocation\">
this is whether blocks of data are allocated, zero or sparse).

This call adds one metadata context to the list to be negotiated.
You can call it as many times as needed.  The list is initially
empty when the handle is created.

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
C<\"qemu:dirty-bitmap:...\"> for qemu-nbd
(see qemu-nbd I<-B> option).";
    see_also = [Link "block_status"];
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

=back";
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
the NBD URI.  This call parses the URI and may call
L<nbd_set_export_name(3)> and L<nbd_set_tls(3)> and other
calls as needed, followed by
L<nbd_connect_tcp(3)> or L<nbd_connect_unix(3)>.
This call returns when the connection has been made.

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
TLS respectively.

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

=item B<tls-psk-file=>F<PSKFILE>

Set the PSK file.  See L<nbd_set_tls_psk_file(3)>.  Note
this is not allowed by default - see next section.

=back

=head2 Disable URI features

For security reasons you might want to disable certain URI
features.  Pre-filtering URIs is error-prone and should not
be attempted.  Instead use the libnbd APIs below to control
what can appear in URIs.  Note you must call these functions
on the same handle before calling C<nbd_connect_uri> or
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

=head2 Optional features

This call will fail if libnbd was not compiled with libxml2; you can
test whether this is the case with L<nbd_supports_uri(3)>.

Support for URIs that require TLS will fail if libnbd was not
compiled with gnutls; you can test whether this is the case
with L<nbd_supports_tls(3)>.";
    see_also = [URLLink "https://github.com/NetworkBlockDevice/nbd/blob/master/doc/uri.md"];
  };

  "connect_unix", {
    default_call with
    args = [ Path "unixsocket" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to NBD server over a Unix domain socket";
    longdesc = "\
Connect (synchronously) over the named Unix domain socket (C<unixsocket>)
to an NBD server running on the same machine.  This call returns
when the connection has been made.";
    example = Some "examples/fetch-first-sector.c";
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
administrator.  This call returns when the connection has been made.";
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
such as C<\"10809\">.  This call returns when the connection
has been made.";
  };

  "connect_socket", {
    default_call with
    args = [ Fd "sock" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect directly to a connected socket";
    longdesc = "\
Pass a connected socket (file descriptor) which libnbd will
talk to.  The program is responsible for connecting this
somehow to an NBD server.  Once the socket is passed to
libnbd it is the responsibility of libnbd.  Libnbd may
read and write to it, set it to non-blocking, etc., and
will finally close it when the handle is closed.  The
program must no longer use the socket.";
  };

  "connect_command", {
    default_call with
    args = [ StringList "argv" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to NBD server command";
    longdesc = "\
Run the command as a subprocess and connect to it over
stdin/stdout.  This is for use with NBD servers which can
behave like inetd clients, such as C<nbdkit --single>.

=head2 Subprocess

Libnbd will fork the C<argv> command and pass the NBD socket
to it using file descriptors 0 and 1 (stdin/stdout):

 ┌─────────┬─────────┐    ┌────────────────┐
 │ program │ libnbd  │    │   NBD server   │
 │         │         │    │       (argv)   │
 │         │ socket ╍╍╍╍╍╍╍╍▶ stdin/stdout │
 └─────────┴─────────┘    └────────────────┘

When the NBD handle is closed the server subprocess
is killed.";
    see_also = [Link "kill_subprocess"];
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
qcow2 files.  To run nbdkit as a subprocess it is usually
better to use L<nbd_connect_command(3)>.

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
is killed.";
    see_also = [Link "connect_command"; Link "kill_subprocess";
                ExternalLink ("qemu-nbd", 1);
                URLLink "http://0pointer.de/blog/projects/socket-activation.html"];
    example = Some "examples/open-qcow2.c";
  };

  "is_read_only", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Connected; Closed ];
    shortdesc = "is the NBD export read-only?";
    longdesc = "\
Returns true if the NBD export is read-only; writes and
write-like operations will fail."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls"];
    example = Some "examples/server-flags.c";
  };

  "can_flush", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Connected; Closed ];
    shortdesc = "does the server support the flush command?";
    longdesc = "\
Returns true if the server supports the flush command
(see L<nbd_flush(3)>, L<nbd_aio_flush(3)>).  Returns false if
the server does not."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls";
                Link "flush"; Link "aio_flush"];
    example = Some "examples/server-flags.c";
  };

  "can_fua", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Connected; Closed ];
    shortdesc = "does the server support the FUA flag?";
    longdesc = "\
Returns true if the server supports the FUA flag on
certain commands (see L<nbd_pwrite(3)>)."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls"; Link "pwrite";
                Link "zero"; Link "trim"];
    example = Some "examples/server-flags.c";
  };

  "is_rotational", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Connected; Closed ];
    shortdesc = "is the NBD disk rotational (like a disk)?";
    longdesc = "\
Returns true if the disk exposed over NBD is rotational
(like a traditional floppy or hard disk).  Returns false if
the disk has no penalty for random access (like an SSD or
RAM disk)."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls"];
    example = Some "examples/server-flags.c";
  };

  "can_trim", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Connected; Closed ];
    shortdesc = "does the server support the trim command?";
    longdesc = "\
Returns true if the server supports the trim command
(see L<nbd_trim(3)>, L<nbd_aio_trim(3)>).  Returns false if
the server does not."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls";
                Link "trim"; Link "aio_trim"];
    example = Some "examples/server-flags.c";
  };

  "can_zero", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Connected; Closed ];
    shortdesc = "does the server support the zero command?";
    longdesc = "\
Returns true if the server supports the zero command
(see L<nbd_zero(3)>, L<nbd_aio_zero(3)>).  Returns false if
the server does not."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls";
                Link "zero"; Link "aio_zero";
                Link "can_fast_zero"];
    example = Some "examples/server-flags.c";
  };

  "can_fast_zero", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Connected; Closed ];
    shortdesc = "does the server support the fast zero flag?";
    longdesc = "\
Returns true if the server supports the use of the
C<LIBNBD_CMD_FLAG_FAST_ZERO> flag to the zero command
(see C<nbd_zero>, C<nbd_aio_zero>).  Returns false if
the server does not."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls";
                Link "zero"; Link "aio_zero"; Link "can_zero"];
    example = Some "examples/server-flags.c";
  };

  "can_df", {
    default_call with
    args = []; ret = RBool;
    shortdesc = "does the server support the don't fragment flag to pread?";
    longdesc = "\
Returns true if the server supports structured reads with an
ability to request a non-fragmented read (see L<nbd_pread_structured(3)>,
L<nbd_aio_pread_structured(3)>).  Returns false if the server either lacks
structured reads or if it does not support a non-fragmented read request."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls";
                Link "pread_structured";
                Link "aio_pread_structured"];
    example = Some "examples/server-flags.c";
  };

  "can_multi_conn", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Connected; Closed ];
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
    see_also = [SectionLink "Multi-conn"];
    example = Some "examples/server-flags.c";
  };

  "can_cache", {
    default_call with
    args = []; ret = RBool;
    permitted_states = [ Connected; Closed ];
    shortdesc = "does the server support the cache command?";
    longdesc = "\
Returns true if the server supports the cache command
(see L<nbd_cache(3)>, L<nbd_aio_cache(3)>).  Returns false if
the server does not."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls";
                Link "cache"; Link "aio_cache"];
    example = Some "examples/server-flags.c";
  };

  "can_meta_context", {
    default_call with
    args = [String "metacontext"]; ret = RBool;
    permitted_states = [ Connected; Closed ];
    shortdesc = "does the server support a specific meta context?";
    longdesc = "\
Returns true if the server supports the given meta context
(see L<nbd_add_meta_context(3)>).  Returns false if
the server does not.

The single parameter is the name of the metadata context,
for example C<LIBNBD_CONTEXT_BASE_ALLOCATION>.
B<E<lt>libnbd.hE<gt>> includes defined constants for well-known
namespace contexts beginning with C<LIBNBD_CONTEXT_>, but you
are free to pass in other contexts."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Flag calls";
                Link "add_meta_context";
                Link "block_status"; Link "aio_block_status"];
  };

  "get_protocol", {
    default_call with
    args = []; ret = RStaticString;
    permitted_states = [ Connected; Closed ];
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
                Link "get_tls_negotiated"];
  };

  "get_size", {
    default_call with
    args = []; ret = RInt64;
    permitted_states = [ Connected; Closed ];
    shortdesc = "return the export size";
    longdesc = "\
Returns the size in bytes of the NBD export."
^ non_blocking_test_call_description;
    see_also = [SectionLink "Size of the export"];
    example = Some "examples/get-size.c";
  };

  "pread", {
    default_call with
    args = [ BytesOut ("buf", "count"); UInt64 "offset" ];
    optargs = [ OFlags ("flags", cmd_flags) ];
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

The C<flags> parameter must be C<0> for now (it exists for future NBD
protocol extensions).";
    see_also = [Link "aio_pread"; Link "pread_structured"];
    example = Some "examples/fetch-first-sector.c";
  };

  "pread_structured", {
    default_call with
    args = [ BytesOut ("buf", "count"); UInt64 "offset";
             Closure chunk_closure ];
    optargs = [ OFlags ("flags", cmd_flags) ];
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

The C<flags> parameter may be C<0> for no flags, or may contain
C<LIBNBD_CMD_FLAG_DF> meaning that the server should not reply with
more than one fragment (if that is supported - some servers cannot do
this, see L<nbd_can_df(3)>). Libnbd does not validate that the server
actually obeys the flag.";
    see_also = [Link "can_df"; Link "pread";
                Link "aio_pread_structured"];
  };

  "pwrite", {
    default_call with
    args = [ BytesIn ("buf", "count"); UInt64 "offset" ];
    optargs = [ OFlags ("flags", cmd_flags) ];
    ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "write to the NBD server";
    longdesc = "\
Issue a write command to the NBD server, writing the data in
C<buf> to the range starting at C<offset> and ending at
C<offset> + C<count> - 1.  NBD can only write all or nothing
using this call.  The call returns when the command has been
acknowledged by the server, or there is an error.

The C<flags> parameter may be C<0> for no flags, or may contain
C<LIBNBD_CMD_FLAG_FUA> meaning that the server should not
return until the data has been committed to permanent storage
(if that is supported - some servers cannot do this, see
L<nbd_can_fua(3)>).";
    see_also = [Link "can_fua"; Link "is_read_only";
                Link "aio_pwrite"];
    example = Some "examples/reads-and-writes.c";
  };

  "shutdown", {
    default_call with
    args = []; optargs = [ OFlags ("flags", cmd_flags) ]; ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "disconnect from the NBD server";
    longdesc = "\
Issue the disconnect command to the NBD server.  This is
a nice way to tell the server we are going away, but from the
client's point of view has no advantage over abruptly closing
the connection (see L<nbd_close(3)>).

This function works whether or not the handle is ready for
transmission of commands, and as such does not take a C<flags>
parameter. If more fine-grained control is needed, see
L<nbd_aio_disconnect(3)>.

The C<flags> parameter must be C<0> for now (it exists for future NBD
protocol extensions).";
    see_also = [Link "close"; Link "aio_disconnect"];
    example = Some "examples/reads-and-writes.c";
  };

  "flush", {
    default_call with
    args = []; optargs = [ OFlags ("flags", cmd_flags) ]; ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "send flush command to the NBD server";
    longdesc = "\
Issue the flush command to the NBD server.  The function should
return when all write commands which have completed have been
committed to permanent storage on the server.  Note this will
return an error if L<nbd_can_flush(3)> is false.

The C<flags> parameter must be C<0> for now (it exists for future NBD
protocol extensions).";
    see_also = [Link "can_flush"; Link "aio_flush"];
  };

  "trim", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset" ];
    optargs = [ OFlags ("flags", cmd_flags) ];
    ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "send trim command to the NBD server";
    longdesc = "\
Issue a trim command to the NBD server, which if supported
by the server causes a hole to be punched in the backing
store starting at C<offset> and ending at C<offset> + C<count> - 1.
The call returns when the command has been acknowledged by the server,
or there is an error.

The C<flags> parameter may be C<0> for no flags, or may contain
C<LIBNBD_CMD_FLAG_FUA> meaning that the server should not
return until the data has been committed to permanent storage
(if that is supported - some servers cannot do this, see
L<nbd_can_fua(3)>).";
    see_also = [Link "can_fua"; Link "can_trim";
                Link "aio_trim"];
  };

  "cache", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset" ];
    optargs = [ OFlags ("flags", cmd_flags) ];
    ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "send cache (prefetch) command to the NBD server";
    longdesc = "\
Issue the cache (prefetch) command to the NBD server, which
if supported by the server causes data to be prefetched
into faster storage by the server, speeding up a subsequent
L<nbd_pread(3)> call.  The server can also silently ignore
this command.  Note this will return an error if
L<nbd_can_cache(3)> is false.

The C<flags> parameter must be C<0> for now (it exists for future NBD
protocol extensions).";
    see_also = [Link "can_cache"; Link "aio_cache"];
  };

  "zero", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset" ];
    optargs = [ OFlags ("flags", cmd_flags) ];
    ret = RErr;
    permitted_states = [ Connected ];
    shortdesc = "send write zeroes command to the NBD server";
    longdesc = "\
Issue a write zeroes command to the NBD server, which if supported
by the server causes a zeroes to be written efficiently
starting at C<offset> and ending at C<offset> + C<count> - 1.
The call returns when the command has been acknowledged by the server,
or there is an error.

The C<flags> parameter may be C<0> for no flags, or may contain
C<LIBNBD_CMD_FLAG_FUA> meaning that the server should not
return until the data has been committed to permanent storage
(if that is supported - some servers cannot do this, see
L<nbd_can_fua(3)>), C<LIBNBD_CMD_FLAG_NO_HOLE> meaning that
the server should favor writing actual allocated zeroes over
punching a hole, and/or C<LIBNBD_CMD_FLAG_FAST_ZERO> meaning
that the server must fail quickly if writing zeroes is no
faster than a normal write (if that is supported - some servers
cannot do this, see L<nbd_can_fast_zero(3)>).";
    see_also = [Link "can_fua"; Link "can_zero";
                Link "can_fast_zero"; Link "aio_zero"];
  };

  "block_status", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset"; Closure extent_closure ];
    optargs = [ OFlags ("flags", cmd_flags) ];
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
value of any previously detected error.

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
validate that the server obeyed the flag.";
    see_also = [Link "add_meta_context"; Link "can_meta_context";
                Link "aio_block_status"];
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
  };

  "aio_connect", {
    default_call with
    args = [ SockAddrAndLen ("addr", "addrlen") ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to the NBD server";
    longdesc = "\
Begin connecting to the NBD server.  The C<addr> and C<addrlen>
parameters specify the address of the socket to connect to.

You can check if the connection is still connecting by calling
L<nbd_aio_is_connecting(3)>, or if it has connected to the server
and completed the NBD handshake by calling L<nbd_aio_is_ready(3)>,
on the connection.";
  };

  "aio_connect_uri", {
    default_call with
    args = [ String "uri" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to an NBD URI";
    longdesc = "\
Begin connecting to the NBD URI C<uri>.  Parameters behave as
documented in L<nbd_connect_uri(3)>.

You can check if the connection is still connecting by calling
L<nbd_aio_is_connecting(3)>, or if it has connected to the server
and completed the NBD handshake by calling L<nbd_aio_is_ready(3)>,
on the connection.";
    see_also = [URLLink "https://github.com/NetworkBlockDevice/nbd/blob/master/doc/uri.md"]
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

You can check if the connection is still connecting by calling
L<nbd_aio_is_connecting(3)>, or if it has connected to the server
and completed the NBD handshake by calling L<nbd_aio_is_ready(3)>,
on the connection.";
    example = Some "examples/aio-connect-read.c";
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

You can check if the connection is still connecting by calling
L<nbd_aio_is_connecting(3)>, or if it has connected to the server
and completed the NBD handshake by calling L<nbd_aio_is_ready(3)>,
on the connection.";
  };

  "aio_connect_tcp", {
    default_call with
    args = [ String "hostname"; String "port" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect to the NBD server over a TCP port";
    longdesc = "\
Begin connecting to the NBD server listening on C<hostname:port>.
Parameters behave as documented in L<nbd_connect_tcp(3)>.

You can check if the connection is still connecting by calling
L<nbd_aio_is_connecting(3)>, or if it has connected to the server
and completed the NBD handshake by calling L<nbd_aio_is_ready(3)>,
on the connection.";
  };

  "aio_connect_socket", {
    default_call with
    args = [ Fd "sock" ]; ret = RErr;
    permitted_states = [ Created ];
    shortdesc = "connect directly to a connected socket";
    longdesc = "\
Begin connecting to the connected socket C<fd>.
Parameters behave as documented in L<nbd_connect_socket(3)>.

You can check if the connection is still connecting by calling
L<nbd_aio_is_connecting(3)>, or if it has connected to the server
and completed the NBD handshake by calling L<nbd_aio_is_ready(3)>,
on the connection.";
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

You can check if the connection is still connecting by calling
L<nbd_aio_is_connecting(3)>, or if it has connected to the server
and completed the NBD handshake by calling L<nbd_aio_is_ready(3)>,
on the connection.";
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

You can check if the connection is still connecting by calling
L<nbd_aio_is_connecting(3)>, or if it has connected to the server
and completed the NBD handshake by calling L<nbd_aio_is_ready(3)>,
on the connection.";
  };

  "aio_pread", {
    default_call with
    args = [ BytesPersistOut ("buf", "count"); UInt64 "offset" ];
    optargs = [ OClosure completion_closure; OFlags ("flags", cmd_flags) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "read from the NBD server";
    longdesc = "\
Issue a read command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Note that you must ensure C<buf> is valid until the command has
completed.  Other parameters behave as documented in L<nbd_pread(3)>.";
    example = Some "examples/aio-connect-read.c";
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "aio_pread_structured"; Link "pread"];
  };

  "aio_pread_structured", {
    default_call with
    args = [ BytesPersistOut ("buf", "count"); UInt64 "offset";
             Closure chunk_closure ];
    optargs = [ OClosure completion_closure; OFlags ("flags", cmd_flags) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "read from the NBD server";
    longdesc = "\
Issue a read command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Other parameters behave as documented in L<nbd_pread_structured(3)>.";
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "aio_pread"; Link "pread_structured"];
  };

  "aio_pwrite", {
    default_call with
    args = [ BytesPersistIn ("buf", "count"); UInt64 "offset" ];
    optargs = [ OClosure completion_closure; OFlags ("flags", cmd_flags) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "write to the NBD server";
    longdesc = "\
Issue a write command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Note that you must ensure C<buf> is valid until the command has
completed.  Other parameters behave as documented in L<nbd_pwrite(3)>.";
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "is_read_only"; Link "pwrite"];
  };

  "aio_disconnect", {
    default_call with
    args = []; optargs = [ OFlags ("flags", cmd_flags) ]; ret = RErr;
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
    optargs = [ OClosure completion_closure; OFlags ("flags", cmd_flags) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "send flush command to the NBD server";
    longdesc = "\
Issue the flush command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Other parameters behave as documented in L<nbd_flush(3)>.";
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "can_flush"; Link "flush"];
  };

  "aio_trim", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset" ];
    optargs = [ OClosure completion_closure; OFlags ("flags", cmd_flags) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "send trim command to the NBD server";
    longdesc = "\
Issue a trim command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Other parameters behave as documented in L<nbd_trim(3)>.";
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "can_trim"; Link "trim"];
  };

  "aio_cache", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset" ];
    optargs = [ OClosure completion_closure; OFlags ("flags", cmd_flags) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "send cache (prefetch) command to the NBD server";
    longdesc = "\
Issue the cache (prefetch) command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Other parameters behave as documented in L<nbd_cache(3)>.";
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "can_cache"; Link "cache"];
  };

  "aio_zero", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset" ];
    optargs = [ OClosure completion_closure; OFlags ("flags", cmd_flags) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "send write zeroes command to the NBD server";
    longdesc = "\
Issue a write zeroes command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Other parameters behave as documented in L<nbd_zero(3)>.";
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "can_zero"; Link "can_fast_zero";
                Link "zero"];
  };

  "aio_block_status", {
    default_call with
    args = [ UInt64 "count"; UInt64 "offset"; Closure extent_closure ];
    optargs = [ OClosure completion_closure; OFlags ("flags", cmd_flags) ];
    ret = RCookie;
    permitted_states = [ Connected ];
    shortdesc = "send block status command to the NBD server";
    longdesc = "\
Send the block status command to the NBD server.

To check if the command completed, call L<nbd_aio_command_completed(3)>.
Or supply the optional C<completion_callback> which will be invoked
as described in L<libnbd(3)/Completion callbacks>.

Other parameters behave as documented in L<nbd_block_status(3)>.";
    see_also = [SectionLink "Issuing asynchronous commands";
                Link "can_meta_context"; Link "block_status"];
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

  "supports_uri", {
    default_call with
    args = []; ret = RBool; is_locked = false; may_set_error = false;
    shortdesc = "true if libnbd was compiled with support for NBD URIs";
    longdesc = "\
Returns true if libnbd was compiled with libxml2 which is required
to support NBD URIs, or false if not.";
    see_also = [Link "connect_uri"; Link "aio_connect_uri"];
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
  | Link "create" | Link "close" -> ()
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

    (* may_set_error = true is incompatible with RUInt because
     * these calls cannot return an error indication.
     *)
    | name, { ret = RUInt; may_set_error = true } ->
       failwithf "%s: if ret is RUInt, may_set_error must be false" name

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
