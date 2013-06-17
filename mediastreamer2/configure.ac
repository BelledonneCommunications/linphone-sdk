dnl Process this file with autoconf to produce a configure script.
AC_INIT([mediastreamer],[2.9.1])

AC_MSG_NOTICE([$PACKAGE_NAME-$PACKAGE_VERSION		A mediastreaming library for telephony application.])
AC_MSG_NOTICE([licensed under the terms of the General Public License (GPL)])

AC_CANONICAL_SYSTEM

dnl Source packaging numbers
MEDIASTREAMER_MAJOR_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f1)
MEDIASTREAMER_MINOR_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f2)
MEDIASTREAMER_MICRO_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f3)
MEDIASTREAMER_EXTRA_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f4)

LIBMEDIASTREAMER_SO_CURRENT=3 dnl increment this number when you add/change/remove an interface
LIBMEDIASTREAMER_SO_REVISION=0 dnl increment this number when you change source code, without changing interfaces; set to 0 when incrementing CURRENT
LIBMEDIASTREAMER_SO_AGE=0 dnl increment this number when you add an interface, set to 0 if you remove an interface

LIBMEDIASTREAMER_SO_VERSION=$LIBMEDIASTREAMER_SO_CURRENT:$LIBMEDIASTREAMER_SO_REVISION:$LIBMEDIASTREAMER_SO_AGE
MEDIASTREAMER_VERSION=${MEDIASTREAMER_MAJOR_VERSION}.${MEDIASTREAMER_MINOR_VERSION}.${MEDIASTREAMER_MICRO_VERSION}

if test -n "$MEDIASTREAMER_EXTRA_VERSION" ; then
	MEDIASTREAMER_VERSION="${MEDIASTREAMER_VERSION}.${MEDIASTREAMER_EXTRA_VERSION}"
fi

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

AC_MSG_CHECKING([warning make an error on compilation])
AC_ARG_ENABLE(strict,
	[AS_HELP_STRING([--enable-strict], [Enable error on compilation warning (default=no)])],
	[wall_werror=$enableval],
	[wall_werror=yes]
)

CFLAGS="-DORTP_INET6 $CFLAGS "

dnl enable ipv6 support
AC_ARG_ENABLE(ipv6,
	[AS_HELP_STRING([--enable-ipv6], [Turn on IPv6 support (default=yes)])],
	[case "${enableval}" in
		yes)	ipv6=true;;
		no)	ipv6=false;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-ipv6) ;;
	esac],
	[ipv6=true]
)
if test x$ipv6 = xtrue ; then
	CFLAGS="$CFLAGS -DINET6"
fi

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

dnl Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_OBJC

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
fi

AC_ARG_ENABLE([debug],
	AS_HELP_STRING([--enable-debug=@<:@yes/no@:>@],[enables the display of traces showing the execution of the library. @<:@default=yes@:>@]),
	[],
	[enable_debug=no]
)


AS_CASE([$enable_debug],
		[yes],[
			CFLAGS="$CFLAGS -g -DDEBUG"
			CXXFLAGS="$CXXFLAGS -g -DDEBUG"
		],
		[no],
		[
			case "$CFLAGS" in
				*-O*)
				;;
				*)
					CFLAGS="$CFLAGS -O2 -g"
					CXXFLAGS="$CXXFLAGS -O2 -g"
				;;
			esac
		],
		[AC_MSG_ERROR([Bad value ($enable_debug) for --enable-debug. Valid values are yes or no.])])

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

if test $GCC = yes && test $wall_werror = yes;  then
	CFLAGS="$CFLAGS -Werror -Wno-error=deprecated-declarations"
fi

macosx_found=no
ios_found=false
mingw32ce_found=no
ios_found=no

