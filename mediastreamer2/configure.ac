dnl Process this file with autoconf to produce a configure script.
AC_INIT([mediastreamer],[2.14.0])

AC_MSG_NOTICE([$PACKAGE_NAME-$PACKAGE_VERSION		A mediastreaming library for telephony application.])
AC_MSG_NOTICE([licensed under the terms of the General Public License (GPL)])

dnl do not put anything else before AC_PROG_CC unless checking if macro still work for clang
AC_PROG_CXX(["xcrun clang++" g++])
AC_PROG_CC(["xcrun clang" gcc])
AC_PROG_OBJC(["xcrun clang" gcc])
AC_CANONICAL_SYSTEM

dnl Source packaging numbers
MEDIASTREAMER_MAJOR_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f1)
MEDIASTREAMER_MINOR_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f2)
MEDIASTREAMER_MICRO_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f3)
MEDIASTREAMER_EXTRA_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f4)

LIBMEDIASTREAMER_SO_CURRENT=7 dnl increment this number when you add/change/remove an interface
LIBMEDIASTREAMER_SO_REVISION=0 dnl increment this number when you change source code, without changing interfaces; set to 0 when incrementing CURRENT
LIBMEDIASTREAMER_SO_AGE=0 dnl increment this number when you add an interface, set to 0 if you remove an interface

LIBMEDIASTREAMER_SO_VERSION=$LIBMEDIASTREAMER_SO_CURRENT:$LIBMEDIASTREAMER_SO_REVISION:$LIBMEDIASTREAMER_SO_AGE
MEDIASTREAMER_VERSION=${MEDIASTREAMER_MAJOR_VERSION}.${MEDIASTREAMER_MINOR_VERSION}.${MEDIASTREAMER_MICRO_VERSION}

if test -n "$MEDIASTREAMER_EXTRA_VERSION" ; then
	MEDIASTREAMER_VERSION="${MEDIASTREAMER_VERSION}.${MEDIASTREAMER_EXTRA_VERSION}"
fi

AC_SUBST(LIBMEDIASTREAMER_SO_CURRENT, $LIBMEDIASTREAMER_SO_CURRENT)
AC_SUBST(LIBMEDIASTREAMER_SO_VERSION)
AC_SUBST(MEDIASTREAMER_VERSION)

PACKAGE=mediastreamer

OS=`uname|sed -e 'y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/'`
AC_MSG_RESULT([Building Package on ${OS}])
AM_INIT_AUTOMAKE([1.9 tar-pax subdir-objects])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],)
AC_SUBST([docdir], [${datadir}/doc])
AC_CONFIG_HEADERS(mediastreamer-config.h)
AC_DEFINE_UNQUOTED(MEDIASTREAMER_MAJOR_VERSION,$MEDIASTREAMER_MAJOR_VERSION, [major version])
AC_DEFINE_UNQUOTED(MEDIASTREAMER_MINOR_VERSION,$MEDIASTREAMER_MINOR_VERSION, [minor version])
AC_DEFINE_UNQUOTED(MEDIASTREAMER_MICRO_VERSION,$MEDIASTREAMER_MICRO_VERSION, [micro version])
AC_DEFINE_UNQUOTED(MEDIASTREAMER_VERSION,"$MEDIASTREAMER_VERSION",[MEDIASTREAMER version number])

MS_PUBLIC_CFLAGS=

gl_LD_OUTPUT_DEF

AC_MSG_CHECKING([warning make an error on compilation])

AC_ARG_ENABLE(strict,
	AC_HELP_STRING([--enable-strict], [Build with stricter options @<:@yes@:>@]),
	[strictness="${enableval}"],
	[strictness=yes]
)

STRICT_OPTIONS="-Wall -Wuninitialized"
STRICT_OPTIONS_CC="-Wdeclaration-after-statement -Wstrict-prototypes"
STRICT_OPTIONS_CXX=""

#for clang

case $CC in
	*clang*)
		STRICT_OPTIONS="$STRICT_OPTIONS -Qunused-arguments -Wno-unused-function "
		#disabled due to wrong optimization false positive with small string
		#(cf. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=35903)
		STRICT_OPTIONS="$STRICT_OPTIONS -Wno-array-bounds "
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

if test "$strictness" = "yes" ; then
	STRICT_OPTIONS="$STRICT_OPTIONS -Werror -Wextra -Wno-unused-parameter -Wno-error=strict-prototypes -Wno-missing-field-initializers"
	CFLAGS="$CFLAGS -fno-strict-aliasing"
fi
AC_SUBST(STRICT_OPTIONS)
AC_SUBST(STRICT_OPTIONS_CC)
AC_SUBST(STRICT_OPTIONS_CXX)

dnl enable timestamp support
AC_ARG_ENABLE(ntp-timestamp,
	[AS_HELP_STRING([--enable-ntp-timestamp], [Turn on NTP timestamping on received packet (default=no)])],
	[case "${enableval}" in
		yes)	ntptimestamp=true;;
		no)	ntptimestamp=false;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-ntp-timestamp) ;;
	esac],
	[ntptimestamp=false]
)


AC_CONFIG_MACRO_DIR([m4])
LT_INIT([win32-dll shared disable-static])


AC_SUBST([LIBTOOL_DEPS])

if test "$GCC" != "yes" ; then
	case $target_os in
		*hpux*)
			dnl we are probably using HPUX cc compiler, so add a +O2 to CFLAGS
			CFLAGS="$CFLAGS +O2 -g "
		;;
	esac
else
	CFLAGS="$CFLAGS -Wall -fdiagnostics-show-option"
	OBJCFLAGS="$OBJCFLAGS -Wall"
	CXXFLAGS="$CXXFLAGS -Wall"
fi

AC_ARG_ENABLE([debug],
	AS_HELP_STRING([--enable-debug=@<:@yes/no@:>@],[enables the display of traces showing the execution of the library. @<:@default=yes@:>@]),
	[],
	[enable_debug=no]
)


AS_CASE([$enable_debug],
		[yes],[
			CFLAGS="$CFLAGS -g -O0 -DDEBUG"
			CXXFLAGS="$CXXFLAGS -g -O0 -DDEBUG"
			OBJCFLAGS="$OBJCFLAGS -g -O0 -DDEBUG"
		],
		[no],
		[
			case "$CFLAGS" in
				*-O*)
				;;
				*)
					CFLAGS="$CFLAGS -O2 -g"
					CXXFLAGS="$CXXFLAGS -O2 -g"
					OBJCFLAGS="$OBJCFLAGS -O2 -g"
				;;
			esac
		],
		[AC_MSG_ERROR([Bad value ($enable_debug) for --enable-debug. Valid values are yes or no.])])

