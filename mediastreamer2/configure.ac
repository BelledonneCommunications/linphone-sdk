dnl Process this file with autoconf to produce a configure script.
AC_INIT([mediastreamer],[2.8.2])

AC_MSG_NOTICE([$PACKAGE_NAME-$PACKAGE_VERSION		A mediastreaming library for telephony application.])
AC_MSG_NOTICE([licensed under the terms of the General Public License (GPL)])

AC_CANONICAL_SYSTEM

dnl Source packaging numbers
MEDIASTREAMER_MAJOR_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f1)
MEDIASTREAMER_MINOR_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f2)
MEDIASTREAMER_MICRO_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f3)
MEDIASTREAMER_EXTRA_VERSION=$(echo $PACKAGE_VERSION | cut -d. -f4)

LIBMEDIASTREAMER_SO_CURRENT=1 dnl increment this number when you add/change/remove an interface
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

AM_INIT_AUTOMAKE([tar-ustar])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],)
AC_CONFIG_HEADERS(mediastreamer-config.h)
AC_DEFINE_UNQUOTED(MEDIASTREAMER_MAJOR_VERSION,$MEDIASTREAMER_MAJOR_VERSION, [major version])
AC_DEFINE_UNQUOTED(MEDIASTREAMER_MINOR_VERSION,$MEDIASTREAMER_MINOR_VERSION, [minor version])
AC_DEFINE_UNQUOTED(MEDIASTREAMER_MICRO_VERSION,$MEDIASTREAMER_MICRO_VERSION, [micro version])
AC_DEFINE_UNQUOTED(MEDIASTREAMER_VERSION,"$MEDIASTREAMER_VERSION",[MEDIASTREAMER version number])

MS_PUBLIC_CFLAGS=

AC_SUBST([mkdir_p])

AC_MSG_CHECKING([warning make an error on compilation])
AC_ARG_ENABLE(strict,
[  --enable-strict       Enable error on compilation warning [default=no]],
[wall_werror=$enableval],
[
	if test "$USER" = "smorlat" ; then
		wall_werror=yes
	else
		wall_werror=no
	fi
]
)

CFLAGS="-DORTP_INET6 $CFLAGS "

dnl enable ipv6 support
AC_ARG_ENABLE(ipv6,
      [  --enable-ipv6    Turn on ipv6 support],
      [case "${enableval}" in
        yes)  ipv6=true;;
        no)   ipv6=false;;
        *) AC_MSG_ERROR(bad value ${enableval} for --enable-ipv6) ;;
      esac],[ipv6=true])
if test x$ipv6 = xtrue ; then
	CFLAGS="$CFLAGS -DINET6"
fi


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
	CFLAGS="$CFLAGS -Wall"
fi

AC_ARG_ENABLE([debug],
			AS_HELP_STRING([--enable-debug=@<:@yes/no@:>@],[enables the display of traces showing the execution of the library. @<:@default=yes@:>@]),
			[],											dnl [action-if-given]
			[enable_debug=no])							dnl [action-if-not-given]
 

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
  	i386-apple*)
		ios_found=yes
        ;;
        armv6-apple*)
		ios_found=yes
        ;;
        armv7-apple*)
		ios_found=yes
        ;;
        x86_64-apple*)
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
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,["mediastreamer"],[name of the gettext domain. Used in the call to 'bindtextdomain()'])
AC_SUBST([GETTEXT_PACKAGE])
if test "$mingw_found" != "yes" ; then
	dnl gettext macro does not work properly under mingw. And we want to use the one provided by GTK.
	AM_GNU_GETTEXT([external])
	AM_GNU_GETTEXT_VERSION([0.18])
	LIBS="$LIBS $LIBINTL"
else
	AC_DEFINE(ENABLE_NLS,1,[Tells whether localisation is possible])
	AC_DEFINE(HAVE_GETTEXT,1,[Tells wheter localisation is possible])
	LIBS="$LIBS -lintl"
fi

AM_CONDITIONAL(BUILD_MACOSX, test x$macosx_found = xyes)
AM_CONDITIONAL(BUILD_IOS, test x$ios_found = xyes)

AC_CONFIG_COMMANDS([libtool-hacking],[
if test "$mingw_found" = "yes" ; then
	echo "Hacking libtool to work with mingw..."
	sed -e 's/\*\" \$a_deplib \"\*/\*/' < ./libtool > libtool.tmp
	cp -f ./libtool.tmp ./libtool
	rm -f ./libtool.tmp
else
	echo "No need to hack libtool."
fi
], [mingw_found=$mingw_found])


