# nbd client library in userspace
# Copyright (C) 2013-2020 Red Hat Inc.
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

AC_INIT([libnbd],[1.3.7])

AC_CONFIG_MACRO_DIR([m4])
m4_ifdef([AC_USE_SYSTEM_EXTENSIONS],[],
         [m4_define([AC_USE_SYSTEM_EXTENSIONS],[])])
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

dnl NB: Do not [quote] this parameter.
AM_INIT_AUTOMAKE(foreign)
AC_PROG_LIBTOOL
LT_INIT

AC_CANONICAL_HOST

AC_PROG_SED

dnl Check for basic C environment.
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_CPP

AC_C_PROTOTYPES
test "x$U" != "x" && AC_MSG_ERROR([Compiler not ANSI compliant])

AM_PROG_CC_C_O

AX_PTHREAD

dnl Check for C++ (optional, we just use this to test the header
dnl can be included from C++ code).
AC_PROG_CXX

dnl The C++ compiler test is pretty useless because even if it fails
dnl it sets CXX=g++.  So test the compiler actually works.
AC_MSG_CHECKING([if the C++ compiler really really works])
AS_IF([$CXX --version >&AS_MESSAGE_LOG_FD 2>&1],[have_cxx=yes],[have_cxx=no])
AC_MSG_RESULT([$have_cxx])
AM_CONDITIONAL([HAVE_CXX], [test "$have_cxx" = "yes"])

AC_ARG_ENABLE([gcc-warnings],
    [AS_HELP_STRING([--enable-gcc-warnings],
                    [turn on lots of GCC warnings (for developers)])],
     [case $enableval in
      yes|no) ;;
      *)      AC_MSG_ERROR([bad value $enableval for gcc-warnings option]) ;;
      esac
      gcc_warnings=$enableval],
      [gcc_warnings=no]
)
if test "x$gcc_warnings" = "xyes"; then
    WARNINGS_CFLAGS="-Wall -Wvla -Werror"
    AC_SUBST([WARNINGS_CFLAGS])
fi

dnl Check for other headers, all optional.
AC_CHECK_HEADERS([\
    byteswap.h \
    endian.h \
    stdatomic.h \
    sys/endian.h])