AC_ARG_ENABLE(non-free-codecs,
	AC_HELP_STRING([--enable-non-free-codecs], [Include non-free codecs in build @<:@yes@:>@]),
	[non_free_codecs="${enableval}"],
	[non_free_codecs=yes]
)

if test "$non_free_codecs" = "yes" ; then
	AC_DEFINE(HAVE_NON_FREE_CODECS, 1, [whether non free codecs are allowed in the build])
else
	AC_DEFINE(HAVE_NON_FREE_CODECS, 0, [whether non free codecs are allowed in the build])
fi

dnl Checks for header files.
AC_HEADER_STDC

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME
AC_WORDS_BIGENDIAN
if test x$ac_cv_c_bigendian = xyes ; then
	CFLAGS="$CFLAGS -D_BIGENDIAN "
fi

macosx_found=no
mingw32ce_found=no
ios_found=no

dnl add thread flags
case $host in
	i386-apple*|armv6-apple*|armv7-apple*|armv7s-apple*|aarch64-apple*|arm64-apple*|*-darwin.ios)
		ios_found=yes
	;;
	x86_64-apple*|i686-apple*)
		macosx_found=yes
		OBJCFLAGS="$OBJCFLAGS -Wno-error=deprecated-declarations"
	;;
  	*mingw32ce)
		CFLAGS="$CFLAGS -D_WIN32_WINNT=0x0501  -D_WIN32_WCE  -DORTP_STATIC"
		CXXFLAGS="$CXXFLAGS -D_WIN32_WINNT=0x0501 -DORTP_STATIC  -D_WIN32_WCE"
dnl ORTP_STATIC to tell ortp not to export its variable with dllexport, as if we were building statically, or dynamically on linux
	  	LIBS="$LIBS -lws2"
		mingw_found=yes
		mingw32ce_found=yes
	;;
	*mingw*)
		dnl Workaround for mingw, whose compiler does not check in /usr/include ...
		CPPFLAGS="$CPPFLAGS -I/usr/include"
		LDFLAGS="$LDFLAGS -L/usr/lib"
		CFLAGS="$CFLAGS -D_WIN32_WINNT=0x0501 -DORTP_STATIC"
		CXXFLAGS="$CXXFLAGS -D_WIN32_WINNT=0x0501 -DORTP_STATIC"
dnl ORTP_STATIC to tell ortp not to export its variable with dllexport, as if we were building statically, or dynamically on linux
		LIBS="$LIBS -lws2_32 -lwinmm "
		mingw_found=yes
	;;
	*nto-qnx*)
	;;
	*)
		MSPLUGINS_CFLAGS="-pthread"
		MSPLUGINS_LIBS="-shared -pthread"
		CFLAGS="$CFLAGS -pthread -D_REENTRANT"
		LIBS="$LIBS -pthread -lpthread"
	;;
esac

if test "$macosx_found" == "yes" ; then
	LIBS="$LIBS -framework CoreFoundation -framework AudioToolbox -framework CoreAudio -framework AppKit"
fi
if test "$ios_found" == "yes" ; then
	LIBS="$LIBS -framework CoreFoundation -framework AudioToolbox -framework CoreAudio -framework Foundation -framework QuartzCore -framework OpenGLES -framework UIKit -framework AVFoundation"
fi

dnl localization tools
IT_PROG_INTLTOOL([0.40], [no-xml])


GETTEXT_PACKAGE="mediastreamer"
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE",[name of the gettext domain. Used in the call to 'bindtextdomain()'])
if test "$mingw_found" != "yes" ; then
	dnl gettext macro does not work properly under mingw. And we want to use the one provided by GTK.
	CPPFLAGS_save=$CPPFLAGS
	AM_GNU_GETTEXT([external])
	AC_SUBST(INTLLIBS)
	CPPFLAGS=$CPPFLAGS_save
	LIBS="$LIBS $LIBINTL"
else
	if test "$USE_NLS" = "yes" ; then
		AC_DEFINE(ENABLE_NLS,1,[Tells whether localisation is possible])
		AC_DEFINE(HAVE_GETTEXT,1,[Tells wheter localisation is possible])
		LIBS="$LIBS -L/usr/lib -lintl"
	fi
fi

AM_CONDITIONAL(BUILD_MACOSX, test x$macosx_found = xyes)
AM_CONDITIONAL(BUILD_IOS, test x$ios_found = xyes)

AC_CONFIG_COMMANDS([libtool-hacking],
	[if test "$mingw_found" = "yes" ; then
		echo "Hacking libtool to work with mingw..."
		sed -e 's/\*\" \$a_deplib \"\*/\*/' < ./libtool > libtool.tmp
		cp -f ./libtool.tmp ./libtool
		rm -f ./libtool.tmp
	else
		echo "No need to hack libtool."
	fi],
	[mingw_found=$mingw_found]
)

dnl conditionnal build of tools.
AC_ARG_ENABLE(tools,
	[AS_HELP_STRING([--enable-tools=[yes/no]], [Turn on or off compilation of console interface (default=yes)])],
	[case "${enableval}" in
		yes)	build_tools=true ;;
		no)	build_tools=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-tools) ;;
	esac],
	[build_tools=check]
)
AM_CONDITIONAL(BUILD_TOOLS, test x$build_tools != xfalse)
if test "$build_tools" != "false" ; then
	build_tools=true
	AC_DEFINE(BUILD_TOOLS, 1, [Define if tools enabled] )
fi

dnl prefer fixed point computations
AC_ARG_ENABLE(fixed_point,
	[AS_HELP_STRING([--enable-fixed-point=@<:@yes/no/guess@:>@], [Turn on fixed point computations (default=guess)])],
	[case "${enableval}" in
		yes)	fixed_point=true;;
		no)	fixed_point=false;;
		guess)	fixed_point=guess;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-fixed-point) ;;
	esac],
	[fixed_point=guess]
)