dnl add thread flags
case $host in
	i386-apple*|armv6-apple*|armv7-apple*|armv7s-apple*)
		ios_found=yes
	;;
	x86_64-apple*|i686-apple*)
		macosx_found=yes
	;;
  	*mingw32ce)
		CFLAGS="$CFLAGS -DINET6 -DORTP_INET6 -D_WIN32_WINNT=0x0501  -D_WIN32_WCE  -DORTP_STATIC"
		CXXFLAGS="$CXXFLAGS -DINET6 -DORTP_INET6 -D_WIN32_WINNT=0x0501 -DORTP_STATIC  -D_WIN32_WCE"
dnl ORTP_STATIC to tell ortp not to export its variable with dllexport, as if we were building statically, or dynamically on linux
	  	LIBS="$LIBS -lws2"
		mingw_found=yes
		mingw32ce_found=yes
	;;
	*mingw*)
		CFLAGS="$CFLAGS -DINET6 -DORTP_INET6 -D_WIN32_WINNT=0x0501 -DORTP_STATIC"
		CXXFLAGS="$CXXFLAGS -DINET6 -DORTP_INET6 -D_WIN32_WINNT=0x0501 -DORTP_STATIC"
dnl ORTP_STATIC to tell ortp not to export its variable with dllexport, as if we were building statically, or dynamically on linux
		LIBS="$LIBS -lws2_32 -lwinmm "
		mingw_found=yes
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
	LIBS="$LIBS -framework CoreFoundation -framework AudioToolbox -framework CoreAudio "
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
	CPPFLAGS=$CPPFLAGS_save
	LIBS="$LIBS $LIBINTL"
else
	if test "$USE_NLS" = "yes" ; then
		AC_DEFINE(ENABLE_NLS,1,[Tells whether localisation is possible])
		AC_DEFINE(HAVE_GETTEXT,1,[Tells wheter localisation is possible])
		LIBS="$LIBS -lintl"
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
			CFLAGS="$CFLAGS $LIBUPNP_CFLAGS"
			AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
				#include <upnp.h>
			]], [[
				struct Upnp_Action_Complete aa;
				char *url = aa.CtrlUrl;
				url = url;	
			]])],,
			CFLAGS="$old_CFLAGS"
			AC_DEFINE(USE_PATCHED_UPNP, 1, [Define if upnp is patched]))
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
AM_CONDITIONAL(BUILD_G726, test "$have_spandsp" = "true" )

MS_CHECK_VIDEO
AM_CONDITIONAL(BUILD_VIDEO, test "$video" = "true")
AM_CONDITIONAL(BUILD_THEORA, test "$have_theora" = "yes")
AM_CONDITIONAL(BUILD_VP8, test "$have_vp8" = "yes")
AM_CONDITIONAL(BUILD_WIN32, test "$mingw_found" = "yes")
AM_CONDITIONAL(BUILD_WIN32_WCE, test "$mingw32ce_found" = "yes")
AM_CONDITIONAL(BUILD_FFMPEG, test "$ffmpeg" = "true")
AM_CONDITIONAL(BUILD_SDL,test "$sdl_found" = "true" )
AM_CONDITIONAL(BUILD_X11_XV, test "$enable_xv" = "true" )
AM_CONDITIONAL(BUILD_X11_GL, test "$enable_glx" = "true" )
AM_CONDITIONAL(BUILD_UPNP, test "$build_upnp" = "true" )

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
		if test -e $srcdir/../oRTP/include/ortp/ortp.h ; then
			echo "building from linphone source tree, using ../oRTP/include/ortp/ortp.h"
			ORTP_CFLAGS="-I\$(top_srcdir)/../oRTP/include"
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
		PKG_CHECK_MODULES(ORTP, ortp >= 0.21.0, ,
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

AM_CONDITIONAL(BUILD_V4L1, test x$found_v4l1 = xyes )
AM_CONDITIONAL(BUILD_V4L2, test x$found_v4l2 = xyes )

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

PKG_CHECK_MODULES(LIBV4L2, libv4l2,
	[AC_DEFINE(HAVE_LIBV4L2,1,[Defined if we have libv4l2])
	have_libv4l2=yes],
	[echo "No libv4l2 found."]
)
PKG_CHECK_MODULES(LIBV4L1, libv4l1,
	[AC_DEFINE(HAVE_LIBV4L1,1,[Defined if we have libv4l1])
	have_libv4l1=yes],
	[echo "No libv4l1 found."]
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


AC_PATH_PROG(PCAP,pcap-config,false)
AM_CONDITIONAL(HAVE_PCAP, test $PCAP != false)
AC_ARG_ENABLE(pcap,
	[AS_HELP_STRING([--enable-pcap], [Enable pcap library (default=no)])],
	[case "${enableval}" in
		yes)	enable_pcap=yes ;;
		no)	enable_pcap=no ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-pcap) ;;
	esac],
	[enable_pcap=no]
)
AM_CONDITIONAL(ENABLE_PCAP, test x$enable_pcap = xyes)
if test x$enable_pcap == xyes ; then
	if test $PCAP != false ; then
		PCAP_LIBS=`pcap-config --libs`
		PCAP_CFLAGS=`pcap-config --cflags`
		PCAP_CFLAGS="$PCAP_CFLAGS -DHAVE_PCAP"
		AC_SUBST(PCAP_LIBS)
		AC_SUBST(PCAP_CFLAGS)
	fi
