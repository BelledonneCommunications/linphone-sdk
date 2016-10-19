dnl Process this file with autoconf to produce a configure script.
AC_INIT([msopenh264],[1.2.0])

AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE([tar-ustar])
AC_CONFIG_HEADERS(config.h)

AC_MSG_CHECKING([warning make an error on compilation])
AC_ARG_ENABLE(strict,
[  --enable-strict         Enable error on compilation warning [default=yes]],
[wall_werror=$enableval],
[wall_werror=yes]
)


AC_ARG_ENABLE(decoder,
[  --disable-decoder       Disable decoder - encoder only configuration [default=no]],
[decoder_enabled=$enableval],
[decoder_enabled=yes]
)

if test "$decoder_enabled" = "yes" ; then
	AC_DEFINE(OPENH264_DECODER_ENABLED, 1, [whether decoder is compiled])
fi

dnl Checks for programs.
AC_PROG_CXX

AC_LIBTOOL_WIN32_DLL
AC_DISABLE_STATIC
AC_PROG_LIBTOOL


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


CFLAGS="$CFLAGS -Wall"


dnl Checks for header files.
AC_HEADER_STDC

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME
AC_WORDS_BIGENDIAN
if test x$ac_cv_c_bigendian = xyes ; then
	CFLAGS="$CFLAGS -D_BIGENDIAN"
fi

if test $GCC = yes && test $wall_werror = yes;  then
	CFLAGS="$CFLAGS -Werror -Wextra "
fi

if test $GCC = yes ; then
	CFLAGS="$CFLAGS -pthread"
	LDFLAGS="$LDFLAGS -pthread"
fi

PKG_CHECK_MODULES(MEDIASTREAMER, mediastreamer >= 2.14.0)

found_openh264=no
AC_ARG_WITH(openh264,
	[AS_HELP_STRING([--with-openh264], [Sets the installation prefix of the openh264 library. (default=$prefix)])],
	[ openh264prefix=${withval}],
	[ openh264prefix=$prefix ]
)
CPPFLAGS_save=$CPPFLAGS
CPPFLAGS="$CPPFLAGS -I${openh264prefix}/include -L${openh264prefix}/lib"
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_CHECK_HEADERS([stdbool.h wels/codec_api.h],
		[ AC_CHECK_LIB(openh264, WelsCreateDecoder, [ OPENH264_LIBS="-lopenh264"; found_openh264=yes]) ],
		[],
		[[#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#endif
		]]
	)
AC_SUBST(OPENH264_LIBS)
AC_LANG_RESTORE
CPPFLAGS=$CPPFLAGS_save

if test "${found_openh264}" = "no"; then
	AC_MSG_ERROR([Could not find the openh264 library.])
fi

LDFLAGS="$LDFLAGS -rdynamic "

dnl define path of plugins:
PACKAGE_PLUGINS_DIR="\$(libdir)/mediastreamer/plugins"
AC_SUBST(PACKAGE_PLUGINS_DIR)

dnl ##################################################
dnl # Check for ESP Packager
dnl ##################################################

AC_PATH_PROG(EPM,epm,false)
AC_PATH_PROG(MKEPMLIST,mkepmlist,false)
AC_PATH_PROG(EPMINSTALL,epminstall,false)
AM_CONDITIONAL(WITH_EPM,test $EPM != false && test $MKEPMLIST != false && test $EPMINSTALL != false)


# Preferred packaging system, as per EPM terminology
case $target in
*-*-linux*)
	if test -f /etc/debian_version ; then
		EPM_PKG_EXT=deb
	else
     		EPM_PKG_EXT=rpm
     	fi
     	;;
*-hp-hpux*)
     	EPM_PKG_EXT=depot.gz;;
*-dec-osf*)
	EPM_PKG_EXT=setld;;
esac
AC_SUBST(EPM_PKG_EXT)

# System software User & Group names
case $target in
*-*-linux*)
	SYS_USER=root
	SYS_GROUP=root
	;;
*-*-hpux*|*-dec-osf*)
	SYS_USER=bin
	SYS_GROUP=bin
	;;
esac
AC_SUBST(SYS_USER)
AC_SUBST(SYS_GROUP)

# CPU Architecture
case $target_cpu in
i?86)		ARCH=i386;;
*)		ARCH=$target_cpu;;
esac
AC_SUBST(ARCH)

# Various other packaging variables, that can be over-ridden ad `make
# package' time
SUMMARY="A H264 codec mediastreamer plugin"
AC_SUBST(SUMMARY)
PACKAGER=anonymous
AC_SUBST(PACKAGER)
LICENSE=GPL
AC_SUBST(LICENSE)
VENDOR=Linphone
AC_SUBST(VENDOR)
RELEASE=1
AC_SUBST(RELEASE)


AC_OUTPUT(
Makefile
build/Makefile
build/vs/Makefile
src/Makefile
msopenh264.iss
)