if test "$fixed_point" = "guess" ; then
	AC_MSG_CHECKING([whether fixed point arithmetic is preferred])
	case ${target_cpu}${host_cpu} in
		*bfin*)
			fixed_point=true
		;;
		*arm*)
			fixed_point=true
		;;
		*)
			fixed_point=false
		;;
	esac
	AC_MSG_RESULT([$fixed_point])
fi


if test x$fixed_point = xtrue ; then
	MS_PUBLIC_CFLAGS="$MS_PUBLIC_CFLAGS -DMS_FIXED_POINT"
fi


dnl initialize pkg-config so that we can use it within if else fi statements.
PKG_PROG_PKG_CONFIG()

AC_SUBST(MSPLUGINS_CFLAGS)
AC_SUBST(MSPLUGINS_LIBS)

dnl	*********************************
dnl	various checks for soundcard apis
dnl	*********************************

found_sound=no

if test x$mingw_found = xyes ; then
	found_sound=yes
fi

AC_ARG_ENABLE(sound,
	[AS_HELP_STRING([--disable-sound], [Turn off all possible sound backend (default=no)])],
	[case "${enableval}" in
		yes)	use_sound=true ;;
		no)	use_sound=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-sound) ;;
	esac],
	[use_sound=true]
)

AC_ARG_ENABLE(oss,
	[AS_HELP_STRING([--enable-oss], [Enable oss support])],
	[case "${enableval}" in
		yes)	oss=true ;;
		no)	oss=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-oss) ;;
	esac],
	[oss=$use_sound]
)

if "$oss" = "true"; then
	AC_CHECK_HEADERS(soundcard.h sys/soundcard.h machine/soundcard.h sys/audio.h)
	if test "${ac_cv_header_sys_soundcard_h}" = "yes" || \
	   test "${ac_cv_header_soundcard_h}" = "yes" || \
	   test "${ac_cv_header_sys_audio_h}" = "yes" || \
	   test "${ac_cv_header_machine_soundcard_h}" = "yes"; then
		found_sound=yes
	else
		oss=false
	fi
fi

AM_CONDITIONAL(BUILD_OSS, test x$oss = xtrue)

dnl conditionnal build of ALSA support
AC_ARG_ENABLE(alsa,
	[AS_HELP_STRING([--enable-alsa], [Turn on alsa native support compiling])],
	[case "${enableval}" in
		yes)	alsa=true ;;
		no)	alsa=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-alsa) ;;
	esac],
	[alsa=$use_sound]
)

if test "$alsa" = "true"; then
	AC_CHECK_HEADERS(alsa/asoundlib.h,
		[ AC_CHECK_LIB(asound,snd_pcm_open,
			[ ALSA_LIBS="-lasound"
			  found_sound=yes
			AC_DEFINE(__ALSA_ENABLED__,1,[defined if alsa support is available])
			alsa_enabled=true
			])
		]
	)
fi
AC_SUBST(ALSA_LIBS)

AM_CONDITIONAL(BUILD_ALSA, test x$alsa_enabled = xtrue)

dnl conditionnal build of QSA support
AC_ARG_ENABLE(qsa,
	[AS_HELP_STRING([--enable-qsa], [Turn on QSA (QNX Sound Architecture) support])],
	[case "${enableval}" in
		yes)	qsa=true ;;
		no)	qsa=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-qsa) ;;
	esac],
	[qsa=$false]
)
if test "$qsa" = "true"; then
	AC_CHECK_HEADERS(sys/asoundlib.h,
		[ AC_CHECK_LIB(asound, snd_pcm_open,
			[ QSA_LIBS="-lasound"
			  found_sound=yes
			AC_DEFINE(__QSA_ENABLED__, 1, [defined if QSA support is available])
			qsa_enabled=true
			])
		]
	)
fi
AC_SUBST(QSA_LIBS)
AM_CONDITIONAL(BUILD_QSA, test x$qsa_enabled = xtrue)


AC_ARG_ENABLE(artsc,
	[AS_HELP_STRING([--enable-artsc], [Turn on artsc (KDE<4) sound input/output (default=no)])],
	[case "${enableval}" in
		yes)	artsc=true ;;
		no)	artsc=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-artsc) ;;
	esac],
	[artsc=false]
)

arts_enabled=false

if test "$artsc" = "true" ; then
	dnl check for arts (kde sound daemon) support
	PKG_CHECK_MODULES(ARTS, [artsc],[
		dnl New detection
		arts_enabled=true
	],[
		dnl Old detection
		if test x$artsc = xtrue ; then
			AC_CHECK_HEADERS(kde/artsc/artsc.h,
				[ AC_CHECK_LIB(artsc,arts_init,
					[ ARTS_LIBS="-lartsc"
					arts_enabled=true
					])
				]
			)
		fi
		AC_SUBST(ARTS_LIBS)
	])
fi

if test x$arts_enabled = xtrue; then
	found_sound=yes
	AC_DEFINE(__ARTS_ENABLED__,1,[defined if arts support is available])
fi

AM_CONDITIONAL(BUILD_ARTS, test x$arts_enabled = xtrue)

AC_ARG_ENABLE(portaudio,
	[AS_HELP_STRING([--enable-portaudio], [Turn on portaudio native support compiling (default=no)])],
	[case "${enableval}" in
		yes)	portaudio=true ;;
		no)	portaudio=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-portaudio) ;;
	esac],
	[portaudio=false]
)

if test "$portaudio" = "true"; then
	AC_CHECK_HEADERS(portaudio.h,
		[ AC_CHECK_LIB(portaudio,Pa_Initialize,
			[PORTAUDIO_LIBS="-lportaudio"
			found_sound=yes
			AC_DEFINE(__PORTAUDIO_ENABLED__,1,[defined if portaudio support is available])
			portaudio_enabled=true]
			)
		]
	)
fi

AC_SUBST(PORTAUDIO_LIBS)
AM_CONDITIONAL(BUILD_PORTAUDIO, test x$portaudio_enabled = xtrue)

AC_ARG_ENABLE(macsnd,
	[AS_HELP_STRING([--enable-macsnd], [Turn on MAC OS X Audio Units sound support])],
	[case "${enableval}" in
		yes)	macsnd=true ;;
		no)	macsnd=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-macsnd) ;;
	esac],
	[macsnd=$use_sound]
)

macaqsnd_default=no
if test "$ios_found" = "yes" ; then
	macaqsnd_default=true;
