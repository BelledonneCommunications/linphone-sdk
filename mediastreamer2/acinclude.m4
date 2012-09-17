dnl -*- autoconf -*-
AC_DEFUN([MS_CHECK_DEP],[
	dnl $1=dependency description
	dnl $2=dependency short name, will be suffixed with _CFLAGS and _LIBS
	dnl $3=headers's place
	dnl $4=lib's place
	dnl $5=header to check
	dnl $6=lib to check
	dnl $7=function to check in library
	
	dep_name=$2
	dep_headersdir=$3
	dep_libsdir=$4
	dep_header=$5
	dep_lib=$6
	dep_funclib=$7
	other_libs=$8	
	
	CPPFLAGS_save=$CPPFLAGS
	LDFLAGS_save=$LDFLAGS
	LIBS_save=$LIBS

	case "$target_os" in
		*mingw*)
			ms_check_dep_mingw_found=yes
		;;
	esac
	if test "$ms_check_dep_mingw_found" != "yes" ; then
		CPPFLAGS=`echo "-I$dep_headersdir"|sed -e "s:-I/usr/include[\ ]*$::"`
		LDFLAGS=`echo "-L$dep_libsdir"|sed -e "s:-L/usr/lib\(64\)*[\ ]*::"`
	else
		CPPFLAGS="-I$dep_headersdir"	
		LDFLAGS="-L$dep_libsdir"
	fi


	LIBS="-l$dep_lib"

	
	$2_CFLAGS="$CPPFLAGS"
	$2_LIBS="$LDFLAGS $LIBS"

	AC_CHECK_HEADERS([$dep_header],[AC_CHECK_LIB([$dep_lib],[$dep_funclib],found=yes,found=no, [$other_libs])
	],found=no)
	
	if test "$found" = "yes" ; then
		eval $2_found=yes
	else
		eval $2_found=no
		eval $2_CFLAGS=
		eval $2_LIBS=
	fi
	AC_SUBST($2_CFLAGS)
	AC_SUBST($2_LIBS)
	CPPFLAGS=$CPPFLAGS_save
	LDFLAGS=$LDFLAGS_save
	LIBS=$LIBS_save
])


