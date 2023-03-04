# NBD client library in userspace
# Copyright Red Hat
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


import traceback


# The NBD shell.
def shell():
    import argparse
    import code
    import os
    import sys

    import libnbdmod
    import nbd

    description = '''Network Block Device (NBD) shell'''
    epilog = '''Please read the nbdsh(1) manual page for full usage.'''
    parser = argparse.ArgumentParser(prog='nbdsh', description=description,
                                     epilog=epilog)

    # Allow intermixing of various options for replay in command-line order:
    # each option registered with this Action subclass will append a tuple
    # to a single list of snippets
    class SnippetAction(argparse.Action):
        def __init__(self, option_strings, dest, nargs=None,
                     default=argparse.SUPPRESS, **kwargs):
            if nargs not in [0, None]:
                raise ValueError("nargs must be 0 or None")
            super().__init__(option_strings, dest, nargs=nargs,
                             default=default, **kwargs)

        def __call__(self, parser, namespace, values, option_string=None):
            dest = self.dest
            if dest != 'command':
                setattr(namespace, 'need_handle',
                        getattr(namespace, 'need_handle') + 1)
            elif values == '-':
                dest = 'stdin'
            snippets = getattr(namespace, 'snippets')[:]
            snippets.append((dest, values))
            setattr(namespace, 'snippets', snippets)

    parser.set_defaults(need_handle=0, snippets=[])
    short_options = []
    long_options = []

    parser.add_argument('--base-allocation', action=SnippetAction, nargs=0,
                        help='request the "base:allocation" meta context')
    long_options.append("--base-allocation")

    parser.add_argument('-c', '--command', action=SnippetAction,
                        help="run a Python statement "
                        "(may be used multiple times)")
    short_options.append("-c")
    long_options.append("--command")

    parser.add_argument('-n', action='store_true',
                        help="do not create the implicit handle 'h'")
    short_options.append("-n")

    parser.add_argument('--opt-mode', action=SnippetAction, nargs=0,
                        help='request opt mode during connection')
    long_options.append("--opt-mode")

    parser.add_argument('-u', '--uri', action=SnippetAction,
                        help="connect to NBD URI")
    short_options.append("-u")
    long_options.append("--uri")
    # For back-compat, provide --connect as an undocumented synonym to --uri
    parser.add_argument('--connect', dest='uri', action=SnippetAction,
                        help=argparse.SUPPRESS)

    parser.add_argument('-v', '--verbose', action='store_true',
                        help="enable verbose debugging")
    short_options.append("-v")
    long_options.append("--verbose")

    parser.add_argument('-V', '--version', action='store_true',
                        help="display version information")
    short_options.append("-V")
    long_options.append("--version")

    # These hidden options are used by bash tab completion.
    parser.add_argument("--short-options", action='store_true',
                        help=argparse.SUPPRESS)
    parser.add_argument("--long-options", action='store_true',
                        help=argparse.SUPPRESS)

    args = parser.parse_args()

    # It's an error if -n is passed with certain other options.
    if args.n and args.need_handle:
        print("error: -n option cannot be used with " +
              "--base-allocation, --opt-mode or --uri",
              file=sys.stderr)
        exit(1)

    # Handle the informational options which exit.
    if args.version:
        libnbdmod.display_version("nbdsh")
        exit(0)
    if args.short_options:
        short_options.sort()
        print("\n".join(short_options))
        exit(0)
    if args.long_options:
        long_options.sort()
        print("\n".join(long_options))
        exit(0)

    # If verbose, set LIBNBD_DEBUG=1
    if args.verbose:
        os.environ["LIBNBD_DEBUG"] = "1"

    # Create the handle.
    if not args.n:
        h = nbd.NBD()
        h.set_handle_name("nbdsh")

    # Run all snippets
    # https://stackoverflow.com/a/11754346
    d = dict(locals(), **globals())
    do_snippet = {
        "command": lambda arg: exec(arg, d, d),
        "stdin": lambda arg: exec(sys.stdin.read(), d, d),
        "base_allocation": lambda arg: h.add_meta_context(
            nbd.CONTEXT_BASE_ALLOCATION),
        "opt_mode": lambda arg: h.set_opt_mode(True),
        "uri": lambda arg: h.connect_uri(arg),
    }
    try:
        for (act, arg) in args.snippets:
            do_snippet[act](arg)
    except nbd.Error as ex:
        if nbd.NBD().get_debug():
            traceback.print_exc()
        else:
            print("nbdsh: command line script failed: %s" % ex.string,
                  file=sys.stderr)
        sys.exit(1)

    # If there are no explicit -c or --command parameters, go interactive.
    if len(args.snippets) - args.need_handle == 0:
        sys.ps1 = "nbd> "
        code.interact(banner=make_banner(args), local=locals(), exitmsg='')


def make_banner(args):
    lines = []
    def line(x): lines.append(x)
    def blank(): line("")
    def example(ex, desc): line("%-34s # %s" % (ex, desc))

    blank()
    line("Welcome to nbdsh, the shell for interacting with")
    line("Network Block Device (NBD) servers.")
    blank()
    if not args.n:
        line("The ‘nbd’ module has already been imported and there")
        line("is an open NBD handle called ‘h’.")
        blank()
    else:
        line("The ‘nbd’ module has already been imported.")
        blank()
        example("h = nbd.NBD()", "Create a new handle.")
    example("h.get_size()", "Get size of the remote disk.")
    example("buf = h.pread(512, 0)", "Read the first sector.")
    example("exit() or Ctrl-D", "Quit the shell")
    example("help(nbd)", "Display documentation")
    blank()

    return "\n".join(lines)