fi

AC_ARG_ENABLE(macaqsnd,
	[AS_HELP_STRING([--enable-macaqsnd], [Turn on native MAC OS X Audio Queue sound support (default=no)])],
	[case "${enableval}" in
		yes)	macaqsnd=true ;;
		no)	macaqsnd=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-macaqsnd) ;;
	esac],
	[macaqsnd=$macaqsnd_default]
)

if test "$macosx_found" = "yes" ; then
	if test "$macsnd" = "true"; then
		AC_DEFINE(__MACSND_ENABLED__,1,[defined if native macosx sound support is available])
		macsnd_enabled=true
		LIBS="$LIBS -framework Carbon -framework AudioUnit"
		found_sound=yes
	fi
fi

if test "$macaqsnd" = "true"; then
	AC_DEFINE(__MAC_AQ_ENABLED__,1,[defined if native macosx AQ sound support is available])
	found_sound=yes
	macaqsnd=true
fi

AM_CONDITIONAL(BUILD_MACSND, test x$macsnd_enabled = xtrue)
AM_CONDITIONAL(BUILD_MACAQSND, test x$macaqsnd = xtrue)
AM_CONDITIONAL(BUILD_IOSIOUNIT, test x$ios_found = xyes)

AC_ARG_ENABLE(pulseaudio,
	[AS_HELP_STRING([--disable-pulseaudio], [Disable pulseaudio support])],
	[case "${enableval}" in
		yes)	pulseaudio=true ;;
		no)	pulseaudio=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-pulseaudio) ;;
	esac],
	[pulseaudio=$use_sound]
)


if test x$pulseaudio = xtrue ; then
	PKG_CHECK_MODULES(LIBPULSE, libpulse >= 0.9.21,
		[AC_DEFINE(__PULSEAUDIO_ENABLED__,1,[Pulse audio support])
		found_sound=yes],
		[pulseaudio=false]
	)
fi

AM_CONDITIONAL(BUILD_PULSEAUDIO,test x$pulseaudio = xtrue)


if test "${found_sound}${use_sound}" = "noyes"; then
	AC_MSG_ERROR([Could not find a support sound driver API. Use --disable-sound if you don't care about having sound.])
fi


dnl	*************************************
dnl	check for various codecs libraries
dnl	*************************************

AC_ARG_ENABLE(speex,
	[AS_HELP_STRING([--disable-speex], [Disable speex support])],
	[case "${enableval}" in
		yes)	speex=true ;;
		no)	speex=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-speex) ;;
	esac],
	[speex=true]
)

if test x$speex = xtrue; then
	dnl check for installed version of speex
	PKG_CHECK_MODULES(SPEEX, speex >= 1.2beta3,
		[ AC_DEFINE(HAVE_SPEEX_NOISE,1,[tells whether the noise arg of speex_echo_cancel can be used]) ],
		[try_other_speex=yes]
	)
	PKG_CHECK_MODULES(SPEEX, speex >= 1.2beta3, build_speex=yes)
	build_resample=false
	PKG_CHECK_MODULES(SPEEXDSP, speexdsp >= 1.2beta3,
		[SPEEX_LIBS="$SPEEX_LIBS $SPEEXDSP_LIBS"
		AC_DEFINE(HAVE_SPEEXDSP,1,[have speexdsp library])
		build_resample=yes],
		[AC_MSG_ERROR([No libspeexdsp library found.])]
	)
	AC_SUBST(SPEEX_CFLAGS)
	AC_SUBST(SPEEX_LIBS)
fi

AM_CONDITIONAL(BUILD_SPEEX, test x$build_speex = xyes )
AM_CONDITIONAL(BUILD_RESAMPLE, test x$build_resample = xyes )

AC_ARG_ENABLE(gsm,
	[AS_HELP_STRING([--disable-gsm], [Disable gsm support])],
	[case "${enableval}" in
		yes)	gsm=true ;;
		no)	gsm=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-gsm) ;;
	esac],
	[gsm=auto]
)

if test x$gsm != xfalse; then

dnl check for gsm
build_gsm=no
AC_ARG_WITH(gsm,
	[  --with-gsm		Sets the installation prefix of gsm codec library [default=/usr] ],
	[ gsmdir=${withval}],
	[ gsmdir=/usr ]
)

if test x"$gsmdir" != xno ; then
	test x"$gmsdir" = xyes && gsmdir=/usr
	MS_CHECK_DEP([gsm codec],[GSM],[${gsmdir}/include],
		[${gsmdir}/lib],[gsm/gsm.h],[gsm],[gsm_create])
	if test "$GSM_found" = "yes" ; then
		build_gsm=yes
	elif test "$gsm" = "true"; then
		AC_MSG_ERROR([Could not locate gsm headers or library.])
	fi
else
	build_gsm=no
fi

fi




dnl check for matroska file support
AC_ARG_ENABLE(matroska,
	[AS_HELP_STRING([--enable-matroska], [Enable mkv file support])],
	[case "${enableval}" in
		yes)	matroska=true ;;
		no)	matroska=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-matroska) ;;
	esac],
	[matroska=guess]
)
AC_ARG_WITH(ebml,
	[AS_HELP_STRING([--with-ebml],[Sets the installation prefix of ebml library (v2) [default=/usr]])],
	[ ebmldir=${withval} ],
	[ ebmldir=/usr ]
)
AC_ARG_WITH(matroska,
	[AS_HELP_STRING([--with-matroska],[Sets the installation prefix of matroska library (v2) [default=/usr]])],
	[ matroskadir=${withval} ],
	[ matroskadir=/usr ]
)

if test x$matroska != xfalse ; then