dnl prefer fixed point computations
AC_ARG_ENABLE(fixed_point,
      [  --enable-fixed-point    Turn on fixed point computations (default: guess)],
      [case "${enableval}" in
        yes)  fixed_point=true;;
        no)   fixed_point=false;;
	guess) fixed_point=guess;;
        *) AC_MSG_ERROR(bad value ${enableval} for --enable-fixed-point) ;;
      esac],[fixed_point=guess])

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

AC_CHECK_LIB(rt,clock_gettime,[LIBS="$LIBS -lrt"])

dnl	*********************************
dnl	various checks for soundcard apis
dnl	*********************************

found_sound=no

if test x$mingw_found = xyes ; then
	found_sound=yes
fi

AC_ARG_ENABLE(sound,
      [  --disable-sound	Turn off all possible sound backend (default=no)],
      [case "${enableval}" in
        yes) use_sound=true ;;
        no)  use_sound=false ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --disable-sound) ;;
      esac],[use_sound=true])

AC_ARG_ENABLE(oss,
      [  --enable-oss    Enable oss support],
      [case "${enableval}" in
        yes) oss=true ;;
        no)  oss=false ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --disable-oss) ;;
      esac],[oss=$use_sound])

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
      [  --enable-alsa    Turn on alsa native support compiling],
      [case "${enableval}" in
        yes) alsa=true ;;
        no)  alsa=false ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --enable-alsa) ;;
      esac],[alsa=$use_sound])

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
      [  --enable-artsc    Turn on artsc (kde<4) sound input/output (no) ],
      [case "${enableval}" in
        yes) artsc=true ;;
        no)  artsc=false ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --enable-artsc) ;;
      esac],[artsc=false])

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
      [  --enable-portaudio    Turn on portaudio native support compiling],
      [case "${enableval}" in
        yes) portaudio=true ;;
        no)  portaudio=false ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --enable-portaudio) ;;
      esac],[portaudio=false])

if test "$portaudio" = "true"; then
	AC_CHECK_HEADERS(portaudio.h,
		[ AC_CHECK_LIB(portaudio,Pa_Initialize,
			[ PORTAUDIO_LIBS="-lportaudio"
			  found_sound=yes
			AC_DEFINE(__PORTAUDIO_ENABLED__,1,[defined if portaudio support is available])
			portaudio_enabled=true 
			])
		]
	)
fi

AC_SUBST(PORTAUDIO_LIBS)
AM_CONDITIONAL(BUILD_PORTAUDIO, test x$portaudio_enabled = xtrue)

AC_ARG_ENABLE(macsnd,
      [  --enable-macsnd    Turn on Audio Units macosx sound support (default=yes)],
      [case "${enableval}" in
        yes) macsnd=true ;;
        no)  macsnd=false ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --enable-macsnd) ;;
      esac],[macsnd=$use_sound])

macaqsnd_default=no
if test "$ios_found" = "yes" ; then
	macaqsnd_default=true;
fi

AC_ARG_ENABLE(macaqsnd,
      [  --enable-macaqsnd    Turn on native macosx Audio Queue sound support (default=no)],
      [case "${enableval}" in
        yes) macaqsnd=true ;;
        no)  macaqsnd=false ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --enable-macaqsnd) ;;
      esac],[macaqsnd=$macaqsnd_default])



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
      [  --disable-pulseaudio    Disable pulseaudio support],
      [case "${enableval}" in
        yes) pulseaudio=true ;;
        no)  pulseaudio=false ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --disable-pulseaudio) ;;
      esac],[pulseaudio=$use_sound])


if test x$pulseaudio = xtrue ; then
	PKG_CHECK_MODULES(LIBPULSE, libpulse >= 0.9.21,
		[
		AC_DEFINE(__PULSEAUDIO_ENABLED__,1,[Pulse audio support])
		found_sound=yes
		],
		[pulseaudio=false])
fi

AM_CONDITIONAL(BUILD_PULSEAUDIO,test x$pulseaudio = xtrue)


if test "${found_sound}${use_sound}" = "noyes"; then
	AC_MSG_ERROR([Could not find a support sound driver API. Use --disable-sound if you don't care about having sound.])
fi


dnl	*************************************
dnl	check for various codecs libraries
dnl	*************************************

AC_ARG_ENABLE(speex,
      [  --disable-speex    Disable speex support],
      [case "${enableval}" in
        yes) speex=true ;;
        no)  speex=false ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --disable-speex) ;;
      esac],[speex=true])

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
    build_resample=yes] ,
    [AC_MSG_ERROR([No libspeexdsp library found.])
]
)
AC_SUBST(SPEEX_CFLAGS)
AC_SUBST(SPEEX_LIBS)

