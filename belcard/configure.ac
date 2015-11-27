#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([belcard], [0.0.1], [sylvain.berfini@belledonne-communications.com])
AC_CONFIG_SRCDIR([src/])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE
AM_SILENT_RULES(yes)
LT_INIT(disable-static win32-dll)

# Checks for programs.
AC_PROG_CXX

CXXFLAGS="$CXXFLAGS -std=c++11 -Wall -Werror"

# Checks for libraries.
dnl Check for xxd
AC_CHECK_PROG(xxd_found, xxd, yes)
if test "$xxd_found" != yes ;then
        AC_MSG_ERROR("xxd is required (provided by vim package)")
fi


# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_CONFIG_FILES(
Makefile
src/Makefile
tests/Makefile
include/Makefile
include/belcard/Makefile
)

AC_OUTPUT
