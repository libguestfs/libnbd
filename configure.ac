# nbd client library in userspace
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

AC_INIT([libnbd],[0.1.2])

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
    WARNINGS_CFLAGS="-Wall -Werror"
    AC_SUBST([WARNINGS_CFLAGS])
fi

dnl Check for GnuTLS (optional, for TLS support).
PKG_CHECK_MODULES([GNUTLS], [gnutls >= 3.3.0], [
    AC_SUBST([GNUTLS_CFLAGS])
    AC_SUBST([GNUTLS_LIBS])
    AC_DEFINE([HAVE_GNUTLS],[1],[gnutls found at compile time.])
], [
    AC_MSG_WARN([gnutls not found or < 3.3.0, TLS support will be disabled.])
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
PKG_CHECK_MODULES([LIBXML2], [libxml-2.0], [
    AC_SUBST([LIBXML2_CFLAGS])
    AC_SUBST([LIBXML2_LIBS])
    AC_DEFINE([HAVE_LIBXML2],[1],[libxml2 found at compile time.])
], [
    AC_MSG_WARN([libxml2 not found, NBD URI support will be disabled.])
])
AM_CONDITIONAL([HAVE_LIBXML2], [test "x$LIBXML2_LIBS" != "x"])

dnl nbdkit, nbdkit-sh-plugin and development headers are only needed
dnl to run the test suite.
AC_CHECK_PROG([NBDKIT], [nbdkit], [nbdkit])
AS_IF([test "x$NBDKIT" != "x"], [
    PKG_CHECK_MODULES([NBDKIT], [nbdkit >= 1.12], [
        AC_SUBST([NBDKIT_CFLAGS])
        AC_SUBST([NBDKIT_LIBS])
        have_nbdkit_devel=yes
        ], [
        AC_MSG_WARN([nbdkit development package is not installed, tests will be disabled])
    ])

    AC_MSG_CHECKING([for nbdkit-sh-plugin])
    AS_IF([$NBDKIT sh --version >&AS_MESSAGE_LOG_FD 2>&1], [
        AC_MSG_RESULT([yes])
        have_nbdkit_sh_plugin=yes
    ],[
        AC_MSG_RESULT([no])
]   )
])
AM_CONDITIONAL([HAVE_NBDKIT], [test "x$NBDKIT" != "x" && test "x$have_nbdkit_devel" = "xyes" && test "x$have_nbdkit_sh_plugin" = "xyes"])

dnl nbd-server and qemu-nbd for interop testing.
AC_CHECK_PROG([NBD_SERVER], [nbd-server], [nbd-server])
AM_CONDITIONAL([HAVE_NBD_SERVER], [test "x$NBD_SERVER" != "x"])
AC_CHECK_PROG([QEMU_NBD], [qemu-nbd], [qemu-nbd])
AM_CONDITIONAL([HAVE_QEMU_NBD], [test "x$QEMU_NBD" != "x"])

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

dnl OCaml, required for running the generator when building from git,
dnl optional the rest of the time unless you want to build the OCaml
dnl bindings.
AC_PROG_OCAML
AC_PROG_FINDLIB
AM_CONDITIONAL([HAVE_OCAML_TOPLEVEL],[test "x$OCAML" != "xno"])
AM_CONDITIONAL([HAVE_OCAML],
               [test "x$OCAMLC" != "xno" && test "x$OCAMLFIND" != "xno"])
AM_CONDITIONAL([HAVE_OCAMLOPT],
               [test "x$OCAMLOPT" != "xno" && test "x$OCAMLFIND" != "xno"])

dnl Flags we want to pass to every OCaml compiler call.
OCAML_WARN_ERROR="-warn-error CDEFLMPSUVYZX+52-3"
AC_SUBST([OCAML_WARN_ERROR])
OCAML_FLAGS="-g -annot -safe-string"
AC_SUBST([OCAML_FLAGS])

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
            AC_SUBST([PYTHON_CFLAGS])
            AC_SUBST([PYTHON_LIBS])
            AC_SUBST([PYTHON_VERSION])
            AC_DEFINE([HAVE_PYTHON],[1],[Python library found at compile time])
        ],[
            PKG_CHECK_MODULES([PYTHON], [python],[
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
    [test "x$PYTHON" != "xno" && test "x$PYTHON_LIBS" != "x" ])

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
                 docs/Makefile
                 examples/Makefile
                 generator/Makefile
                 include/Makefile
                 interop/Makefile
                 lib/Makefile
                 lib/libnbd.pc
                 ocaml/Makefile
                 ocaml/META
                 ocaml/examples/Makefile
                 python/Makefile
                 sh/Makefile
                 tests/Makefile
                 tests/functions.sh
                 valgrind/Makefile])

AC_OUTPUT
