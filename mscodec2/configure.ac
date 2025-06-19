#   -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.



AC_INIT([mscodec2],[0.1])
AC_CANONICAL_SYSTEM
AC_PREREQ(2.63)
AC_CONFIG_SRCDIR([mscodec2.c])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([tar-ustar --warnings=no-portability])
AC_PROG_LIBTOOL
AC_PROG_CC
AC_CONFIG_HEADERS(config.h)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_ARG_ENABLE(strict,
	[  --enable-strict       Enable error on compilation warning [default=yes]],
	[wall_werror=$enableval],
	[wall_werror=yes]
)

CFLAGS="$CFLAGS -Wall"

if test $GCC = yes && test $wall_werror = yes;  then
	CFLAGS="$CFLAGS -Werror "
fi

dnl check for libraries that have pkg-config files installed
PKG_CHECK_MODULES(ORTP, ortp >= 0.24.2,[found_ortp=true],foo=bar)
PKG_CHECK_MODULES(MEDIASTREAMER, mediastreamer >= 2.11.2,[found_ms2=true],foo=bar)

dnl Check for lib codec2
AC_ARG_WITH( codec2,
      [  --with-codec2      Set prefix where lib codec2 can be found (ex:/usr, /usr/local)[[default=PREFIX or /usr if NONE]] ],
      [ codec2_prefix=${withval}],[ if test "$prefix" != "NONE"; then
					codec2_prefix=${prefix}
				      else
					codec2_prefix="/usr"
				      fi ])

found_codec2=no

if test "$codec2_prefix" != "none" ; then
	if test "$codec2_prefix" != "/usr" ; then
		CODEC2_CFLAGS="-I${codec2_prefix}/include"
		CODEC2_LIBS="-L${codec2_prefix}/lib"
	fi
	CODEC2_LIBS="$CODEC2_LIBS -lcodec2"

	CPPFLAGS_save=$CPPFLAGS
	LIBS_save=$LIBS

	CPPFLAGS="$CPPFLAGS $CODEC2_CFLAGS"
	LIBS="$LIBS $CODEC2_LIBS"
	AC_CHECK_HEADERS(codec2/codec2.h,
				[found_codec2=yes; AC_MSG_NOTICE([codec2 usable])],
				[CODEC2_CFLAGS=""
					CODEC2_LIBS=""])

	CPPFLAGS=$CPPFLAGS_save
	LIBS=$LIBS_save
fi

if test "x$found_codec2" != "xyes" ; then
	AC_MSG_ERROR("Requires lib codec2")
fi

AC_SUBST(ORTP_CFLAGS)
AC_SUBST(ORTP_LIBS)
AC_SUBST(MEDIASTREAMER_CFLAGS)
AC_SUBST(MEDIASTREAMER_LIBS)
AC_SUBST(CODEC2_CFLAGS)
AC_SUBST(CODEC2_LIBS)

VISIBILITY_CFLAGS=
case "$target_os" in
	*mingw*)
		CFLAGS="$CFLAGS -D_WIN32_WINNT=0x0501"
		mingw_found=yes
	;;
	*)
		VISIBILITY_CFLAGS="-fvisibility=hidden"
	;;
esac
AC_SUBST(VISIBILITY_CFLAGS)

dnl Initialize libtool
LT_INIT([win32-dll shared ])

AC_CONFIG_COMMANDS([libtool-hacking],
	[if test "$mingw_found" = "yes" ; then
		echo "Hacking libtool to work with mingw..."
		sed -e 's/\*\" \$a_deplib \"\*/\*/' < ./libtool > libtool.tmp
		cp -f ./libtool.tmp ./libtool
		rm -f ./libtool.tmp
	fi],
	[mingw_found=$mingw_found]
)

dnl Create the following files from their .in counterparts
AC_CONFIG_FILES([
	Makefile
])


AC_OUTPUT