build_matroska=yes
if test x"$ebmldir" != xno ; then
	MS_CHECK_DEP([ebml library],[EBML],[${ebmldir}/include],
		[${ebmldir}/lib],[ebml/ebml.h],[ebml2],[EBML_ElementRender])

	if test "$EBML_found" = "no" ; then
		no_ebml=true
		dnl check without ebml lib, directly in matroska2 (cmake build doesn't create libebml)
		MS_CHECK_DEP([ebml library],[EBML],[${ebmldir}/include],
			[${ebmldir}/lib],[ebml/ebml.h],[matroska2],[EBML_ElementRender])
	fi
	if test "$EBML_found" = "no" ; then
		build_matroska=no
		if test "$matroska" = "true" ; then
			AC_MSG_ERROR([Could not locate ebml headers or library.])
		fi
	fi
else
	build_matroska=no
fi

if test x"$matroskadir" != xno ; then

	if test "$no_ebml" = "true" ; then
		MS_CHECK_DEP([matroska library],[MATROSKA],[${matroskadir}/include],
		[${matroskadir}/lib],[matroska/matroska.h],[matroska2],[MATROSKA_BlockReleaseData])
	else
		MS_CHECK_DEP([matroska library],[MATROSKA],[${matroskadir}/include],
		[${matroskadir}/lib],[matroska/matroska.h],[matroska2],[MATROSKA_BlockReleaseData],[-lebml2])
	fi
	if test "$MATROSKA_found" = "trueno" ; then
		build_matroska=no
		if test "$matroska" = "true" ; then
			AC_MSG_ERROR([Could not locate matroska headers or library.])
		fi
	fi
else
	build_matroska=no
fi

fi



dnl check for spandsp support to bring up g726 codec
AC_ARG_ENABLE(spandsp,
	[AS_HELP_STRING([--disable-spandsp], [Disable spandsp support])],
	[case "${enableval}" in
		yes)	spandsp=true ;;
		no)	spandsp=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-spandsp) ;;
	esac],
	[spandsp=true]
)

if test x$spandsp = xtrue; then
	dnl check for installed version of speex
	PKG_CHECK_MODULES(SPANDSP, spandsp >= 0.0.6,
		[ AC_DEFINE(HAVE_SPANDSP,1,[tells whether spandsp can be used])
		have_spandsp=true ],
		[have_spandsp=false]
	)
	AC_SUBST(SPANDSP_CFLAGS)
	AC_SUBST(SPANDSP_LIBS)
fi

dnl check for installed version of libupnp
AC_ARG_ENABLE(upnp,
	[AS_HELP_STRING([--disable-upnp], [Disable uPnP support])],
	[case "${enableval}" in
		yes)	build_upnp=true ;;
		no)	build_upnp=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-upnp) ;;
	esac],
	[build_upnp=auto]
)

if test "$build_upnp" != "false" ; then
	PKG_CHECK_MODULES([LIBUPNP], [libupnp],
		[if  pkg-config --atleast-version=1.6 "libupnp < 1.7"; then
			build_upnp=true
			old_CFLAGS="$CFLAGS"
			CFLAGS="$CFLAGS $LIBUPNP_CFLAGS -Werror -Wno-error=unused-variable"
			AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
				#include <upnp.h>
			]], [[
				struct Upnp_Action_Complete aa={0};
				char *url = aa.CtrlUrl;
			]])],,
			AC_DEFINE(USE_PATCHED_UPNP, 1, [Define if upnp is patched]))
			CFLAGS="$old_CFLAGS"
		else
			AC_MSG_ERROR([libupnp >= 1.6 < 1.5 required.])
		fi],
		[if test "$build_upnp" == "true" ; then
			AC_MSG_ERROR([libupnp not found.])
		else
			build_upnp=false
		fi]
	)

fi

dnl check for opus codec support
AC_ARG_ENABLE(opus,
	[AS_HELP_STRING([--disable-opus], [Disable opus codec support])],
	[case "${enableval}" in
		yes) opus=true ;;
		no) opus=false ;;
		*) AC_MSG_ERROR(bad value ${enableval} for --disable-opus) ;;
	esac], [opus=true])
if test x$opus = xtrue; then
PKG_CHECK_MODULES(OPUS, opus >= 0.9.0,
	[AC_DEFINE(HAVE_OPUS, 1, [Tells whether opus can be used])
	have_opus=true],
	[have_opus=false]
)
AC_SUBST(OPUS_CFLAGS)
AC_SUBST(OPUS_LIBS)
fi

AM_CONDITIONAL(BUILD_OPUS, test x$have_opus = xtrue)
AM_CONDITIONAL(BUILD_GSM, test x$build_gsm = xyes )
AM_CONDITIONAL(BUILD_MATROSKA, test x$build_matroska = xyes )
AM_CONDITIONAL(BUILD_G726, test "$have_spandsp" = "true" )

MS_CHECK_VIDEO
AM_CONDITIONAL(BUILD_VIDEO, test "$video" = "true")
AM_CONDITIONAL(BUILD_THEORA, test "$have_theora" = "yes")
AM_CONDITIONAL(BUILD_VP8, test "$have_vp8" = "yes")
AM_CONDITIONAL(BUILD_WIN32, test "$mingw_found" = "yes")
AM_CONDITIONAL(BUILD_WIN32_WCE, test "$mingw32ce_found" = "yes")
AM_CONDITIONAL(BUILD_AVCODEC, test x$avcodec_found = xyes)
AM_CONDITIONAL(BUILD_SWSCALE, test x$swscale_found = xyes)
AM_CONDITIONAL(BUILD_SDL,test "$sdl_found" = "true" )
AM_CONDITIONAL(BUILD_X11_XV, test "$enable_xv" = "true" )
AM_CONDITIONAL(BUILD_X11_GL, test "$enable_glx" = "true" )
AM_CONDITIONAL(BUILD_UPNP, test "$build_upnp" = "true" )

if test "$build_vp8" != "false" ; then
	AC_DEFINE(HAVE_VPX, 1, [Tells whether vpx can be used])
fi

dnl	*********************************************
dnl	Enable/disable oRTP dependency
dnl	*********************************************
AC_ARG_ENABLE(ortp,
	[AS_HELP_STRING([--disable-ortp], [Disable usage of the oRTP library (default=no)])],
	[case "${enableval}" in
		yes)	ortp_enabled=true ;;
		no)	ortp_enabled=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-ortp) ;;
	esac],
	[ortp_enabled=true]
)
AM_CONDITIONAL(ORTP_ENABLED, test x$ortp_enabled = xtrue)


dnl	*********************************************
dnl	setup oRTP dependency
dnl	*********************************************
AC_ARG_ENABLE(external-ortp,
	[AS_HELP_STRING([--enable-external-ortp], [Use external oRTP library (default=no)])],
	[case "${enableval}" in
		yes)	external_ortp=true ;;
		no)	external_ortp=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-external-ortp) ;;
	esac],
	[external_ortp=false]
)

