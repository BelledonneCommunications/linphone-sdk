#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([msisac],[1.0.0])
AC_PREREQ(2.63)

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_TARGET
AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE([tar-ustar --warnings=no-portability])

AC_ARG_ENABLE(strict,
[  --enable-strict       Enable error on compilation warning [default=no]],
[wall_werror=$enableval],
[wall_werror=yes]
)


CFLAGS="$CFLAGS -Wall"

if test "$wall_werror" = "yes" ; then
	STRICT_CFLAGS="-Werror"
fi

AC_SUBST(STRICT_CFLAGS)


# case $host in
# 	arm*)
# 		case $CC in
# 			*clang*) # clang doesn't like the old ARM assembly
# 				ISAC_FLAVOUR=fix
# 			;;
# 			*)
# 				ISAC_FLAVOUR=fix
# 			;;
# 		esac
# 	;;
# 	*)
# 		ISAC_FLAVOUR=fix
# 	;;
# esac

# we use fixed point implementation for speed
ISAC_FLAVOUR=fix

AC_SUBST([ISAC_FLAVOUR])
AM_CONDITIONAL(ISAC_FLAVOUR_MAIN, test $ISAC_FLAVOUR = main)

# case $host in
#         armv6-apple*)
#                 ISAC_MAKE_OPTIONS="ADDED_DEFINES+=IPHONE TOOLCHAIN_PREFIX=XXXX"
# 		CFLAGS="$CFLAGS -mno-thumb"
#         ;;
#         armv7-apple*)
#                 ISAC_MAKE_OPTIONS="USE_NEON=yes ADDED_DEFINES+=IPHONE TOOLCHAIN_PREFIX=XXXX"
# 		CFLAGS="$CFLAGS -mno-thumb"
#         ;;
#   *)
#      ;;
# esac

#AC_SUBST([ISAC_MAKE_OPTIONS])


# Checks for programs.
AC_PATH_PROG([GIT],[git])
if test -z "$GIT"; then
   AC_MSG_ERROR(['git' is required to build this package])
fi

LT_INIT([disable-static])

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
  msisac.iss
])

#  src/Makefile
#  pkgconfig/Makefile
#  pkgconfig/silk.pc
#  man/Makefile
AC_OUTPUT
