#!/usr/bin/env bash
# nbd client library in userspace
# Copyright (C) 2013-2023 Red Hat Inc.
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

. ../tests/functions.sh

set -e

# Determine the absolute pathname of the execvpe helper binary. The "realpath"
# utility is not in POSIX, but Linux, FreeBSD and OpenBSD all have it.
# Unfortunately, not all variants recognize the "--" end-of-options argument
# separator, though. :/
bname=$(basename -- "$0" .sh)
dname=$(dirname -- "$0")
execvpe=$(realpath "$dname/$bname")

# This is an elaborate way to control the PATH variable around the $execvpe
# helper binary as narrowly as possible.
#
# If $1 is "_", then the $execvpe helper binary is invoked with PATH unset.
# Otherwise, the binary is invoked with PATH set to $1.
#
# $2 and onward are passed to $execvpe; note that $2 becomes *both*
# "program-to-exec" for the helper *and* argv[0] for the program executed by the
# helper.
#
# The command itself (including the PATH setting) is written to "cmd" (for error
# reporting purposes only); the standard output and error are saved in "out" and
# "err" respectively; the exit status is written to "status". This function
# should never fail; if it does, then that's a bug in this unit test script, or
# the disk is full etc.
run()
{
    local pathctl=$1
    local program=$2
    local exit_status

    shift 1

    if test _ = "$pathctl"; then
        printf 'unset PATH; %s %s %s\n' "$execvpe" "$program" "$*" >cmd
        set +e
        (
            unset PATH
            "$execvpe" "$program" "$@" >out 2>err
        )
        exit_status=$?
        set -e
    else
        printf 'PATH=%s %s %s %s\n' "$pathctl" "$execvpe" "$program" "$*" >cmd
        set +e
        PATH=$pathctl "$execvpe" "$program" "$@" >out 2>err
        exit_status=$?
        set -e
    fi
    printf '%d\n' $exit_status >status
}

# After "run" returns, the following three functions can verify the result.
#
# Check if the helper binary failed in nbd_internal_execvpe_init().
#
# $1 is the error number (a macro name such as ENOENT) that's expected of
# nbd_internal_execvpe_init().
init_fail()
{
    local expected_error="nbd_internal_execvpe_init: $1"
    local cmd=$(< cmd)
    local err=$(< err)
    local status=$(< status)

    if test 0 -eq "$status"; then
        printf "'%s' should have failed\\n" "$cmd" >&2
        return 1
    fi
    if test x"$err" != x"$expected_error"; then
        printf "'%s' should have failed with '%s', got '%s'\\n" \
               "$cmd" "$expected_error" "$err" >&2
        return 1
    fi
}