if test "$ortp_enabled" = 'true'; then
	if test "$external_ortp" = 'false'; then
		if test -e ../oRTP/ortp.defs ; then
			echo "building from linphone source tree, using ../oRTP/include/ortp/ortp.h"
			ORTP_CFLAGS="-I\$(top_srcdir)/../oRTP/include"
			ORTP_DEFS=`cat ../oRTP/ortp.defs`
			ORTP_CFLAGS="$ORTP_CFLAGS $ORTP_DEFS"
			ORTP_LIBS="\$(top_builddir)/../oRTP/src/libortp.la"
			if test x$ac_cv_c_bigendian = xyes ; then
				ORTP_CFLAGS="$ORTP_CFLAGS -DORTP_BIGENDIAN"
			fi
			if test x$ntptimestamp = xtrue ; then
				ORTP_CFLAGS="$ORTP_CFLAGS -DORTP_TIMESTAMP"
			fi
		else
			external_ortp=true
		fi
	fi
	if test "$external_ortp" = 'true'; then
		PKG_CHECK_MODULES(ORTP, ortp >= 1.0.0, ,
			[ AC_MSG_ERROR([Couldn't find ortp library]) ] )
	fi
else
	AC_CHECK_HEADERS(sys/shm.h)
	ORTP_CFLAGS="-Iortp-deps"
fi
AC_SUBST(ORTP_CFLAGS)
AC_SUBST(ORTP_LIBS)


dnl	*********************************************
dnl	Enable/disable mediastreamer2 filters
dnl	*********************************************
AC_ARG_ENABLE(filters,
	[AS_HELP_STRING([--disable-filters], [Disable compilation of filters (default=no)])],
	[case "${enableval}" in
		yes)	filters=true ;;
		no)	filters=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-filters) ;;
	esac],
	[filters=true]
)
AM_CONDITIONAL(MS2_FILTERS, test x$filters = xtrue)

AM_CONDITIONAL(BUILD_VOIP_LIBRARY, test x$ortp_enabled = xtrue)


dnl check dlopen support in headers and libraries, so that we can use mediastreamer plugins
AC_CHECK_HEADERS(dlfcn.h)
have_dlopen=false
AC_CHECK_LIB(dl,dlopen,[LIBS="$LIBS -ldl"; have_dlopen=true])
AC_CHECK_FUNC(dlopen,[have_dlopen=true])
if test "$have_dlopen" = "true" ; then
	AC_DEFINE(HAVE_DLOPEN,1,[Defined if dlopen() is availlable])
fi

dnl check various things
AC_FUNC_ALLOCA
AC_ARG_ENABLE(relativeprefix,
	[AS_HELP_STRING([--enable-relativeprefix], [Build a linphone that finds its resources relatively to the directory where it is installed])],
	[case "${enableval}" in
		yes)	relativeprefix=yes ;;
		no)	relativeprefix=no ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-relativeprefix) ;;
	esac],
	[relativeprefix=guess]
)

if test "x${prefix}" = "xNONE"; then
	package_prefix=${ac_default_prefix}
else
	package_prefix=${prefix}
fi



if test x$mingw_found = xyes || test x$relativeprefix = xyes ; then
	package_prefix="."
	PACKAGE_PLUGINS_DIR="./lib/mediastreamer/plugins"
else
	PACKAGE_PLUGINS_DIR="\$(libdir)/mediastreamer/plugins"
fi

dnl define path of plugins:

AC_SUBST(PACKAGE_PLUGINS_DIR)

PACKAGE_DATA_DIR="$prefix/share"
AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR,  "${package_prefix}/share" ,[path of data])
AC_SUBST(PACKAGE_DATA_DIR)

dnl check for video4linux headers

AC_ARG_ENABLE(v4l,
	[AS_HELP_STRING([--disable-v4l], [Disable usage of video4linux, really discouraged (linux-only)])],
	[case "${enableval}" in
		yes)	v4l=yes ;;
		no)	v4l=no ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --disable-libv4l1) ;;
	esac],
	[v4l=yes]
)

if test "$v4l" = "yes" ; then
	AC_CHECK_HEADERS(linux/videodev.h linux/videodev2.h)
	if test "${ac_cv_header_linux_videodev_h}" = "yes" ; then
		found_v4l1=yes
	else
		found_v4l1=no
	fi
	if test "${ac_cv_header_linux_videodev2_h}" = "yes" ; then
		found_v4l2=yes
	else
		found_v4l2=no
	fi

	AC_ARG_ENABLE(libv4l1,
		[AS_HELP_STRING([--disable-libv4l1], [Disable usage of libv4l1, really discouraged])],
		[case "${enableval}" in
			yes)	libv4l1=yes ;;
			no)	libv4l1=no ;;
			*)	AC_MSG_ERROR(bad value ${enableval} for --disable-libv4l1) ;;
		esac],
		[libv4l1=yes]
	)

	AC_ARG_ENABLE(libv4l2,
		[AS_HELP_STRING([--disable-libv4l2], [Disable usage of libv4l2, really discouraged])],
		[case "${enableval}" in
			yes)	libv4l2=yes ;;
			no)	libv4l2=no ;;
			*)	AC_MSG_ERROR(bad value ${enableval} for --disable-libv4l2) ;;
		esac],
		[libv4l2=yes]
	)

	AS_IF([test "$libv4l2" != "no"],
	[PKG_CHECK_MODULES(LIBV4L2, libv4l2,
		[AC_DEFINE(HAVE_LIBV4L2,1,[Defined if we have libv4l2])
		have_libv4l2=yes],
		[echo "No libv4l2 found."]
	)
	])
	AS_IF([test "$libv4l1" != "no"],
	[PKG_CHECK_MODULES(LIBV4L1, libv4l1,
		[AC_DEFINE(HAVE_LIBV4L1,1,[Defined if we have libv4l1])
		have_libv4l1=yes],
		[echo "No libv4l1 found."]
	)
	])

	PKG_CHECK_MODULES(LIBV4LCONVERT, libv4lconvert,
		[AC_DEFINE(HAVE_LIBV4LCONVERT,1,[Defined if we have libv4lconvert])
		have_libv4lconvert=yes],
		[echo "No libv4lconvert found."]
	)

	if test "$found_v4l2" = "yes" && test "$have_libv4l2" != "yes" ; then
		if test "$video" = "true" && test "$libv4l2" = "yes" ; then
			AC_MSG_ERROR([
	Missing libv4l2. It is highly recommended to build with
	libv4l2 headers and library. Many camera will won't work or will crash
	your application if libv4l2 is not installed.
	If you know what you are doing, you can use --disable-libv4l2 to disable
	this check.
			])
		fi
	fi
