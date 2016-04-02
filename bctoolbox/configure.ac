#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])

AC_INIT([bctoolbox],[0.0.2],[jehan.monnier@linphone.org])

BCTOOLBOX_SO_CURRENT=0 dnl increment this number when you add/change/remove an interface
BCTOOLBOX_SO_REVISION=0 dnl increment this number when you change source code, without changing interfaces; set to 0 when incrementing CURRENT
BCTOOLBOX_SO_AGE=0 dnl increment this number when you add an interface, set to 0 if you remove an interface

BCTOOLBOX_SO_VERSION=$BCTOOLBOX_SO_CURRENT:$BCTOOLBOX_SO_REVISION:$BCTOOLBOX_SO_AGE

AC_SUBST(BCTOOLBOX_SO_CURRENT, $BCTOOLBOX_SO_CURRENT)
AC_SUBST(BCTOOLBOX_SO_VERSION)

AC_CONFIG_SRCDIR([src/crypto/mbedtls.c])

AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_SYSTEM

LT_INIT([shared disable-static])

dnl initialize pkg-config so that we can use it within if else fi statements.
PKG_PROG_PKG_CONFIG()

AM_INIT_AUTOMAKE()
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
dnl AC_PROG_CC_C99


CFLAGS="$CFLAGS "
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

if test "$strict" = "true" ; then
	STRICT_OPTIONS="$STRICT_OPTIONS -Werror"
fi
AC_SUBST(STRICT_OPTIONS)


# Checks for libraries.

# Checks for header files.
AC_ARG_WITH( mbedtls,
      [  --with-mbedtls      Set prefix where mbedtls can be found (ex:/usr or /usr/local)[default=PREFIX] ],
      [ mbedtls_prefix=${withval}],[ mbedtls_prefix=${prefix} ])


AC_ARG_WITH( polarssl,
      [  --with-polarssl      Set prefix where polarssl can be found (ex:/usr or /usr/local)[default=PREFIX] ],
      [ polarssl_prefix=${withval}],[ polarssl_prefix=${prefix} ])

AC_ARG_WITH( cunit,
      [  --with-cunit      Set prefix where cunit can be found (ex:/usr or /usr/local)[default=PREFIX] ],
      [ cunit_prefix=${withval}],[ cunit_prefix=${prefix} ])


if test "$mbedtls_prefix" = "NONE" ; then
	mbedtls_prefix=/usr/local
fi

if test "$polarssl_prefix" = "NONE" ; then
	polarssl_prefix=/usr/local
fi

if test "$cunit_prefix" = "NONE" ; then
	cunit_prefix=/usr/local
fi


if test "$mbedtls_prefix" != "/usr" ; then
	MBEDTLS_CFLAGS="-I${mbedtls_prefix}"
	MBEDTLS_LIBS="-L${mbedtls_prefix}/lib"
fi

if test "$cunit_prefix" != "/usr" ; then
	CUNIT_CFLAGS="-I${cunit_prefix}"
	CUNIT_LIBS="-L${cunit_prefix}/lib"
fi


CPPFLAGS_save=$CPPFLAGS
LIBS_save=$LIBS
CPPFLAGS="$CPPFLAGS $CUNIT_CFLAGS"
LIBS="$LIBS $CUNIT_LIBS"

AC_CHECK_HEADER(CUnit/CUnit.h, [
	AC_CHECK_LIB(cunit, CU_get_error, [CUNIT_LIBS="CUNIT_LIBS -lcunit"; cunit_found=true], 
	[cunit_found=false]) ],
	[cunit_found=false]
)

if test "$cunit_found" != "true" ; then
	AC_MSG_ERROR("CUnit not found")
