# NBD client library in userspace
# Copyright (C) 2013-2019 Red Hat Inc.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

# The NBD shell.
def shell():
    import argparse
    import code
    import sys

    import nbd

    description = '''Network Block Device (NBD) shell'''
    epilog = '''Please read the nbdsh(1) manual page for full usage.'''
    parser = argparse.ArgumentParser (prog='nbdsh', description=description,
                                      epilog=epilog)
    short_options = []
    long_options = []

    parser.add_argument ('--base-allocation', action='store_true',
                         help='request the "base:allocation" meta context')
    long_options.append ("--base-allocation")

    parser.add_argument ('-u', '--uri',
                         help="connect to NBD URI")
    short_options.append ("-u")
    long_options.append ("--uri")
    # For back-compat, provide --connect as an undocumented synonym to --uri
    parser.add_argument ('--connect', dest='uri', help=argparse.SUPPRESS)

    parser.add_argument ('-c', '--command', action='append',
                         help="run a command")
    short_options.append ("-c")
    long_options.append ("--command")

    parser.add_argument ('-V', '--version', action='version',
                         version=nbd.package_name + ' ' + nbd.__version__)
    short_options.append ("-V")
    long_options.append ("--version")

    # These hidden options are used by bash tab completion.
    parser.add_argument ("--short-options", action='store_true')
    parser.add_argument ("--long-options", action='store_true')

    args = parser.parse_args ()

    if args.short_options:
        short_options.sort()
        print ("\n".join (short_options))
        exit (0)
    if args.long_options:
        long_options.sort()
        print ("\n".join (long_options))
        exit (0)

    h = nbd.NBD ()
    h.set_handle_name ("nbdsh")
    sys.ps1 = "nbd> "

    banner = '''
Welcome to nbdsh, the shell for interacting with
Network Block Device (NBD) servers.

The ‘nbd’ module has already been imported and there
is an open NBD handle called ‘h’.

h.connect_tcp ("remote", "10809")   # Connect to a remote server.
h.get_size ()                       # Get size of the remote disk.
buf = h.pread (512, 0, 0)           # Read the first sector.
exit() or Ctrl-D                    # Quit the shell
help (nbd)                          # Display documentation
'''

    if args.base_allocation:
        h.add_meta_context (nbd.CONTEXT_BASE_ALLOCATION)
    if args.uri is not None:
        h.connect_uri (args.uri)
    # If there are no -c or --command parameters, go interactive,
    # otherwise we run the commands and exit.
    if not args.command:
        code.interact (banner = banner, local = locals(), exitmsg = '')
    else:
        # https://stackoverflow.com/a/11754346
        d = dict (locals(), **globals())
        for c in args.command:
            if c != '-':
                exec (c, d, d)
            else:
                exec (sys.stdin.read (), d, d)
