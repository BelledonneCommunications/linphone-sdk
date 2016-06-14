#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([belr], [0.1.0], [simon.morlat@linphone.org])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE
AM_SILENT_RULES(yes)
LT_INIT(disable-static win32-dll)

# Checks for programs.
AC_PROG_CXX

CXXFLAGS="$CXXFLAGS -std=c++11 -Wall -Werror"

# Checks for libraries.

dnl ##################################################
dnl # Check for BcToolbox
dnl ##################################################

dnl check bctoolbox
PKG_CHECK_MODULES(BCTOOLBOX, [bctoolbox],[libbctoolbox_found=yes],foo=bar)
if test "$libbctoolbox_found" != "yes" ; then
        dnl Check the lib presence in case the PKG-CONFIG version is not found
        AC_LANG_CPLUSPLUS
        AC_CHECK_LIB(bctoolbox, main, [BCTOOLBOX_LIBS+=" -lbctoolbox"; libbctoolbox_found=yes], [foo=bar])
        AC_LANG_C
fi
if test "$libbctoolbox_found" != "yes" ; then
        AC_MSG_ERROR([libbctoolbox not found. Install it and try again])
fi

AC_SUBST(BCTOOLBOX_LIBS)

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_CONFIG_FILES(
Makefile
src/Makefile
tools/Makefile
include/Makefile
include/belr/Makefile
)

AC_OUTPUT