fi


AC_ARG_ENABLE(tests,
	[AS_HELP_STRING([--enable-tests], [Enable compilation of tests (default=yes)])],
	[case "${enableval}" in
		yes)	tests=yes ;;
		no)	tests=no  ;;
		*)	AC_MSG_ERROR(bad value ${enableval} for --enable-tests) ;;
	esac],
	[tests=yes]
)
AM_CONDITIONAL(ENABLE_TESTS, test x$tests = xyes)


dnl ##################################################
dnl # Check for CUnit
dnl ##################################################

PKG_CHECK_MODULES(CUNIT, cunit, [found_cunit=yes],[found_cunit=no])

if test "$found_cunit" = "no" ; then
	AC_CHECK_HEADERS(CUnit/CUnit.h,
		[
			AC_CHECK_LIB(cunit,CU_add_suite,[
				found_cunit=yes
				CUNIT_LIBS+=" -lcunit"
			])
		])
fi
case "$target_os" in
	*darwin*)
		#hack for macport
		CUNIT_LIBS+=" -lncurses"
	;;
	*mingw*)
		CPPFLAGS="$CPPFLAGS -D_WIN32_WINNT=0x0501"
		LIBS="$LIBS -lws2_32 -liphlpapi"
		LDFLAGS="$LDFLAGS -Wl,--export-all-symbols"
	;;
esac

if test "$found_cunit" = "no" ; then
	AC_MSG_WARN([Could not find cunit framework, tests are not compiled.])
else
	AC_CHECK_LIB(cunit,CU_get_suite,[
					AC_DEFINE(HAVE_CU_GET_SUITE,1,[defined when CU_get_suite is available])
					],[foo=bar],[$CUNIT_LIBS])
	AC_CHECK_LIB(cunit,CU_curses_run_tests,[
					AC_DEFINE(HAVE_CU_CURSES,1,[defined when CU_curses_run_tests is available])
					],[foo=bar],[$CUNIT_LIBS])
fi

AM_CONDITIONAL(BUILD_TESTS,test x$found_cunit = xyes)


dnl ##################################################
dnl # Check for doxygen
dnl ##################################################

AC_PATH_PROG(DOXYGEN,doxygen,false)
AM_CONDITIONAL(HAVE_DOXYGEN, test $DOXYGEN != false)


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
	build/Makefile
	build/win32native/Makefile
	build/win32native/mediastream/Makefile
	build/win32-novideo/Makefile
	build/wince/Makefile
	build/winmob/Makefile
	mediastreamer.pc
	mediastreamer2.spec
	help/Makefile
	help/Doxyfile
	tester/Makefile
)

AC_OUTPUT