else

	AC_CHECK_LIB(cunit, CU_get_suite, 
		[ AC_DEFINE(HAVE_CU_GET_SUITE, 1, [Defined when CU_get_suite is available])]
	)
	AC_CHECK_LIB(cunit, CU_set_trace_handler,
		[ AC_DEFINE(HAVE_CU_SET_TRACE_HANDLER, 1, [Defined when CU_set_trace_handler is available])]
	)
	AC_CHECK_HEADERS(CUnit/CUCurses.h)
fi

CPPFLAGS=$CPPFLAGS_save
LIBS=$LIBS_save
AC_SUBST(CUNIT_CFLAGS)
AC_SUBST(CUNIT_LIBS)

CPPFLAGS_save=$CPPFLAGS
LIBS_save=$LIBS
CPPFLAGS="$CPPFLAGS $MBEDTLS_CFLAGS"
LIBS="$LIBS $MBEDTLS_LIBS"

AC_CHECK_HEADER(mbedtls/ssl.h, [
	AC_CHECK_LIB(mbedtls, mbedtls_pk_parse_key, [mbedtls_found=true; MBEDTLS_LIBS="$MBEDTLS_LIBS -lmbedtls"], [mbedtls_found=false]) ],
	[mbedtls_found=false]
)

CPPFLAGS=$CPPFLAGS_save
LIBS=$LIBS_save

AM_CONDITIONAL(ENABLE_MBEDTLS, test "$mbdetls_found" = "true")

if test "$mbedtls_found" = "false" ; then

	MBEDTLS_CFLAGS=
	MBEDTLS_LIBS=
	if test "$polarssl_prefix" != "/usr" ; then
		POLARSSL_CFLAGS="-I${polarssl_prefix}"
		POLARSSL_LIBS="-L${polarssl_prefix}/lib"
	fi
	CPPFLAGS="$CPPFLAGS $POLARSSL_CFLAGS"
	LIBS="$LIBS $POLARSSL_LIBS"

	AC_CHECK_HEADER(polarssl/ssl.h, [
		AC_CHECK_LIB(polarssl, ssl_read, [polarssl_found=true; POLARSSL_LIBS="$POLARSSL_LIBS -lpolarssl"], [polarssl_found=false]) ],
			[polarssl_found=false]
	)

	if test "$polarssl_found" = "false" ; then
		AC_MSG_ERROR("Neither polarssl nor mbedtls were found.")
	fi
	AC_CHECK_HEADER(polarssl/compat-1.2.h, [polarssl_post_12_found=true], [polarssl_post_12_found=false])

	CPPFLAGS=$CPPFLAGS_save
	LIBS=$LIBS_save
fi

AC_SUBST(MBEDTLS_CFLAGS)
AC_SUBST(MBEDTLS_LIBS)
AC_SUBST(POLARSSL_CFLAGS)
AC_SUBST(POLARSSL_LIBS)


AM_CONDITIONAL(ENABLE_POLARSSL12, test "$polarssl_post_12_found" != "true")
AM_CONDITIONAL(ENABLE_POLARSSL, test "$polarssl_post_12_found" = "true")


dnl substitute variables used by .pc files

if test "$prefix" = "NONE" ; then
	install_prefix=/usr
else 
	install_prefix=$prefix
fi

PROJECT_VERSION=$VERSION
CMAKE_INSTALL_PREFIX=$install_prefix
CMAKE_INSTALL_FULL_LIBDIR=${install_prefix}/lib
CMAKE_INSTALL_FULL_INCLUDEDIR=${install_prefix}/include

AC_SUBST(PROJECT_VERSION)
AC_SUBST(CMAKE_INSTALL_PREFIX)
AC_SUBST(CMAKE_INSTALL_FULL_LIBDIR)
AC_SUBST(CMAKE_INSTALL_FULL_INCLUDEDIR)


AC_CONFIG_FILES(
[
Makefile
include/Makefile
include/bctoolbox/Makefile
src/Makefile
src/crypto/Makefile
src/tester/Makefile
bctoolbox-tester.pc
bctoolbox.pc
])
AC_OUTPUT