AC_DEFUN([MS_CHECK_VIDEO],[

	dnl conditionnal build of video support
	AC_ARG_ENABLE(video,
		  [AS_HELP_STRING([--enable-video], [Turn on video support compiling (default=yes])],
		  [case "${enableval}" in
			yes) video=true ;;
			no)  video=false ;;
			*) AC_MSG_ERROR(bad value ${enableval} for --enable-video) ;;
		  esac],[video=true])

	AC_ARG_ENABLE(ffmpeg,
		  [AS_HELP_STRING([--disable-ffmpeg], [Disable ffmpeg support])],
		  [case "${enableval}" in
			yes) ffmpeg=true ;;
			no)  ffmpeg=false ;;
			*) AC_MSG_ERROR(bad value ${enableval} for --disable-ffmpeg) ;;
		  esac],[ffmpeg=true])
	
	if test "$video" = "true"; then
		
		if test "$ffmpeg" = "true"; then
			dnl test for ffmpeg presence
			PKG_CHECK_MODULES(FFMPEG, [libavcodec >= 51.0.0 ],ffmpeg_found=yes , ffmpeg_found=no)
			if test x$ffmpeg_found = xno ; then
			   AC_MSG_ERROR([Could not find libavcodec (from ffmpeg) headers and library. This is mandatory for video support])
			fi
			
			FFMPEG_LIBS="$FFMPEG_LIBS -lavutil"
			
			PKG_CHECK_MODULES(SWSCALE, [libswscale >= 0.7.0 ],swscale_found=yes , swscale_found=no)
			if test x$swscale_found = xno ; then
			   AC_MSG_ERROR([Could not find libswscale (from ffmpeg) headers and library. This is mandatory for video support])
			fi

			dnl check for new/old ffmpeg header file layout
			CPPFLAGS_save=$CPPFLAGS
			CPPFLAGS="$FFMPEG_CFLAGS $CPPFLAGS"
			AC_CHECK_HEADERS(libavcodec/avcodec.h)
			CPPFLAGS=$CPPFLAGS_save

			dnl to workaround a bug on debian and ubuntu, check if libavcodec needs -lvorbisenc to compile	
			AC_CHECK_LIB(avcodec,avcodec_register_all, novorbis=yes , [
				LIBS="$LIBS -lvorbisenc"
				], $FFMPEG_LIBS )

			dnl when swscale feature is not provided by
			dnl libswscale, its features are swallowed by
			dnl libavcodec, but without swscale.h and without any
			dnl declaration into avcodec.h (this is to be
			dnl considered as an ffmpeg bug).
			dnl 
			dnl #if defined(HAVE_LIBAVCODEC_AVCODEC_H) && !defined(HAVE_LIBSWSCALE_SWSCALE_H)
			dnl # include "swscale.h" // private linhone swscale.h
			dnl #endif
			CPPFLAGS_save=$CPPFLAGS
			CPPFLAGS="$SWSCALE_CFLAGS $CPPFLAGS"
			AC_CHECK_HEADERS(libswscale/swscale.h)
			CPPFLAGS=$CPPFLAGS_save

			if test "$macosx_found" = "yes" ; then
				dnl we use quartz+opengl directly on mac os for video display.
				enable_sdl_default=false
				enable_x11_default=false
				OBJCFLAGS="$OBJCFLAGS -framework QTKit"
				LIBS="$LIBS -framework QTKit -framework CoreVideo "
				dnl the following check is necessary but due to automake bug it forces every platform to have an objC compiler !
				dnl AC_LANG_PUSH([Objective C])
				dnl AC_CHECK_HEADERS([QTKit/QTKit.h],[],[AC_MSG_ERROR([QTKit framework not found, required for video support])])
				dnl AC_LANG_POP([Objective C])
			elif test "$ios_found" = "yes" ; then
				enable_sdl_default=false
				enable_x11_default=false
			elif test "$ms_check_dep_mingw_found" = "yes" ; then
				enable_sdl_default=false
				enable_x11_default=false
			else
				enable_sdl_default=false
				enable_x11_default=true
			fi

			AC_ARG_ENABLE(sdl,
			  [AS_HELP_STRING([--disable-sdl], [Disable SDL support (default: disabled except on macos)])],
			  	  [case "${enableval}" in
				  yes) enable_sdl=true ;;
				  no)  enable_sdl=false ;;
			  *) AC_MSG_ERROR(bad value ${enableval} for --disable-sdl) ;;
		  	  esac],[enable_sdl=$enable_sdl_default])

			sdl_found="false"
			if test "$enable_sdl" = "true"; then
				   PKG_CHECK_MODULES(SDL, [sdl >= 1.2.0 ],sdl_found=true,[AC_MSG_ERROR([No SDL library found])])
			fi


			AC_ARG_ENABLE(x11,
			  [AS_HELP_STRING([--disable-x11], [Disable X11 support])],
		 	  [case "${enableval}" in
			  yes) enable_x11=true ;;
			  no)  enable_x11=false ;;
			  *) AC_MSG_ERROR(bad value ${enableval} for --disable-x11) ;;
		  	  esac],[enable_x11=$enable_x11_default])

			if test "$enable_x11" = "true"; then
			   AC_CHECK_HEADERS(X11/Xlib.h)
			fi

			AC_ARG_ENABLE(xv,
			  [AS_HELP_STRING([--enable-xv], [Enable xv support])],
			  [case "${enableval}" in
			  yes) enable_xv=true ;;
			  no)  enable_xv=false ;;
			  *) AC_MSG_ERROR(bad value ${enableval} for --enable-xv) ;;
		  	  esac],[enable_xv=$enable_x11_default])

			if test "$enable_xv" = "true"; then
				AC_CHECK_HEADERS(X11/extensions/Xv.h,[] ,[enable_xv=false])
				AC_CHECK_HEADERS(X11/extensions/Xvlib.h,[] ,[enable_xv=false],[
					#include <X11/Xlib.h>
				])
				AC_CHECK_LIB(Xv,XvCreateImage,[LIBS="$LIBS -lXv"],[enable_xv=false])
				if test "$enable_xv" = "false" ; then
					AC_MSG_ERROR([No X video output API found. Please install X11+Xv headers.])
				fi
			fi
			AC_ARG_ENABLE(gl,
			  [AS_HELP_STRING([--enable-gl], [Enable GL rendering support (require glx and glew)])],
			  [case "${enableval}" in
			  yes) enable_gl=true ;;
			  no)  enable_gl=false ;;
			  *) AC_MSG_ERROR(bad value ${enableval} for --enable-gl) ;;
		  	  esac],[enable_gl=false])

			if test "$enable_gl" = "true"; then
				AC_CHECK_HEADERS(GL/gl.h,[] ,[enable_gl=false])
				AC_CHECK_HEADERS(GL/glx.h,[] ,[enable_gl=false],[
					#include <GL/glx.h>
				])
				if test "$enable_gl" = "false" ; then
					AC_MSG_ERROR([No GL/GLX API found. Please install GL and GLX headers.])
				fi
				PKG_CHECK_MODULES(GLEW,[glew >= 1.6])
				AC_CHECK_HEADERS(X11/Xlib.h)
			fi
		fi
		
		AC_ARG_ENABLE(theora,
		  [AS_HELP_STRING([--disable-theora], [Disable theora support])],
		  [case "${enableval}" in
			yes) theora=true ;;
			no)  theora=false ;;
			*) AC_MSG_ERROR(bad value ${enableval} for --disable-theora) ;;
		  esac],[theora=true])

		if test x$theora = xtrue; then
		PKG_CHECK_MODULES(THEORA, [theora >= 1.0alpha7 ], [have_theora=yes],
					[have_theora=no])
		fi
		
		AC_ARG_ENABLE(vp8,
		  [AS_HELP_STRING([--disable-vp8], [Disable vp8 support])],
		  [case "${enableval}" in
			yes) vp8=true ;;
			no)  vp8=false ;;
			*) AC_MSG_ERROR(bad value ${enableval} for --disable-vp8) ;;
		  esac],[vp8=true])

		vp8dir=/usr
		if test x$vp8 = xtrue; then
			PKG_CHECK_MODULES(VP8, [vpx >= 0.9.6 ], [have_vp8=yes],
					[have_vp8=no])
			if test "$have_vp8" = "no" ; then
				MS_CHECK_DEP([VP8 codec],[VP8],[${vp8dir}/include],
					[${vp8dir}/lib],[vpx/vpx_encoder.h],[vpx],[vpx_codec_encode])
				if test "$VP8_found" = "yes" ; then
					have_vp8=yes
				fi
			fi
		fi

		if test "$ffmpeg" = "false"; then
		   FFMPEG_CFLAGS=" $FFMPEG_CFLAGS -DNO_FFMPEG"
		fi

		VIDEO_CFLAGS=" $FFMPEG_CFLAGS -DVIDEO_ENABLED"
		VIDEO_LIBS=" $FFMPEG_LIBS $SWSCALE_LIBS"

		if test "$sdl_found" = "true" ; then
			VIDEO_CFLAGS="$VIDEO_CFLAGS $SDL_CFLAGS -DHAVE_SDL"
			VIDEO_LIBS="$VIDEO_LIBS $SDL_LIBS"
		fi

		if test "${ac_cv_header_X11_Xlib_h}" = "yes" ; then
			VIDEO_LIBS="$VIDEO_LIBS -lX11"
		fi

		if test "$mingw_found" = "yes" ; then
			VIDEO_LIBS="$VIDEO_LIBS -lvfw32 -lgdi32"
		fi
		if test "$ios_found" = "yes" ; then
			LIBS="$LIBS -framework AVFoundation -framework CoreVideo -framework CoreMedia"
		fi
		if test "$enable_gl" = "true"; then
			VIDEO_LIBS="$VIDEO_LIBS -lGL -lGLEW"
			VIDEO_CFLAGS="$VIDEO_CFLAGS -DHAVE_GL"
		fi
		if test "$enable_xv" = "true"; then
			VIDEO_CFLAGS="$VIDEO_CFLAGS -DHAVE_XV"
		fi
	fi
	
	AC_SUBST(VIDEO_CFLAGS)
	AC_SUBST(VIDEO_LIBS)
])
