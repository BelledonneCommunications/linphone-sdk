#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([belle-sip], [0.0.1], [jehan.monnier@linphone.org])
AC_CONFIG_SRCDIR([src/belle_sip_utils.c])

AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
LT_INIT(disable-static win32-dll)

if test -f /etc/debian_version ; then
	use_deb=true;
else
	use_rpm=true;
fi

AC_ARG_ENABLE(debug,
		[  --enable-debug    Turn on debug mode (default=yes)],
		[case "${enableval}" in
		yes) debug=true ;;
		no)  debug=false ;;
		*) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
		esac],[debug=true])

if test "$debug" = "no" ; then
	CFLAGS="-g -O2"
else
	CFLAGS="-g"
fi

if test x$GCC = xyes ; then
	STRICT_OPTIONS=" -Wall -Werror"
dnl because of antlr3 we must accept a few warnings...
	LESS_STRICT_OPTIONS="$STRICT_OPTIONS -Wno-error=unused-function \
	 -Wno-error=uninitialized -Wno-error=unused-variable \
	 -Wno-error=unused-but-set-variable "

fi

AC_SUBST(STRICT_OPTIONS)
AC_SUBST(LESS_STRICT_OPTIONS)

# Checks for libraries.

# Checks for header files.

found_antlr3=no
AC_CHECK_HEADERS([antlr3.h], [found_antlr3=yes])

if test x$found_antlr3 = xno ; then
	AC_MSG_ERROR([Could not find antlr3 development files. Please install antlr3 (libantlr3c-dev on debian/ubuntu systems)])
fi

found_cunit=no
AC_CHECK_HEADERS([CUnit/Basic.h], [found_cunit=yes])

if test x$found_cunit = xno ; then
	AC_MSG_WARN([Could not find cunit framework, tests are not compiled.])
fi

# Checks for typedefs, structures, and compiler characteristics.


# Checks for library functions.
AC_CHECK_LIB(rt, clock_gettime)

AM_CONDITIONAL(BUILD_TESTS,test x$found_cunit = xyes)

AC_CONFIG_FILES(
[
Makefile
include/Makefile
include/belle-sip/Makefile
src/Makefile
tester/Makefile
])
AC_OUTPUT
