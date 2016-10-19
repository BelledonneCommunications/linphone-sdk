#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])

AC_INIT([belle-sip],[1.5.0],[jehan.monnier@linphone.org])

BELLESIP_SO_CURRENT=0 dnl increment this number when you add/change/remove an interface
BELLESIP_SO_REVISION=0 dnl increment this number when you change source code, without changing interfaces; set to 0 when incrementing CURRENT
BELLESIP_SO_AGE=0 dnl increment this number when you add an interface, set to 0 if you remove an interface

BELLESIP_SO_VERSION=$BELLESIP_SO_CURRENT:$BELLESIP_SO_REVISION:$BELLESIP_SO_AGE

AC_SUBST(BELLESIP_SO_CURRENT, $BELLESIP_SO_CURRENT)
AC_SUBST(BELLESIP_SO_VERSION)

AC_CONFIG_SRCDIR([src/belle_sip_utils.c])

AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_SYSTEM

dnl initialize pkg-config so that we can use it within if else fi statements.
PKG_PROG_PKG_CONFIG()

AM_INIT_AUTOMAKE([subdir-objects])
AM_SILENT_RULES(yes)

# Checks for programs.
dnl do not put anythingelse before AC_PROG_CC unless checking if macro still work for clang
dnl because of tunnel library wrapper, C++ is required.
AC_PROG_CXX(["xcrun clang++" g++])
AC_PROG_CC(["xcrun clang" gcc])
AC_PROG_OBJC(["xcrun clang" gcc])

AM_PROG_CC_C_O
case $INSTALL in
	*ginstall*)
		INSTALL="$INSTALL -C"
	;;
esac
AC_PROG_CC_C99
LT_INIT(win32-dll)

case "$target" in
	*-apple-darwin.ios|i386-apple*|armv6-apple*|armv7-apple*|armv7s-apple*|arm64-apple*|aarch64-apple*)
		LIBS="$LIBS -framework Foundation -framework CoreFoundation -framework CFNetwork -framework UIKit"
		build_apple=yes
	;;
	#macosx 64 bits
	x86_64-apple-darwin*)
		LIBS="$LIBS -framework Foundation"
		OBJCFLAGS="$OBJCFLAGS  -fmodules"
		build_apple=yes
	;;
esac

AM_CONDITIONAL([BUILD_APPLE], [test "x$build_apple" = "xyes"])

dnl Workaround for mingw, whose compiler does not check in /usr/include ...
case "$target_os" in
	*mingw*)
		if test "$cross_compiling" != "yes"; then
			if test "$prefix" = "/usr" ; then
				CPPFLAGS="$CPPFLAGS -I/usr/include"
				LDFLAGS="$LDFLAGS -L/usr/lib"
			fi
		fi
	;;
esac


if test -f /etc/debian_version ; then
	use_deb=true;
else
	use_rpm=true;
fi

AC_ARG_ENABLE(debug,
		[  --enable-debug    Turn on debug mode (default=no)],
		[case "${enableval}" in
		yes) debug=true ;;
		no)  debug=false ;;
		*) AC_MSG_ERROR(bad value ${enableval} for --enable-debug) ;;
		esac],[debug=false])

CFLAGS="$CFLAGS -fms-extensions"
if test "$debug" = "no" ; then
	CFLAGS="$CFLAGS -g -O2"
else
	CFLAGS="$CFLAGS -g"
fi

AC_ARG_ENABLE(strict,
		[  --enable-strict    Turn on strict mode compilation, no warnings allowed (default=yes)],
		[case "${enableval}" in
		yes) strict=true ;;
		no)  strict=false ;;
		*) AC_MSG_ERROR(bad value ${enableval} for --enable-strict) ;;
		esac],[strict=true])

STRICT_OPTIONS="-Wall"
STRICT_OPTIONS_CC="-Wdeclaration-after-statement -Wstrict-prototypes"
STRICT_OPTIONS_CXX=""

case "$CC" in
	*clang*)
	STRICT_OPTIONS="$STRICT_OPTIONS -Wno-error=unknown-warning-option -Qunused-arguments -Wno-tautological-compare -Wno-builtin-requires-header -Wno-unused-function -Wno-gnu-designator "
	#disabled due to wrong optimization false positive with small string
	#(cf. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=35903)
	STRICT_OPTIONS="$STRICT_OPTIONS -Wno-array-bounds "
	;;
	gcc*)
	STRICT_OPTIONS="$STRICT_OPTIONS -Wno-error=pragmas"
	;;
esac