# Check if the helper binary failed in nbd_internal_fork_safe_execvpe().
#
# $1 is the output (the list of candidate pathnames) that
# nbd_internal_execvpe_init() is expected to produce; with inner <newline>
# characters replaced with <comma> characters, and the last <newline> stripped.
#
# $2 is the error number (a macro name such as ENOENT) that's expected of
# nbd_internal_fork_safe_execvpe().
execve_fail()
{
    local expected_output=$1
    local expected_error="nbd_internal_fork_safe_execvpe: $2"
    local cmd=$(< cmd)
    local out=$(< out)
    local err=$(< err)
    local status=$(< status)

    if test 0 -eq "$status"; then
        printf "'%s' should have failed\\n" "$cmd" >&2
        return 1
    fi
    if test x"$err" != x"$expected_error"; then
        printf "'%s' should have failed with '%s', got '%s'\\n" \
               "$cmd" "$expected_error" "$err" >&2
        return 1
    fi
    out=${out//$'\n'/,}
    if test x"$out" != x"$expected_output"; then
        printf "'%s' should have output '%s', got '%s'\\n" \
               "$cmd" "$expected_output" "$out" >&2
        return 1
    fi
}

# Check if the helper binary and the program executed by it succeeded.
#
# $1 is the output (the list of candidate pathnames) that
# nbd_internal_execvpe_init() is expected to produce, followed by any output
# expected of the program that's executed by the helper; with inner <newline>
# characters replaced with <comma> characters, and the last <newline> stripped.
success()
{
    local expected_output=$1
    local cmd=$(< cmd)
    local out=$(< out)
    local status=$(< status)

    if test 0 -ne "$status"; then
        printf "'%s' should have succeeded\\n" "$cmd" >&2
        return 1
    fi
    out=${out//$'\n'/,}
    if test x"$out" != x"$expected_output"; then
        printf "'%s' should have output '%s', got '%s'\\n" \
               "$cmd" "$expected_output" "$out" >&2
        return 1
    fi
}

# Create a temporary directory and change the working directory to it.
tmpd=$(mktemp -d)
cleanup_fn rm -r -- "$tmpd"
cd "$tmpd"

# If the "file" parameter of execvpe() is an empty string, then we must fail --
# in nbd_internal_execvpe_init() -- regardless of PATH.
run _  ""; init_fail ENOENT
run "" ""; init_fail ENOENT
run .  ""; init_fail ENOENT

# Create subdirectories for triggering non-fatal internal error conditions of
# execvpe(). (Almost) every subdirectory will contain one entry, called "f".
#
# Create a directory that's empty.
mkdir empty

# Create a directory with a named pipe (FIFO) in it.
mkdir fifo
mkfifo fifo/f

# Create a directory with a directory in it.
mkdir subdir
mkdir subdir/f

# Create a directory with a non-executable file in it.
mkdir nxregf
touch nxregf/f

# Create a symlink loop.
ln -s symlink symlink

# Create a directory with a (most likely) binary executable in it.
mkdir bin
expr_pathname=$(command -p -v expr)
cp -- "$expr_pathname" bin/f

# Create a directory with an executable shell script that does not contain a
# shebang (#!). The script will print $0 and $1, and not depend on PATH for it.
mkdir sh
printf "command -p printf '%%s %%s\\\\n' \"\$0\" \"\$1\"\\n" >sh/f
chmod u+x sh/f

# In the tests below, invoke each "f" such that the "file" parameter of
# execvpe() contain a <slash> character.
#
# Therefore, PATH does not matter. Set it to the empty string. (Which in this
# implementation would cause nbd_internal_execvpe_init() to fail with ENOENT, if
# the "file" parameter didn't contain a <slash>.)
run "" empty/f;   execve_fail empty/f   ENOENT
run "" fifo/f;    execve_fail fifo/f    EACCES
run "" subdir/f;  execve_fail subdir/f  EACCES
run "" nxregf/f;  execve_fail nxregf/f  EACCES
run "" nxregf/f/; execve_fail nxregf/f/ ENOTDIR
run "" symlink/f; execve_fail symlink/f ELOOP

# This runs "expr 1 + 1".
run "" bin/f 1 + 1; success bin/f,2

# This triggers the ENOEXEC branch in nbd_internal_fork_safe_execvpe().
# nbd_internal_fork_safe_execvpe() will first try
#
#   execve("sh/f", {"sh/f", "arg1", NULL}, envp)
#
# hitting ENOEXEC. Then it will successfully call
#
#   execve("/bin/sh", {"sh/f", "sh/f", "arg1", NULL}, envp)
#
# The shell script will get "sh/f" for $0 and "arg1" for $1, and print those
# out.
run "" sh/f arg1; success sh/f,"sh/f arg1"

# In the tests below, the "file" parameter of execvpe() will not contain a
# <slash> character.
#
# Show that PATH matters that way -- first, trigger an ENOENT in
# nbd_internal_execvpe_init() by setting PATH to the empty string.
run "" expr 1 + 1; init_fail ENOENT

# Fall back to confstr(_CS_PATH) in nbd_internal_execvpe_init(), by unsetting
# PATH. Verify the generated candidates by invoking "getconf PATH" here, and
# appending "/expr" to each prefix.
expected_output=$(
    path=$(command -p getconf PATH)
    IFS=:
    for p in $path; do
        printf '%s/%s\n' $p expr
    done
    command -p expr 1 + 1
)
run _ expr 1 + 1; success "${expected_output//$'\n'/,}"

# Continue with tests where the "file" parameter of execvpe() does not contain a
# <slash> character, but now set PATH to explicit prefix lists.
#
# Show that, if the last candidate fails execve() with an error number that
# would not be fatal otherwise, we do get that error number.
run empty:fifo:subdir:nxregf:symlink f
execve_fail empty/f,fifo/f,subdir/f,nxregf/f,symlink/f ELOOP

# Put a single prefix in PATH, such that it leads to a successful execution.
# This exercises two things at the same time: (a) that
# nbd_internal_execvpe_init() produces *one* candidate (i.e., that no <colon> is
# seen), and (b) that nbd_internal_fork_safe_execvpe() succeeds for the *last*
# candidate. Repeat the test with "expr" (called "f" under "bin") and the shell
# script (called "f" under "sh", triggering the ENOEXEC branch).
run bin f 1 + 1; success bin/f,2
run sh  f arg1;  success sh/f,"sh/f arg1"

# Demonstrate that the order of candidates matters. The first invocation finds
# "expr" (called "f" under "bin"), the second invocation finds the shell script
# under "sh" (triggering the ENOEXEC branch).
run empty:bin:sh f 1 + 1; success empty/f,bin/f,sh/f,2
run empty:sh:bin f arg1;  success empty/f,sh/f,bin/f,"sh/f arg1"

# Check the expansion of zero-length prefixes in PATH to ".", plus the
# (non-)insertion of the "/" separator.
run a/:       f;       execve_fail a/f,./f                 ENOENT
run :a/       f;       execve_fail ./f,a/f                 ENOENT
run :         f;       execve_fail ./f,./f                 ENOENT
pushd bin
run :         f 1 + 1; success     ./f,./f,2
popd
run :a/:::b/: f;       execve_fail ./f,a/f,./f,./f,b/f,./f ENOENT