fi
AM_CONDITIONAL(BUILD_V4L1, test x$found_v4l1 = xyes )
AM_CONDITIONAL(BUILD_V4L2, test x$found_v4l2 = xyes )

AC_ARG_ENABLE(pcap,
	[AS_HELP_STRING([--enable-pcap], [Enable pcap library (default=auto)])],
	[case "${enableval}" in
		yes)	enable_pcap=yes ;;
		no)	enable_pcap=no ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-pcap) ;;
	esac],
	[enable_pcap=auto]
)

if test x$enable_pcap != xno ; then

	AC_PATH_PROG(PCAP,pcap-config,false)
	if test $PCAP != false ; then
		PCAP_LIBS=`pcap-config --libs`
		PCAP_CFLAGS=`pcap-config --cflags`
		AC_SUBST(PCAP_LIBS)
		AC_SUBST(PCAP_CFLAGS)
		AC_DEFINE(HAVE_PCAP,1,[whether we compile with libpcap support])
		enable_pcap=yes
	else
		enable_pcap=no
	fi
fi

AM_CONDITIONAL(ENABLE_PCAP, test x$enable_pcap = xyes)


dnl ##################################################
dnl # Check for bctoolbox-tester
dnl ##################################################

PKG_CHECK_MODULES(BCTOOLBOXTESTER, bctoolbox-tester, [found_pkg_config_bctoolboxtester=yes],[found_pkg_config_bctoolboxtester=no])

case "$target_os" in
	*mingw*)
		CPPFLAGS="$CPPFLAGS -D_WIN32_WINNT=0x0501"
		LIBS="$LIBS -lws2_32 -liphlpapi"
		LDFLAGS="$LDFLAGS -Wl,--export-all-symbols"
	;;
esac

if test "$found_pkg_config_bctoolboxtester" = "no" ; then
	AC_MSG_WARN([Could not find bctoolbox-tester wrapper, tests are not compiled.])
fi


AC_ARG_ENABLE(tests,
	[AS_HELP_STRING([--enable-tests], [Enable compilation of tests (default=yes)])],
	[case "${enableval}" in
		yes)	tests=true ;;
		no)		tests=false  ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-tests) ;;
	esac],
	[tests=true]
)
AM_CONDITIONAL(BUILD_TESTS, [test x$tests = xtrue && test x$found_pkg_config_bctoolboxtester = xyes])

dnl check for libsrtp support (secure rtp)
AC_ARG_WITH(srtp,
	AC_HELP_STRING([--with-srtp], [Set prefix where libsrtp can be found or "none" to disable (ex:/usr or /usr/local) [[default=/usr]]]),
	[srtp_prefix=$withval],
	[if test "$prefix" != "NONE"; then
		srtp_prefix=$prefix
	 else
		srtp_prefix="/usr"
	 fi])

if test "${srtp_prefix}" != "none" ; then
	if test "${srtp_prefix}" != "/usr" ; then
		SRTP_CFLAGS="-I${srtp_prefix}/include -I${srtp_prefix}/include/srtp"
		SRTP_LIBS="-L${srtp_prefix}/lib"
	fi
	SRTP_LIBS="$SRTP_LIBS -lsrtp"

	dnl check srtp headers
	CPPFLAGS_save=$CPPFLAGS
	CPPFLAGS="$CPPFLAGS $SRTP_CFLAGS"
	AC_CHECK_HEADER([srtp/srtp.h],have_srtp_headers=yes)
	CPPFLAGS=$CPPFLAGS_save

	dnl check for srtp lib
	LDFLAGS_save=$LDFLAGS
	LDFLAGS="$LDFLAGS $SRTP_LIBS"
	LIBS_save=$LIBS
	AC_CHECK_LIB(srtp,[srtp_init, crypto_policy_set_aes_cm_256_hmac_sha1_80, crypto_policy_set_aes_cm_256_hmac_sha1_32], have_srtp_lib=yes)
	LDFLAGS=$LDFLAGS_save
	LIBS=$LIBS_save

	if test "$have_srtp_headers$have_srtp_lib" = "yesyes" ; then
		LDFLAGS_save=$LDFLAGS
		LIBS_save=$LIBS
		LDFLAGS="$LDFLAGS $SRTP_LIBS"
		AC_CHECK_LIB(
			srtp,
			sha1_update,[
				AC_MSG_WARN([This libsrtp version exports symbols conflicting with polarssl, resulting in a bad execution path. libsrtp will be statically linked])
				srtp_polarssl_conflict=yes
			],
			srtp_polarssl_conflict=no
		)
		LDFLAGS=$LDFLAGS_save
		LIBS=$LIBS_save

		if test "$srtp_polarssl_conflict" = "yes"; then
			srtp_static_lib="${srtp_prefix}/lib/libsrtp.a"
			if test -f $srtp_static_lib -a -r $srtp_static_lib; then
				SRTP_LIBS="${srtp_prefix}/lib/libsrtp.a -Bsymbolic -Wl,--version-script=\$(top_srcdir)/src/libsrtp.map"
				AC_DEFINE(HAVE_SRTP, 1, [Defined when srtp support is compiled])
				have_srtp=true
			else
				AC_MSG_WARN([Could not access to $srtp_static_lib. Please use --with-srtp=PREFIX to specify the prefix where libsrtp.a has been installed])
				SRTP_CFLAGS=
				SRTP_LIBS=
				have_srtp=true
			fi
		else
			AC_DEFINE(HAVE_SRTP, 1, [Defined when srtp support is compiled])
			have_srtp=true
		fi

	else
		AC_MSG_NOTICE([Could not find libsrtp headers or lib, cryto transport disabled.])
		have_srtp=false
		SRTP_CFLAGS=
		SRTP_LIBS=
	fi

	AC_SUBST(SRTP_CFLAGS)
	AC_SUBST(SRTP_LIBS)
else
	have_srtp=false
	SRTP_CFLAGS=
	SRTP_LIBS=
fi


