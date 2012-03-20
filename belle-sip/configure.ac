#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([belle-sip], [0.0.1], [jehan.monnier@linphone.org])
AC_CONFIG_SRCDIR([src/belle_sip_utils.c])

AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE
AM_SILENT_RULES(yes)

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
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

STRICT_OPTIONS=" -Wall -Werror"
dnl because of antlr3 we must accept a few warnings...
       LESS_STRICT_OPTIONS= #more portable
#       LESS_STRICT_OPTIONS="$STRICT_OPTIONS -Wno-error=unused-function \
#        -Wno-error=uninitialized -Wno-error=unused-variable \
#        -Wno-error=unused-but-set-variable "

AC_SUBST(STRICT_OPTIONS)
AC_SUBST(LESS_STRICT_OPTIONS)

# Checks for libraries.

# Checks for header files.
AC_ARG_WITH( antlr,
      [  --with-antlr      Set prefix where libantlr can be found or "none" to disable (ex:/usr or /usr/local)[default=/usr] ],
      [ antlr_prefix=${withval}],[ antlr_prefix=/usr ])

found_antlr3=no
ANTLR_CFLAGS="-I${antlr_prefix}/include"
ANTLR_LIBS="-L${antlr_prefix}/lib -lantlr3c"

dnl check antlr headers
CPPFLAGS_save=$CPPFLAGS
CPPFLAGS="$CPPFLAGS $ANTLR_CFLAGS"
AC_CHECK_HEADERS([antlr3.h], [found_antlr3=yes])
CPPFLAGS=$CPPFLAGS_save

if test "$found_antlr3" != "yes" ; then
	AC_MSG_ERROR([Could not find antlr3 development files. Please install antlr3 (libantlr3c-dev on debian/ubuntu systems)])
	ANTLR_CFLAGS=
	ANTLR_LIBS=
fi
AC_PATH_PROG([ANTLR],[antlr3],[no],[$antlr_prefix/bin])

if test $ANTLR = "no" ; then
antlr_java_prefixes="$antlr_prefix/share/java /usr/local/share/java /usr/share/java /opt/local/share/java"

	for antlr_java_prefix in $antlr_java_prefixes
	do
		antlr_jar=$antlr_java_prefix/antlr.jar
		AC_CHECK_FILE([$antlr_jar],[break],[antlr_jar=no])
	done
	if test $antlr_jar = "no" ; then
		AC_MSG_ERROR([Could not find antlr.jar. Please install antlr3 ])
	fi
	AC_PATH_PROG([JAVA],[java])
	if test $JAVA = "no" ; then
		AC_MSG_ERROR([Could not find java prog. Please install java ])
	else	
		ANTLR="$JAVA -jar $antlr_jar"
	fi
fi
AC_SUBST(ANTLR_CFLAGS)
AC_SUBST(ANTLR_LIBS)

PKG_CHECK_MODULES(GNUTLS, gnutls, [found_gnutls=yes],foo=bar)
PKG_CHECK_MODULES(OPENSSL, openssl, [found_openssl=yes],foo=bar)

AM_CONDITIONAL([BUILD_TLS], [test "x$found_gnutls" = "xyes" || test "x$found_openssl" = "xyes"])
TLS_CFLAGS=""
TLS_LIBS=""
if test "x$found_gnutls" = "xyes" ; then
	GNUTLS_LIBS+="$GNUTLS_LIBS -lgnutls-openssl "
	AC_DEFINE(HAVE_GNUTLS,1,[Defined when gnutls api is available])
	TLS_CFLAGS=$GNUTLS_CFLAGS
	TLS_LIBS=$GNUTLS_LIBS
elif test "x$found_openssl" = "xyes" ; then
	AC_DEFINE(HAVE_OPENSSL,1,[Defined when openssl api is available])
	TLS_CFLAGS=$OPENSSL_CFLAGS
	TLS_LIBS=$OPENSSL_LIBS
fi
AC_SUBST(TLS_CFLAGS)
AC_SUBST(TLS_LIBS)	

PKG_CHECK_MODULES(CUNIT, cunit, [found_cunit=yes],[found_cunit=no])

if test "$found_cunit" = "no" ; then
	AC_CHECK_HEADERS(CUnit/CUnit.h,
		[
			found_cunit=yes
			CUNIT_LIBS="-lcunit"
		])
fi


case "$target_os" in
	*darwin*)
		#hack for macport
		CUNIT_LIBS+=" -lncurses"
	;;
esac


AC_SUBST(CUNIT_CFLAGS)
AC_SUBST(CUNIT_LIBS)

if test "$found_cunit" = "no" ; then
	AC_MSG_WARN([Could not find cunit framework, tests are not compiled.])
fi

AM_CONDITIONAL(BUILD_TESTS,test x$found_cunit = xyes)
# Checks for typedefs, structures, and compiler characteristics.


# Checks for library functions.
AC_CHECK_LIB(rt, clock_gettime)
AC_CHECK_LIB(dl, dlopen)


AC_CONFIG_FILES(
[
Makefile
include/Makefile
include/belle-sip/Makefile
src/Makefile
tester/Makefile
])
AC_OUTPUT
