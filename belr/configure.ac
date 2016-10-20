#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([belr], [0.1.1], [simon.morlat@linphone.org])

BELR_SO_CURRENT=0 dnl increment this number when you add/change/remove an interface
BELR_SO_REVISION=1 dnl increment this number when you change source code, without changing interfaces; set to 0 when incrementing CURRENT
BELR_SO_AGE=1 dnl increment this number when you add an interface, set to 0 if you remove an interface

BELR_SO_VERSION=$BELR_SO_CURRENT:$BELR_SO_REVISION:$BELR_SO_AGE

AC_SUBST(BELR_SO_CURRENT, $BELR_SO_CURRENT)
AC_SUBST(BELR_SO_VERSION)

AC_CONFIG_SRCDIR([src/])

AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_SYSTEM

LT_INIT([shared disable-static])

dnl initialize pkg-config so that we can use it within if else fi statements.
PKG_PROG_PKG_CONFIG()

AM_INIT_AUTOMAKE(subdir-objects)
AM_SILENT_RULES(yes)

# Checks for programs.
AC_PROG_CXX

CXXFLAGS="$CXXFLAGS -std=c++11 -Wall -Werror -Wextra -Wno-unused-parameter"

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

dnl substitute variables used by .pc files
if test "$prefix" = "NONE" ; then
	install_prefix=/usr
else
	install_prefix=$prefix
fi

PROJECT_VERSION=$VERSION
CMAKE_INSTALL_PREFIX=$install_prefix
CMAKE_INSTALL_FULL_LIBDIR=${libdir}
CMAKE_INSTALL_FULL_INCLUDEDIR=${install_prefix}/include

AC_SUBST(PROJECT_VERSION)
AC_SUBST(CMAKE_INSTALL_PREFIX)
AC_SUBST(CMAKE_INSTALL_FULL_LIBDIR)
AC_SUBST(CMAKE_INSTALL_FULL_INCLUDEDIR)
AC_SUBST(TESTER_REQUIRES_PRIVATE)

# Checks for library functions.
AC_CONFIG_FILES(
Makefile
src/Makefile
tools/Makefile
include/Makefile
include/belr/Makefile
belr.pc
cmake/BelrConfig.cmake
)

AC_OUTPUT