fi

AM_CONDITIONAL(BUILD_SPEEX, test x$build_speex = xyes )
AM_CONDITIONAL(BUILD_RESAMPLE, test x$build_resample = xyes )

AC_ARG_ENABLE(gsm,
      [  --disable-gsm    Disable gsm support],
      [case "${enableval}" in
        yes) gsm=true ;;
        no)  gsm=false ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --disable-gsm) ;;
      esac],[gsm=auto])
      
if test x$gsm != xfalse; then

dnl check for gsm
build_gsm=no
AC_ARG_WITH( gsm,
		  [  --with-gsm		Sets the installation prefix of gsm codec library [default=/usr] ],
		  [ gsmdir=${withval}],[ gsmdir=/usr ])

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
      [  --disable-spandsp    Disable spandsp support],
      [case "${enableval}" in
        yes) spandsp=true ;;
        no)  spandsp=false ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --disable-spandsp) ;;
      esac],[spandsp=true])

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

dnl	*********************************************
dnl	setup oRTP dependency
dnl	*********************************************
AC_ARG_ENABLE(external-ortp,
      [  --enable-external-ortp    Use external oRTP library],
      [case "${enableval}" in
        yes) external_ortp=true ;;
        no)  external_ortp=false ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --enable-external-ortp) ;;
      esac],[external_ortp=false])

if test "$external_ortp" = 'false'; then
	if test -e $srcdir/../oRTP/include/ortp/ortp.h ; then 
		echo "building from linphone source tree, using ../oRTP/include/ortp/ortp.h"
		ORTP_CFLAGS="-I\$(top_srcdir)/../oRTP/include"
		ORTP_LIBS="\$(top_builddir)/../oRTP/src/libortp.la"
		if test x$ac_cv_c_bigendian = xyes ; then
			ORTP_CFLAGS="$ORTP_CFLAGS -DORTP_BIGENDIAN"
		fi
	else
		external_ortp=true
	fi
fi
if test "$external_ortp" = 'true'; then
	PKG_CHECK_MODULES(ORTP, ortp >= 0.17.0, ,
		[ AC_MSG_ERROR([Couldn't find ortp library]) ] )
fi
AC_SUBST(ORTP_CFLAGS)
AC_SUBST(ORTP_LIBS)


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
      [  --enable-relativeprefix    Build a linphone that finds its resources relatively to the directory where it is installed],
      [case "${enableval}" in
        yes) relativeprefix=yes ;;
        no)  relativeprefix=no ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --enable-relativeprefix) ;;
      esac],[relativeprefix=guess])

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
      [  --disable-libv4l1    Disable usage of libv4l1, really discouraged],
      [case "${enableval}" in
        yes) libv4l1=yes ;;
        no)  libv4l1=no ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --disable-libv4l1) ;;
      esac],[libv4l1=yes])

AC_ARG_ENABLE(libv4l2,
      [  --disable-libv4l2    Disable usage of libv4l2, really discouraged],
      [case "${enableval}" in
        yes) libv4l2=yes ;;
        no)  libv4l2=no ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --disable-libv4l2) ;;
      esac],[libv4l2=yes])

PKG_CHECK_MODULES(LIBV4L2, libv4l2,
	[AC_DEFINE(HAVE_LIBV4L2,1,[Defined if we have libv4l2])
	have_libv4l2=yes]
	,[echo "No libv4l2 found."]
)
PKG_CHECK_MODULES(LIBV4L1, libv4l1,
	[AC_DEFINE(HAVE_LIBV4L1,1,[Defined if we have libv4l1])
	have_libv4l1=yes]
	,[echo "No libv4l1 found."]
)

if test "$found_v4l2" = "yes" && test "$have_libv4l2" != "yes" ; then
	if test "$video" = "true" && test "$libv4l2" = "yes" ; then
		AC_MSG_ERROR(
[
Missing libv4l2. It is highly recommended to build with
libv4l2 headers and library. Many camera will won't work or will crash
your application if libv4l2 is not installed.
If you know what you are doing, you can use --disable-libv4l2 to disable
this check.
])
	fi
fi


AC_ARG_ENABLE(tests,
      [  --enable-tests    Disable compilation of tests],
      [case "${enableval}" in
        yes) tests=yes ;;
        no)  tests=no  ;;
        *) AC_MSG_ERROR(bad value ${enableval} for --enable-tests) ;;
      esac],[tests=yes])
AM_CONDITIONAL(ENABLE_TESTS, test x$tests = xyes)


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
include/Makefile
include/mediastreamer2/Makefile
src/Makefile
tests/Makefile
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
)

AC_OUTPUT


