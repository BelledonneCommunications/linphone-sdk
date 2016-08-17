#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([mswebrtc],[1.0.0])
AC_PREREQ(2.63)

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_TARGET
AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE([tar-ustar --warnings=no-portability])

AC_CONFIG_HEADERS(config.h)

AC_ARG_ENABLE(strict,
[  --enable-strict       Enable error on compilation warning [default=no]],
[wall_werror=$enableval],
[wall_werror=yes]
)

dnl enable minimum optimization and debugging by default.
if test "$CFLAGS" = "" ; then
	CFLAGS="-O2 -g"
fi

CFLAGS="$CFLAGS -Wall"

if test "$wall_werror" = "yes" ; then
	STRICT_CFLAGS="-Werror"
fi

AC_SUBST(STRICT_CFLAGS)

# we use fixed point implementation for speed
ISAC_FLAVOUR=fix

AC_SUBST([ISAC_FLAVOUR])
AM_CONDITIONAL(ISAC_FLAVOUR_MAIN, test $ISAC_FLAVOUR = main)

# check for libraries that have pkg-config files installed
PKG_CHECK_MODULES(MEDIASTREAMER, mediastreamer >= 2.0.0)

# Checks for libraries.
# AC_CHECK_LIB([m],[cosf])

# initialize libtool
AC_LIBTOOL_WIN32_DLL
LT_INIT([disable-static])

# enable to debug cross-compilation
# AC_MSG_NOTICE([Target OS: $target_os])

case $target_os in 
	*mingw*)
		mingw_found=yes
		CFLAGS="$CFLAGS -DWEBRTC_WIN"
	;;
	*linux*)
		CFLAGS="$CFLAGS -DWEBRTC_LINUX -DWEBRTC_THREAD_RR"
	;;
	*darwin*)
		CFLAGS="$CFLAGS -DWEBRTC_MAC -DWEBRTC_THREAD_RR -DWEBRTC_CLOCK_TYPE_REALTIME"
	;;
	*android*)
		CFLAGS="$CFLAGS -DWEBRTC_LINUX -DWEBRTC_ANDROID -DWEBRTC_THREAD_RR -DWEBRTC_CLOCK_TYPE_REALTIME"
	;;
esac

AM_CONDITIONAL(BUILD_WIN32, test x$mingw_found = xyes)


AC_ARG_ENABLE(isac,
	[AS_HELP_STRING([--disable-isac], [Turn off isac audio codec support (default=no)])],
	[case "${enableval}" in
		yes)	build_isac=true ;;
		no)	build_isac=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-isac) ;;
	esac],
	[build_isac=true]
)
AM_CONDITIONAL(BUILD_ISAC, test x$build_isac = xtrue)
if test x${build_isac} = xtrue; then
	AC_DEFINE(BUILD_ISAC, 1, [Defined iSAC support is built])
fi

AC_ARG_ENABLE(aec,
	[AS_HELP_STRING([--disable-aec], [Turn off WebRTC echo canceller support (default=no)])],
	[case "${enableval}" in
		yes)	build_aec=true ;;
		no)	build_aec=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-aec) ;;
	esac],
	[build_aec=true]
)
AM_CONDITIONAL(BUILD_AEC, test x$build_aec = xtrue)
if test x${build_aec} = xtrue; then
	AC_DEFINE(BUILD_AEC, 1, [Defined AEC support is built])
fi

if test "${build_isac}${build_aec}" = "falsefalse"; then
	AC_MSG_ERROR([You need to enable at least one feature of WebRTC.])
fi


AC_CONFIG_COMMANDS([libtool-hacking],[
if test "$mingw_found" = "yes" ; then
	AC_MSG_NOTICE([Hacking libtool to work with mingw...])
	sed -e 's/\*\" \$a_deplib \"\*/\*/' < ./libtool > libtool.tmp
	cp -f ./libtool.tmp ./libtool
	rm -f ./libtool.tmp
fi
],[mingw_found=$mingw_found])

# Checks for library functions.

# Create the following files from their .in counterparts
AC_CONFIG_FILES([
  Makefile
  mswebrtc.iss
])

AC_OUTPUT

echo "Summary of build options:"
printf "* %-30s %s\n" "iSAC" $build_isac
printf "* %-30s %s\n" "AEC" $build_aec
