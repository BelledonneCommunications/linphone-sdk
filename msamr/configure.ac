# process this file with autoconf to produce a configure script.
AC_INIT([msamr],[1.1.2])

# require at least this autoconf version
AC_PREREQ(2.63)

AC_CANONICAL_SYSTEM

# set up the directory structure
AC_CONFIG_SRCDIR([src/msamr.c])
AC_CONFIG_MACRO_DIR([m4])

# initialize automake
AM_INIT_AUTOMAKE([tar-ustar])

# initialize libtool
LT_INIT([disable-static])
AC_LIBTOOL_WIN32_DLL

AC_CONFIG_HEADERS(config.h)


case $target_os in 
	*mingw*)
		mingw_found=yes
	;;
esac

AM_CONDITIONAL(BUILD_WIN32, test x$mingw_found = xyes)

AC_CONFIG_COMMANDS([libtool-hacking],[
if test "$mingw_found" = "yes" ; then
	AC_MSG_NOTICE([Hacking libtool to work with mingw...])
	sed -e 's/\*\" \$a_deplib \"\*/\*/' < ./libtool > libtool.tmp
	cp -f ./libtool.tmp ./libtool
	rm -f ./libtool.tmp
fi
],[mingw_found=$mingw_found])


AC_ARG_ENABLE(strict,
AC_HELP_STRING([--enable-strict],[Enable error on compilation warning [default=yes]]),
[wall_werror=$enableval],
[
	wall_werror=yes
]
)

CFLAGS="$CFLAGS -Wall"

if test "$GCC" = "yes" && test "$wall_werror" = "yes" ; then
	CFLAGS="$CFLAGS -Werror -Wextra -Wno-unused-parameter"
fi

# check for libraries that have pkg-config files installed
PKG_CHECK_MODULES(ORTP, ortp >= 0.16.0)
PKG_CHECK_MODULES(MEDIASTREAMER, mediastreamer >= 2.13.0)

AC_ARG_ENABLE(narrowband,
AC_HELP_STRING([--enable-narrowband],[Enable AMR narrowband [default=yes]]),
[enable_narrowband=$enableval],
[
       enable_narrowband=yes
       BUILD_AMRNB=1
]
)

AM_CONDITIONAL(BUILD_AMRNB, test x$enable_narrowband = xyes)
if test x$enable_narrowband = xyes; then
	PKG_CHECK_MODULES(OPENCORE_AMRNB, opencore-amrnb >= 0.1.2)
fi

AC_ARG_ENABLE(wideband,
AC_HELP_STRING([--enable-wideband],[Enable AMR wideband [default=no]]),
[enable_wideband=$enableval],
[
       enable_wideband=no
       BUILD_AMRWB=1
]
)

AM_CONDITIONAL(BUILD_AMRWB, test x$enable_wideband = xyes)
if test x$enable_wideband = xyes; then
	PKG_CHECK_MODULES(VO_AMRWBENC, vo-amrwbenc >= 0.1.1)
	PKG_CHECK_MODULES(OPENCORE_AMRWB, opencore-amrwb >= 0.1.2)
fi

# create the following files from their .in counterparts
AC_CONFIG_FILES([
	Makefile
	src/Makefile
	msamr.iss
	msamr.spec
])
AC_OUTPUT

