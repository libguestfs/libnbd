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

AC_INIT([libnbd],[0.1])

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

dnl Produce output files.
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([run],
                [chmod +x,-w run])

AC_CONFIG_FILES([Makefile
                 examples/Makefile
                 include/Makefile
                 lib/Makefile
                 tests/Makefile])

AC_OUTPUT
