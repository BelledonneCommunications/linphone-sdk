# process this file with autoconf to produce a configure script.
AC_INIT([msamr],[0.0.1])

# require at least this autoconf version
AC_PREREQ(2.64)

# set up the directory structure
AC_CONFIG_SRCDIR([src/msamr.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

# initialize automake
AM_INIT_AUTOMAKE([tar-ustar])

# initialize libtool
LT_INIT([disable-static])

AC_ARG_ENABLE(strict,
[  --enable-strict       Enable error on compilation warning [default=no]],
[wall_werror=$enableval],
[
	wall_werror=yes
]
)

CFLAGS="$CFLAGS -Wall"

if test "$GCC" = "yes" && test "$wall_werror" = "yes" ; then
	CFLAGS="$CFLAGS -Werror"
fi

# check for libraries that have pkg-config files installed
PKG_CHECK_MODULES(ORTP, ortp >= 0.16.0)
PKG_CHECK_MODULES(MEDIASTREAMER, mediastreamer >= 2.0.0)
PKG_CHECK_MODULES(OPENCORE_AMRNB, opencore-amrnb >= 0.1.2)

# create the following files from their .in counterparts
AC_CONFIG_FILES([
	Makefile
	src/Makefile
])
AC_OUTPUT