# because Darwin's gcc is actually clang, we need to check it...
case "$target_os" in
	*darwin*)
	STRICT_OPTIONS="$STRICT_OPTIONS -Wno-error=unknown-warning-option -Qunused-arguments -Wno-tautological-compare -Wno-unused-function "
	#disabled due to wrong optimization false positive with small string
	#(cf. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=35903)
	STRICT_OPTIONS="$STRICT_OPTIONS -Wno-array-bounds "
	;;
esac

if test "$strict" = "true"; then
	STRICT_OPTIONS="$STRICT_OPTIONS -Werror -Wextra -Wno-unused-parameter -Wno-error=unknown-pragmas -Wuninitialized -Wno-error=strict-prototypes -Wno-missing-field-initializers"
fi

dnl because of antlr3 we must accept a few warnings...
dnl more portable for the moment
LESS_STRICT_OPTIONS="-Wno-error=sign-compare"

AC_SUBST(STRICT_OPTIONS)
AC_SUBST(STRICT_OPTIONS_CC)
AC_SUBST(STRICT_OPTIONS_CXX)
AC_SUBST(LESS_STRICT_OPTIONS)

# Checks for libraries.

# Checks for header files.
AC_ARG_WITH( antlr,
      [  --with-antlr      Set prefix where libantlr3c can be found (ex:/usr or /usr/local)[default=PREFIX] ],
      [ antlr_prefix=${withval}],[ antlr_prefix=${prefix} ])

found_antlr3=no
if test "$antlr_prefix" != "NONE" && test "$antlr_prefix" != "/usr" ; then
	ANTLR_CFLAGS="-I${antlr_prefix}/include"
	ANTLR_LIBS="-L${antlr_prefix}/lib"
fi

ANTLR_LIBS="$ANTLR_LIBS -lantlr3c"

dnl check antlr headers
CPPFLAGS_save=$CPPFLAGS
CPPFLAGS="$ANTLR_CFLAGS $CPPFLAGS"
AC_CHECK_HEADERS([antlr3.h], [found_antlr3=yes])
AC_CHECK_DECL([antlr3StringStreamNew],
		[AC_DEFINE(HAVE_ANTLR_STRING_STREAM_NEW,1,[Defined when antlr 3.4 api is detected])],
		[foo=bar],
		[#include <antlr3.h>])
libresolv_have_res_get_servers=no
resolv_h_hav_res_ndestroy=no
LIBS_save=$LIBS

LIBS="$LIBS -lresolv"

AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <resolv.h>]],
                      			[[res_getservers(NULL,NULL,0)]])]
								,[libresolv_have_res_get_servers=yes
									AC_MSG_NOTICE([res_getservers usable])]
								,[LIBS=$LIBS_save
									AC_MSG_WARN([res_getservers not usable])])

AC_CHECK_DECL([res_ndestroy],
		[resolv_h_hav_res_ndestroy=yes],
		[foo=bar],
		[#include <resolv.h>])
if test "${libresolv_have_res_get_servers}${resolv_h_hav_res_ndestroy}" == "yesyes" ; then
	AC_DEFINE(HAVE_RESINIT,1,[Defined when res_ninit api is available])
fi
CPPFLAGS=$CPPFLAGS_save

if test "$found_antlr3" != "yes" ; then
	AC_MSG_ERROR([Could not find antlr3 development files. Please install antlr3 version > 3.2 (libantlr3c-dev on debian/ubuntu systems)])
	ANTLR_CFLAGS=
	ANTLR_LIBS=
fi
AC_PATH_PROG([ANTLR],[antlr3],[no],[$antlr_prefix/bin /usr/bin])

if test $ANTLR = "no" ; then
antlr_java_prefixes="$antlr_prefix/share/java /usr/local/share/java /usr/share/java /opt/local/share/java"

	for antlr_java_prefix in $antlr_java_prefixes
	do
		antlr_jar=$antlr_java_prefix/antlr.jar
		if test -f $antlr_jar ; then
			break
		else
			antlr_jar=no
		fi
		antlr_jar=$antlr_java_prefix/antlr3.jar
		if test -f $antlr_jar ; then
			break
		else
			antlr_jar=no
		fi
	done
	if test $antlr_jar = "no" ; then
		AC_MSG_ERROR([Could not find antlr.jar. Please install antlr3 ])
	fi
	AC_PATH_PROG([JAVA],[java],[no])
	if test $JAVA = "no" ; then
		AC_MSG_ERROR([Could not find java prog. Please install java ])
	else
		ANTLR="$JAVA -Xmx256m -jar $antlr_jar"
	fi
fi
AC_SUBST(ANTLR_CFLAGS)
AC_SUBST(ANTLR_LIBS)

AC_ARG_ENABLE(	tls,
		[  --enable-tls		Enable TLS support (default=yes)],
		[case "${enableval}" in
		yes) use_tls=true ;;
		no)  use_tls=false ;;
		*) AC_MSG_ERROR(bad value ${enableval} for --enable-tls) ;;
		esac],[use_tls=true])