AC_CHECK_HEADERS([linux/vm_sockets.h], [], [], [#include <sys/socket.h>])

dnl Check for sys_errlist (optional).
AC_MSG_CHECKING([for sys_errlist])
AC_TRY_LINK([], [extern int sys_errlist; char *p = &sys_errlist;], [
    AC_DEFINE([HAVE_SYS_ERRLIST], [1],
              [Define if libc has a sys_errlist variable.])
    AC_MSG_RESULT([yes])
], [
    AC_MSG_RESULT([no])
])

dnl Check for GnuTLS (optional, for TLS support).
AC_ARG_WITH([gnutls],
    [AS_HELP_STRING([--without-gnutls],
                    [disable use of gnutls @<:@default=check@:>@])],
    [],
    [with_gnutls=check])
AS_IF([test "$with_gnutls" != "no"],[
    PKG_CHECK_MODULES([GNUTLS], [gnutls >= 3.3.0], [
        AC_SUBST([GNUTLS_CFLAGS])
        AC_SUBST([GNUTLS_LIBS])
        AC_DEFINE([HAVE_GNUTLS],[1],[gnutls found at compile time.])
    ], [
        AC_MSG_WARN([gnutls not found or < 3.3.0, TLS support will be disabled.])
    ])
])
AM_CONDITIONAL([HAVE_GNUTLS], [test "x$GNUTLS_LIBS" != "x"])

AS_IF([test "$GNUTLS_LIBS" != ""],[
    AC_MSG_CHECKING([for default TLS session priority string])
    AC_ARG_WITH([tls-priority],
        [AS_HELP_STRING([--with-tls-priority],
                        [default TLS session priority string @<:@default=NORMAL@:>@])],
        [tls_priority=$withval],
        [tls_priority=NORMAL])
    AC_MSG_RESULT([$tls_priority])
    AC_DEFINE_UNQUOTED([TLS_PRIORITY],["$tls_priority"],
                       [Default TLS session priority string])

    # Check for APIs which may not be present.
    old_LIBS="$LIBS"
    LIBS="$GNUTLS_LIBS $LIBS"
    AC_CHECK_FUNCS([\
	gnutls_session_set_verify_cert])
    LIBS="$old_LIBS"
])

dnl certtool (part of GnuTLS) for testing TLS with certificates.
AC_CHECK_PROG([CERTTOOL], [certtool], [certtool])
AM_CONDITIONAL([HAVE_CERTTOOL], [test "x$CERTTOOL" != "x"])

dnl psktool (part of GnuTLS) for testing Pre-Shared Keys (PSK).
AC_CHECK_PROG([PSKTOOL], [psktool], [psktool])
AM_CONDITIONAL([HAVE_PSKTOOL], [test "x$PSKTOOL" != "x"])

dnl Check for libxml2 (optional, for NBD URI support)
AC_ARG_WITH([libxml2],
    [AS_HELP_STRING([--without-libxml2],
                    [disable use of libxml2 for URI support @<:@default=check@:>@])],
    [],
    [with_libxml2=check])
AS_IF([test "$with_libxml2" != "no"],[
    PKG_CHECK_MODULES([LIBXML2], [libxml-2.0], [
        AC_SUBST([LIBXML2_CFLAGS])
        AC_SUBST([LIBXML2_LIBS])
        AC_DEFINE([HAVE_LIBXML2],[1],[libxml2 found at compile time.])
    ], [
        AC_MSG_WARN([libxml2 not found, NBD URI support will be disabled.])
    ])
])

dnl nbdkit, basic plugins and nbdkit-sh-plugin are only needed to run
dnl the test suite.
nbdkit_plugins="memory null pattern sh"
nbdkit_min_minor="12" ;# 1.12
AC_CHECK_PROG([NBDKIT], [nbdkit], [nbdkit])
AS_IF([test "x$NBDKIT" != "x"], [
    have_nbdkit_features=yes

    AC_MSG_CHECKING([for nbdkit >= 1.$nbdkit_min_minor])
    nbdkit_minor="$(
        $NBDKIT --version | $SED 's/^nbdkit 1\.\(@<:@0-9@:>@*\)\..*/\1/'
    )"
    AS_IF([test $nbdkit_minor -ge $nbdkit_min_minor],[
        AC_MSG_RESULT([yes (1.$nbdkit_minor)])
    ],[
        AC_MSG_RESULT([no (1.$nbdkit_minor)])
        AC_MSG_WARN([nbdkit is too old, some tests will be disabled])
        have_nbdkit_features=no
    ])

    for p in $nbdkit_plugins; do
        AC_MSG_CHECKING([for nbdkit $p plugin])
        AS_IF([$NBDKIT $p --version >&AS_MESSAGE_LOG_FD 2>&1], [
            AC_MSG_RESULT([yes])
        ],[
            AC_MSG_RESULT([no])
            AC_MSG_WARN([nbdkit $p plugin is missing, some tests will be disabled])
            have_nbdkit_features=no
        ])
    done
])
AM_CONDITIONAL([HAVE_NBDKIT],
    [test "x$NBDKIT" != "x" && test "x$have_nbdkit_features" = "xyes"])

dnl nbd-server and qemu-nbd for interop testing.
AC_CHECK_PROG([NBD_SERVER], [nbd-server], [nbd-server])
AM_CONDITIONAL([HAVE_NBD_SERVER], [test "x$NBD_SERVER" != "x"])
AC_CHECK_PROG([QEMU_NBD], [qemu-nbd], [qemu-nbd])
AM_CONDITIONAL([HAVE_QEMU_NBD], [test "x$QEMU_NBD" != "x"])

dnl glib2 main loop for examples that interoperate with the glib main loop.
PKG_CHECK_MODULES([GLIB], [glib-2.0], [
    AC_SUBST([GLIB_CFLAGS])
    AC_SUBST([GLIB_LIBS])
],[
    AC_MSG_WARN([glib2 not found, some examples will not be compiled])
])
AM_CONDITIONAL([HAVE_GLIB], [test "x$GLIB_LIBS" != "x"])

dnl FUSE is optional to build the FUSE module.
AC_ARG_ENABLE([fuse],
    AS_HELP_STRING([--disable-fuse], [disable FUSE (guestmount) support]),
    [],
    [enable_fuse=yes])
AS_IF([test "x$enable_fuse" != "xno"],[
    PKG_CHECK_MODULES([FUSE],[fuse],[
        AC_SUBST([FUSE_CFLAGS])
        AC_SUBST([FUSE_LIBS])
        AC_DEFINE([HAVE_FUSE],[1],[Define to 1 if you have FUSE.])

        dnl OpenBSD doesn't have this header, but can still compile
        dnl FUSE modules anyhow.  The symbol we need is fuse_chan_fd
        dnl which OpenBSD defines in regular fuse.h.
        old_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS $FUSE_CFLAGS"
        AC_CHECK_HEADERS([fuse_lowlevel.h])
        CFLAGS="$old_CFLAGS"
    ],[
        enable_fuse=no
        AC_MSG_WARN([FUSE library and headers are missing, so optional FUSE module won't be built])
    ])
])
AM_CONDITIONAL([HAVE_FUSE],[test "x$enable_fuse" != "xno"])

dnl Check we have enough to run podwrapper.
AC_CHECK_PROG([PERL],[perl],[perl],[no])
AS_IF([test "x$PERL" != "xno"],[
    AC_MSG_CHECKING([if we have perl Pod::Man and Pod::Simple])
    AS_IF([$PERL -MPod::Man -MPod::Simple -e 1 >&AS_MESSAGE_LOG_FD 2>&1],[
        enable_pod=yes
    ],[
        enable_pod=no
    ])
    AC_MSG_RESULT([$enable_pod])
])
AM_CONDITIONAL([HAVE_POD],
               [test "x$PERL" != "xno" && test "x$enable_pod" = "xyes"])

dnl Define the path to the podwrapper program.
PODWRAPPER="$PERL $(pwd)/podwrapper.pl"
AC_SUBST([PODWRAPPER])

dnl Build the libFuzzer test.  See fuzzing/README.
AC_ARG_ENABLE([libfuzzer],
    [AS_HELP_STRING([--enable-libfuzzer],
                    [build the libFuzzer test (developers only)])],
    [enable_libfuzzer=yes],
    [enable_libfuzzer=no])
AM_CONDITIONAL([ENABLE_LIBFUZZER],[test "x$enable_libfuzzer" = "xyes"])

dnl Bash completion.
PKG_CHECK_MODULES([BASH_COMPLETION], [bash-completion >= 2.0], [
    bash_completion=yes
    AC_MSG_CHECKING([for bash-completions directory])
    m4_ifdef([PKG_CHECK_VAR],[
        PKG_CHECK_VAR(bashcompdir, [bash-completion], [completionsdir])
    ])
    AS_IF([test -z "$bashcompdir"], [
        bashcompdir="${sysconfdir}/bash_completion.d"
    ])
    AC_MSG_RESULT([$bashcompdir])
    AC_SUBST([bashcompdir])
],[
    bash_completion=no
    AC_MSG_WARN([bash-completion not installed])
])
AM_CONDITIONAL([HAVE_BASH_COMPLETION],[test "x$bash_completion" = "xyes"])

dnl OCaml, required for running the generator when building from git,
dnl optional the rest of the time unless you want to build the OCaml
dnl bindings.
AC_PROG_OCAML
AC_PROG_FINDLIB

dnl Flags we want to pass to every OCaml compiler call.
OCAML_WARN_ERROR="-warn-error CDEFLMPSUVYZX+52-3"
AC_SUBST([OCAML_WARN_ERROR])
OCAML_FLAGS="-g -annot -safe-string"
AC_SUBST([OCAML_FLAGS])

dnl Note this only disables the OCaml bindings.
AC_ARG_ENABLE([ocaml],
    AS_HELP_STRING([--disable-ocaml], [disable OCaml language bindings]),
    [],
    [enable_ocaml=yes])
AM_CONDITIONAL([HAVE_OCAML],
    [test "x$OCAMLC" != "xno" && test "x$OCAMLFIND" != "xno" && \
     test "x$enable_ocaml" = "xyes"])
AM_CONDITIONAL([HAVE_OCAMLOPT],
    [test "x$OCAMLOPT" != "xno" && test "x$OCAMLFIND" != "xno" && \
     test "x$enable_ocaml" = "xyes"])

dnl Check if OCaml has caml_alloc_custom_mem (added late 2018).
AS_IF([test "x$OCAMLC" != "xno" && test "x$OCAMLFIND" != "xno" && \
       test "x$enable_ocaml" = "xyes"],[
    AC_MSG_CHECKING([for caml_alloc_custom_mem])
    cat >conftest.c <<'EOF'
#include <caml/custom.h>
int main () { char *p = (void *) caml_alloc_custom_mem; return 0; }
EOF
    AS_IF([$OCAMLC conftest.c >&AS_MESSAGE_LOG_FD 2>&1],[
        AC_MSG_RESULT([yes])
        AC_DEFINE([HAVE_CAML_ALLOC_CUSTOM_MEM],[1],
                  [caml_alloc_custom_mem found at compile time.])
    ],[
        AC_MSG_RESULT([no])
    ])
    rm -f conftest.c conftest.o
])

dnl Check if OCaml has caml_alloc_initialized_string (added 2017).
AS_IF([test "x$OCAMLC" != "xno" && test "x$OCAMLFIND" != "xno" && \
       test "x$enable_ocaml" = "xyes"],[
    AC_MSG_CHECKING([for caml_alloc_initialized_string])
    cat >conftest.c <<'EOF'
#include <caml/alloc.h>
int main () { char *p = (void *) caml_alloc_initialized_string; return 0; }
EOF
    AS_IF([$OCAMLC conftest.c >&AS_MESSAGE_LOG_FD 2>&1],[
        AC_MSG_RESULT([yes])
        AC_DEFINE([HAVE_CAML_ALLOC_INITIALIZED_STRING],[1],
                  [caml_alloc_initialized_string found at compile time.])
    ],[
        AC_MSG_RESULT([no])
    ])
    rm -f conftest.c conftest.o
])

dnl Python, optional for the Python bindings and shell.
PYTHON_PREFIX=
PYTHON_VERSION=
PYTHON_INSTALLDIR=

AC_ARG_ENABLE([python],
    AS_HELP_STRING([--disable-python], [disable Python 3 language bindings]),
    [],
    [enable_python=yes])
AS_IF([test "x$enable_python" != "xno"],[
    AC_CHECK_PROG([PYTHON],[python3],[python3],[no])

    if test "x$PYTHON" != "xno"; then
	AC_MSG_CHECKING([Python version])
        PYTHON_VERSION_MAJOR=`$PYTHON -c "import sys; print (sys.version_info@<:@0@:>@)"`
        PYTHON_VERSION_MINOR=`$PYTHON -c "import sys; print (sys.version_info@<:@1@:>@)"`
        PYTHON_VERSION="$PYTHON_VERSION_MAJOR.$PYTHON_VERSION_MINOR"
	AC_MSG_RESULT([$PYTHON_VERSION])

        # Debian: python-3.2.pc
        PKG_CHECK_MODULES([PYTHON], [python-"$PYTHON_VERSION"],[
            have_python_module=1
            AC_SUBST([PYTHON_CFLAGS])
            AC_SUBST([PYTHON_LIBS])
            AC_SUBST([PYTHON_VERSION])
            AC_DEFINE([HAVE_PYTHON],[1],[Python library found at compile time])
        ],[
            PKG_CHECK_MODULES([PYTHON], [python],[
                have_python_module=1
                AC_SUBST([PYTHON_CFLAGS])
                AC_SUBST([PYTHON_LIBS])
                AC_SUBST([PYTHON_VERSION])
                AC_DEFINE([HAVE_PYTHON],[1],[Python library found at compile time])
            ],[
                AC_MSG_WARN([python $PYTHON_VERSION not found])
            ])
        ])

        AC_MSG_CHECKING([Python prefix])
        PYTHON_PREFIX=`$PYTHON -c "import sys; print (sys.prefix)"`
        AC_MSG_RESULT([$PYTHON_PREFIX])

        AC_ARG_WITH([python-installdir],
                    [AS_HELP_STRING([--with-python-installdir],
	                [directory to install python modules @<:@default=check@:>@])],
			[PYTHON_INSTALLDIR="$withval"
	                 AC_MSG_NOTICE([Python install dir $PYTHON_INSTALLDIR])],
			[PYTHON_INSTALLDIR=check])

        if test "x$PYTHON_INSTALLDIR" = "xcheck"; then
	    PYTHON_INSTALLDIR=
            AC_MSG_CHECKING([for Python site-packages path])
            if test -z "$PYTHON_INSTALLDIR"; then
                PYTHON_INSTALLDIR=`$PYTHON -c "import distutils.sysconfig; \
                                               print (distutils.sysconfig.get_python_lib(1,0));"`
            fi
            AC_MSG_RESULT([$PYTHON_INSTALLDIR])
        fi

        AC_MSG_CHECKING([for Python extension suffix (PEP-3149)])
        if test -z "$PYTHON_EXT_SUFFIX"; then
            python_ext_suffix=`$PYTHON -c "import distutils.sysconfig; \
                                         print (distutils.sysconfig.get_config_var('EXT_SUFFIX') or distutils.sysconfig.get_config_var('SO'))"`
            PYTHON_EXT_SUFFIX=$python_ext_suffix
        fi
        AC_MSG_RESULT([$PYTHON_EXT_SUFFIX])
    fi

    AC_SUBST(PYTHON_PREFIX)
    AC_SUBST(PYTHON_VERSION)
    AC_SUBST(PYTHON_INSTALLDIR)
    AC_SUBST(PYTHON_EXT_SUFFIX)
])
AM_CONDITIONAL([HAVE_PYTHON],
    [test "x$PYTHON" != "xno" && test "x$have_python_module" = "x1" ])

dnl Golang.
AC_ARG_ENABLE([golang],
    AS_HELP_STRING([--disable-golang], [disable Go language bindings]),
        [],
        [enable_golang=yes])
AS_IF([test "x$enable_golang" != "xno"],[
    AC_CHECK_PROG([GOLANG],[go],[go],[no])
    AS_IF([test "x$GOLANG" != "xno"],[
        AC_MSG_CHECKING([if $GOLANG is usable])
        AS_IF([$GOLANG run $srcdir/golang/config-test.go 2>&AS_MESSAGE_LOG_FD],[
            AC_MSG_RESULT([yes])

            # Substitute some golang environment.
            GOOS=`$GOLANG env GOOS`
            GOARCH=`$GOLANG env GOARCH`
            GOROOT=`$GOLANG env GOROOT`
            AC_SUBST([GOOS])
            AC_SUBST([GOARCH])
            AC_SUBST([GOROOT])
        ],[
            AC_MSG_RESULT([no])
            AC_MSG_WARN([golang ($GOLANG) is installed but not usable])
            GOLANG=no
        ])
    ])
],[GOLANG=no])
AM_CONDITIONAL([HAVE_GOLANG],[test "x$GOLANG" != "xno"])

dnl Produce output files.
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([podwrapper.pl],
                [chmod +x,-w podwrapper.pl])
AC_CONFIG_FILES([python/run-python-tests],
                [chmod +x,-w python/run-python-tests])
AC_CONFIG_FILES([run],
                [chmod +x,-w run])
AC_CONFIG_FILES([sh/nbdsh],
                [chmod +x,-w sh/nbdsh])

AC_CONFIG_FILES([Makefile
                 bash/Makefile
                 common/include/Makefile
                 docs/Makefile
                 examples/Makefile
                 fuse/Makefile
                 fuzzing/Makefile
                 generator/Makefile
                 golang/Makefile
                 golang/examples/Makefile
                 include/Makefile
                 interop/Makefile
                 lib/Makefile
                 lib/libnbd.pc
                 lib/local/libnbd.pc
                 ocaml/Makefile
                 ocaml/META
                 ocaml/examples/Makefile
                 ocaml/tests/Makefile
                 python/Makefile
                 sh/Makefile
                 tests/Makefile
                 tests/functions.sh
                 valgrind/Makefile])

AC_OUTPUT

dnl Summary.
echo
echo
echo "----------------------------------------------------------------------"
echo "Thank you for downloading $PACKAGE_STRING"
echo
echo "This is how we have configured the optional components for you today:"
echo

feature ()
{
    printf %s "    $1"
    shift
    if $@; then echo "yes"; else echo "no"; fi
}

echo "Optional library features:"
echo
feature "TLS support ............................ " \
        test "x$HAVE_GNUTLS_TRUE" = "x"
feature "NBD URI support ........................ " \
        test "x$LIBXML2_LIBS" != "x"
feature "FUSE support ........................... " \
        test "x$HAVE_FUSE_TRUE" = "x"
feature "Manual pages ........................... " \
        test "x$HAVE_POD_TRUE" = "x"
feature "Bash tab completion .................... " \
        test "x$HAVE_BASH_COMPLETION_TRUE" = "x"

echo
echo "Language bindings:"
echo
feature "Go ..................................... " \
        test "x$HAVE_GOLANG_TRUE" = "x"
feature "OCaml .................................. " \
        test "x$HAVE_OCAML_TRUE" = "x"
feature "Python ................................. " \
        test "x$HAVE_PYTHON_TRUE" = "x"


echo
echo "If any optional component is configured ‘no’ when you expected ‘yes’"
echo "then you should check the preceding messages and README."
echo
echo "Please report bugs back to the mailing list:"
echo "http://www.redhat.com/mailman/listinfo/libguestfs"
echo
echo "Next you should type 'make' to build the package,"
echo "then 'make check' to run the tests."
