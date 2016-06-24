#   -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.



AC_INIT([bzrtp],[1.0.3])
AC_PREREQ(2.63)
AC_CONFIG_SRCDIR([src/bzrtp.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([tar-ustar --warnings=no-portability])
AC_PROG_CC(["xcrun clang" gcc])
LT_INIT(win32-dll shared disable-static)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_ARG_ENABLE(strict,
[  --enable-strict       Enable error on compilation warning [default=yes]],
[wall_werror=$enableval],
[wall_werror=yes]
)

# configure option to disable the tests
PKG_CHECK_MODULES(BCTOOLBOXTESTER, bctoolbox-tester, [found_pkg_config_bctoolboxtester=yes],[found_pkg_config_bctoolboxtester=no])
if test "$found_pkg_config_bctoolboxtester" = "no" ; then
	AC_MSG_WARN([Could not find bctoolbox-tester wrapper, tests are not compiled.])
fi

dnl check bctoolbox
PKG_CHECK_MODULES(BCTOOLBOX, [bctoolbox] ,[bctoolbox_found=yes] ,foo=bar)
if test "$bctoolbox_found" != "yes" ; then
	AC_MSG_ERROR([bctoolbox not found, aborting. ])
fi

dnl check libxml2
PKG_CHECK_MODULES(LIBXML2, [libxml-2.0] ,[libxml2_found=yes] ,foo=bar)
if test "$libxml2_found" != "yes" ; then
	AC_MSG_WARN([libxml2 not found. Disabling cache.])
else
	AC_DEFINE(HAVE_LIBXML2,1,[defined when libxml2 is available])
fi

AC_ARG_ENABLE(tests,
        [AS_HELP_STRING([--disable-tests], [Disable compilation of tests])],
        [case "${enableval}" in
                yes)    tests_enabled=true ;;
                no)     tests_enabled=false ;;
                *)      AC_MSG_ERROR(bad value ${enableval} for --disable-tests) ;;
        esac],
        [tests_enabled=yes]
)
AM_CONDITIONAL(ENABLE_TESTS, test x$tests_enabled = xyes && test x$found_pkg_config_bctoolboxtester = xyes)

CFLAGS="$CFLAGS -Wall"

case $CC in
        *clang*)
                CFLAGS="$CFLAGS -Qunused-arguments"
        ;;
esac

if test $GCC = yes && test $wall_werror = yes;  then
	CFLAGS="$CFLAGS -Werror "
fi

# Create the following files from their .in counterparts
AC_CONFIG_FILES([
  Makefile
  src/Makefile
  include/Makefile
  include/bzrtp/Makefile
  test/Makefile
  libbzrtp.pc
])

AC_OUTPUT