dnl check bctoolbox library
PKG_CHECK_MODULES(BCTOOLBOX, bctoolbox >= 0.4.0, found_bctoolbox=true, found_bctoolbox=false)
if test "$found_bctoolbox" = "true" ; then
	LIBS="$LIBS $BCTOOLBOX_LIBS"
	CFLAGS="$CFLAGS $BCTOOLBOX_CFLAGS"
else
	AC_MSG_ERROR("Could not find bctoolbox")
fi

dnl check for libzrtp support (RFC 6189: Media Path Key Agreement for Unicast Secure RTP)
AC_ARG_ENABLE(zrtp,
	[AS_HELP_STRING([--enable-zrtp], [Turn on or off compilation of zrtp (default=detect)])],
	[case "${enableval}" in
		yes)	zrtp=true ;;
		no)	zrtp=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-zrtp) ;;
	esac],
	[zrtp=detect]
)


if test "$zrtp" != "false" ; then
	PKG_CHECK_MODULES(LIBBZRTP, libbzrtp >= 1.0.0, found_zrtp=true, found_zrtp=false)
	if test "$zrtp$found_zrtp" = "truefalse" ; then
		AC_MSG_ERROR("Cound not find bZRTP library.")
	fi
	if test "$found_zrtp" = "true" ; then
		if test "$have_srtp" = "false" ; then
			if test "$zrtp" = "true" ; then
				AC_MSG_ERROR("ZRTP requires SRTP")
			else
				AC_MSG_WARN("ZRTP requires SRTP")
				zrtp=false
			fi
		else
			AC_DEFINE(HAVE_ZRTP, 1, [Defined when zrtp support is compiled])
			LIBS="$LIBS $LIBBZRTP_LIBS"
			CFLAGS="$CFLAGS $LIBBZRTP_CFLAGS"
			zrtp=true
		fi
	else
		zrtp=false
	fi
fi

AM_CONDITIONAL(LIBBZRTP,test x$zrtp != xfalse)

dnl check for bcg729 annexB presence to use for VAD/DTX (RFC3389 : Real-time Transport Protocol (RTP) Payload for Comfort Noise (CN))
AC_ARG_ENABLE(g729bCN,
	[AS_HELP_STRING([--enable-g729bCN], [Turn on or off usage of G729AnnexB in RFC3389 implementation of Comfort Noise Payload (default=no)])],
	[case "${enableval}" in
		yes)	g729bCN=true ;;
		no)	g729bCN=false ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-g729bCN) ;;
	esac],
	[g729bCN=false]
)

if test "$g729bCN" = "true" ; then
	PKG_CHECK_MODULES(LIBBCG729, libbcg729 >= 1.0)

	AC_DEFINE(HAVE_G729B, 1, [Defined when use of g729AnnexB Comfort Noise and RFC3389 support is compiled])

	LIBS="$LIBS $LIBBCG729_LIBS"
	CFLAGS="$CFLAGS $LIBBCG729_CFLAGS"
else
	echo "G729 AnnexB in RFC3389 is disabled."
fi
AM_CONDITIONAL(LIBBCG729,test x$g729bCN != xfalse)

dnl ##################################################
dnl # Check for doxygen
dnl ##################################################

AC_ARG_ENABLE(documentation,
	[AS_HELP_STRING([--enable-documentation], [Documentation generation using doxygen (default=yes)])],
	[case "${enableval}" in
		yes)	documentation_enabled=yes;;
		no)		documentation_enabled=no;;
		*)		AC_MSG_ERROR("Bad value for --enable-documentation");;
	esac],
	[documentation_enabled=yes]
)
if test "$documentation_enabled" = "yes" ; then
	AC_CHECK_PROG(DOXYGEN,doxygen,doxygen,false)
else
	DOXYGEN=false
fi
AC_CHECK_PROG(DOXYGEN,doxygen,doxygen,false)
AM_CONDITIONAL(HAVE_DOXYGEN, test "$DOXYGEN" != "false")


dnl ##################################################
dnl # Check for ESP Packager
dnl ##################################################

AC_PATH_PROG(EPM,epm,false)
AC_PATH_PROG(MKEPMLIST,mkepmlist,false)
AC_PATH_PROG(EPMINSTALL,epminstall,false)
AM_CONDITIONAL(WITH_EPM,test $EPM != false && test $MKEPMLIST != false && test $EPMINSTALL != false)


dnl Check for xxd
AC_CHECK_PROG(xxd_found, xxd, yes)
if test "$xxd_found" != yes ;then
 	AC_MSG_ERROR("xxd is required (provided by vim package)")
fi

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
		EPM_PKG_EXT=depot.gz
	;;
	*-dec-osf*)
		EPM_PKG_EXT=setld
	;;
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
	i?86)
		ARCH=i386
	;;
	*)
		ARCH=$target_cpu
	;;
esac
AC_SUBST(ARCH)

# Various other packaging variables, that can be over-ridden ad `make
# package' time
SUMMARY="A mediastreaming library."
AC_SUBST(SUMMARY)
PACKAGER=anonymous
AC_SUBST(PACKAGER)
LICENSE=GPL
AC_SUBST(LICENSE)
VENDOR=Linphone
AC_SUBST(VENDOR)
RELEASE=1
AC_SUBST(RELEASE)

CFLAGS="$CFLAGS $MS_PUBLIC_CFLAGS"
CXXFLAGS="$CXXFLAGS $MS_PUBLIC_CFLAGS "
dnl: these ones gets exported in pkgconfig file.
AC_SUBST(MS_PUBLIC_CFLAGS)

AC_CONFIG_FILES(
	Makefile
	po/Makefile.in
	m4/Makefile
	include/Makefile
	include/mediastreamer2/Makefile
	src/Makefile
	tools/Makefile
	mediastreamer.pc
	mediastreamer2.spec
	help/Makefile
	help/Doxyfile
	tester/Makefile
)

AC_OUTPUT

echo "Mediastreamer2 build configuration ended."
echo "Summary of build options:"
printf "* %-30s %s\n" "Video support"		$video
printf "* %-30s %s\n" "sRTP encryption"		$have_srtp
printf "* %-30s %s\n" "zRTP encryption"		$zrtp
printf "* %-30s %s\n" "Non-free codecs" 	$non_free_codecs
printf "* %-30s %s\n" "pcap support"	 	$enable_pcap