PKG_CHECK_MODULES(BCTOOLBOX, bctoolbox, [found_bctoolbox=yes],[found_bctoolbox=no])
if test "x$found_bctoolbox" = "xyes" ; then
	TLS_CFLAGS=$BCTOOLBOX_CFLAGS
	TLS_LIBS=$BCTOOLBOX_LIBS
else
	AC_MSG_ERROR(["Could not find bctoolbox (required dependency)"])
fi
AC_SUBST(TLS_CFLAGS)
AC_SUBST(TLS_LIBS)
AC_SUBST(TLS_PC)

AC_ARG_ENABLE(tunnel,
	[AS_HELP_STRING([--enable-tunnel], [Enable tunnel support (default=no)])])
if test "$enable_tunnel" = "yes" ; then
	PKG_CHECK_MODULES(TUNNEL, tunnel, [found_tunnel=yes], [found_tunnel=no])
	if test "$found_tunnel" = "yes" ; then
		AC_DEFINE(HAVE_TUNNEL, 1, [Defined when tunnel is enabled and available])
	fi
fi
AM_CONDITIONAL(BUILD_TUNNEL, test "$found_tunnel" = "yes")

PKG_CHECK_MODULES(BCTOOLBOXTESTER, bctoolbox-tester, [found_pkg_config_bctoolboxtester=yes],[found_pkg_config_bctoolboxtester=no])

case "$target_os" in
	*mingw*)
		LIBBELLESIP_CFLAGS="-DBELLESIP_EXPORTS"
		CPPFLAGS="$CPPFLAGS -D_WIN32_WINNT=0x0501"
		LIBS="$LIBS -lws2_32 -liphlpapi"
		LDFLAGS="$LDFLAGS -Wl,--export-all-symbols"
	;;
esac
AC_SUBST(LIBBELLESIP_CFLAGS)

if test "$found_pkg_config_bctoolboxtester" = "no" ; then
	AC_MSG_WARN([Could not find bctoolbox tester wrapper, tests are not compiled.])
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

dnl check zlib
AC_ARG_ENABLE(zlib,
	[AS_HELP_STRING([--disable-zlib], [Disable ZLib support])],
	[case "${enableval}" in
		yes)	build_zlib=true ;;
		no)	build_zlib=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-zlib) ;;
	esac],
	[build_zlib=auto]
)
if test "$build_zlib" != "false" ; then
	PKG_CHECK_MODULES(ZLIB, [zlib], [found_zlib=yes], [found_zlib=no])
	if test "x$found_zlib" = "xno" ; then
		AC_CHECK_LIB(z, inflate,
			[AC_CHECK_HEADER([zlib.h],
			[AC_COMPILE_IFELSE(
				[AC_LANG_PROGRAM([[
				 #include <zlib.h>
				 #if !defined(ZLIB_VERNUM) || (ZLIB_VERNUM < 0x1230)
				 // compile error
				 #endif
			]],[])],
			[found_zlib=yes])])])
		if test "x$found_zlib" = "xno" ; then
			AC_MSG_NOTICE([zlib library and headers not found])
		else
			AC_DEFINE( HAVE_ZLIB, 1, [ZLIB support] )
			ZLIB_LIBS='-lz'
			AC_SUBST(ZLIB_LIBS)
		fi
	else
		AC_MSG_NOTICE([ZLIB found])
		AC_DEFINE( HAVE_ZLIB, 1, [ZLIB support] )
	fi
fi

LIBS_PRIVATE="$LIBS_PRIVATE $ANTLR_LIBS $POLARSSL_LIBS"
AC_SUBST(LIBS_PRIVATE)

REQUIRES_PRIVATE=""
AC_SUBST(REQUIRES_PRIVATE)

# Checks for typedefs, structures, and compiler characteristics.


# Eliminate -lstdc++ addition to postdeps for cross compiles.
postdeps_CXX=`echo " $postdeps_CXX " | sed 's, -lstdc++ ,,g'`


# Checks for library functions.
AC_CHECK_LIB(rt, clock_gettime)
AC_CHECK_LIB(dl, dlopen)
AC_CHECK_LIB(pthread, pthread_getspecific,,
    [AC_MSG_ERROR([pthread library not found])])

AC_CONFIG_FILES(
[
Makefile
include/Makefile
include/belle-sip/Makefile
src/Makefile
src/grammars/Makefile
tester/Makefile
belle-sip.pc
belle-sip.spec
])
AC_OUTPUT
