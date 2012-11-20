GITVERSION_FILE=gitversion.h
GITVERSION_FILE_TMP=gitversion.h.tmp

ECHO=/bin/echo

ANDROID_SRC_FILES= \
	android/AudioRecord.cpp android/AudioRecord.h \
	android/AudioSystem.cpp android/AudioSystem.h \
	android/AudioTrack.cpp android/AudioTrack.h \
	android/String8.cpp android/String8.h \
	android/androidsound.cpp \
	android/androidsound_depr.cpp \
	android/androidvideo.cpp \
	android/android-display.c \
	android/android-display-bad.cpp \
	android/android-opengl-display.c \
	android/audio.h \
	android/loader.cpp android/loader.h \
	android/android_echo.cpp android/android_echo.h \
	audiofilters/webrtc_aec.c

EXTRA_DIST= audiofilters/winsnd2.c audiofilters/winsnd.c videofilters/winvideo.c  \
	videofilters/winvideods.c videofilters/wincevideods.c dxfilter.h dxfilter.cpp \
	audiofilters/winsndds.cpp \
	voip/nowebcamCIF.jpg audiofilters/winsnd3.c utils/vfw-missing.h \
	videofilters/winvideo2.c utils/msjava.c $(ANDROID_SRC_FILES) \
	$(GITVERSION_FILE) yuv2rgb.fs yuv2rgb.vs

BUILT_SOURCES=basedescs.h $(GITVERSION_FILE) yuv2rgb.fs.h yuv2rgb.vs.h

CLEANFILES=basedescs.h voipdescs.h basedescs.txt voipdescs.txt $(GITVERSION_FILE) yuv2rgb.fs.h yuv2rgb.vs.h

lib_LTLIBRARIES=libmediastreamer_base.la
if BUILD_VOIP_LIBRARY
lib_LTLIBRARIES+=libmediastreamer_voip.la
endif

libmediastreamer_base_la_SOURCES=	base/mscommon.c \
					$(GITVERSION_FILE) \
					base/msfilter.c \
					base/msqueue.c \
					base/msticker.c \
					base/eventqueue.c \
					base/mssndcard.c \
					otherfilters/tee.c \
					otherfilters/join.c \
					base/mswebcam.c \
					base/mtu.c \
					otherfilters/void.c \
					otherfilters/itc.c
libmediastreamer_voip_la_SOURCES=

#dummy c++ file to force libtool to use c++ linking (because of msdscap-mingw.cc)
nodist_EXTRA_libmediastreamer_base_la_SOURCES = dummy.cxx


if ORTP_ENABLED
BUILT_SOURCES+=voipdescs.h
libmediastreamer_voip_la_SOURCES+=	voip/private.h \
					voip/msvoip.c \
					voip/mediastream.c \
					voip/audiostream.c \
					voip/ringstream.c \
					voip/ice.c \
					otherfilters/msrtp.c \
					voip/qualityindicator.c \
					voip/audioconference.c \
					voip/bitratedriver.c \
					voip/qosanalyzer.c \
					voip/bitratecontrol.c
else
libmediastreamer_base_la_SOURCES+=	ortp-deps/logging.c \
					ortp-deps/port.c \
					ortp-deps/str_utils.c
if MS2_FILTERS
libmediastreamer_base_la_SOURCES+=	ortp-deps/b64.c \
					ortp-deps/payloadtype.c
endif
endif

if MS2_FILTERS
libmediastreamer_voip_la_SOURCES+=	audiofilters/alaw.c \
					audiofilters/ulaw.c \
					audiofilters/dtmfgen.c \
					audiofilters/msconf.c \
					utils/g711common.h \
					audiofilters/msvolume.c \
					utils/dsptools.c \
					utils/kiss_fft.c \
					utils/_kiss_fft_guts.h \
					utils/kiss_fft.h \
					utils/kiss_fftr.c \
					utils/kiss_fftr.h \
					audiofilters/equalizer.c \
					audiofilters/chanadapt.c \
					audiofilters/audiomixer.c \
					audiofilters/tonedetector.c \
					utils/g722.h \
					utils/g722_decode.c \
					utils/g722_encode.c \
					audiofilters/msg722.c \
					audiofilters/l16.c \
					audiofilters/genericplc.c \
					audiofilters/msfileplayer.c \
					audiofilters/msfilerec.c \
					audiofilters/waveheader.h

if BUILD_SPEEX
libmediastreamer_voip_la_SOURCES+=	audiofilters/msspeex.c audiofilters/speexec.c
endif

if BUILD_GSM
libmediastreamer_voip_la_SOURCES+=	audiofilters/gsm.c
endif

if BUILD_G726
libmediastreamer_voip_la_SOURCES+=	audiofilters/g726.c
endif

if BUILD_OPUS
libmediastreamer_voip_la_SOURCES+=	audiofilters/msopus.c
endif

if BUILD_WIN32
libmediastreamer_voip_la_SOURCES+=	audiofilters/winsnd3.c
endif

if BUILD_RESAMPLE
libmediastreamer_voip_la_SOURCES+=	audiofilters/msresample.c
endif

if BUILD_ALSA
libmediastreamer_voip_la_SOURCES+=	audiofilters/alsa.c
endif

if BUILD_OSS
libmediastreamer_voip_la_SOURCES+=	audiofilters/oss.c
endif

if BUILD_ARTS
libmediastreamer_voip_la_SOURCES+=	audiofilters/arts.c
endif

if BUILD_PORTAUDIO
libmediastreamer_voip_la_SOURCES+=	audiofilters/pasnd.c
endif

if BUILD_MACSND
libmediastreamer_voip_la_SOURCES+=	audiofilters/macsnd.c
endif

if BUILD_IOSIOUNIT
libmediastreamer_voip_la_SOURCES+=	audiofilters/msiounit.m
libmediastreamer_voip_la_SOURCES+=	audiofilters/aac-eld.c
endif

if BUILD_MACAQSND
libmediastreamer_voip_la_SOURCES+=	audiofilters/aqsnd.c
endif

if BUILD_PULSEAUDIO
libmediastreamer_voip_la_SOURCES+=	audiofilters/pulseaudio.c
endif

if BUILD_VIDEO

if BUILD_MACOSX
libmediastreamer_voip_la_SOURCES+=	videofilters/qtcapture.m \
					videofilters/msosxdisplay.m \
					utils/shaders.c utils/shaders.h \
					utils/opengles_display.c utils/opengles_display.h
nodist_libmediastreamer_voip_la_SOURCES = yuv2rgb.fs.h yuv2rgb.fs.h
endif

if BUILD_IOS
libmediastreamer_voip_la_SOURCES+=	voip/scaler.c \
					videofilters/iosdisplay.m \
					videofilters/ioscapture.m \
					utils/shaders.c \
					utils/shaders.h \
					utils/opengles_display.c \
					utils/opengles_display.h
nodist_libmediastreamer_voip_la_SOURCES = yuv2rgb.fs.h yuv2rgb.fs.h
endif

if BUILD_V4L1
libmediastreamer_voip_la_SOURCES+=	videofilters/msv4l.c
endif

if BUILD_V4L2
libmediastreamer_voip_la_SOURCES+=	videofilters/msv4l2.c
endif

if BUILD_WIN32
libmediastreamer_voip_la_SOURCES+=	videofilters/msdscap-mingw.cc videofilters/drawdib-display.c
endif

if BUILD_THEORA
libmediastreamer_voip_la_SOURCES+=	videofilters/theora.c
endif

if BUILD_VP8
libmediastreamer_voip_la_SOURCES+=	videofilters/vp8.c
endif

if BUILD_FFMPEG
libmediastreamer_voip_la_SOURCES+=	videofilters/videoenc.c \
					videofilters/videodec.c \
					utils/swscale.h \
					utils/ffmpeg-priv.h \
					utils/ffmpeg-priv.c \
					videofilters/h264dec.c \
					videofilters/jpegwriter.c
endif

if BUILD_SDL
libmediastreamer_voip_la_SOURCES+=	videofilters/videoout.c 
endif

if BUILD_X11_XV
libmediastreamer_voip_la_SOURCES+=	videofilters/x11video.c
endif

if BUILD_X11_GL
libmediastreamer_voip_la_SOURCES+=	videofilters/glxvideo.c utils/opengles_display.c utils/shaders.c
endif

libmediastreamer_voip_la_SOURCES+=	voip/rfc2429.h \
					videofilters/pixconv.c  \
					videofilters/sizeconv.c \
					voip/msvideo.c \
					voip/msvideo_neon.c \
					voip/msvideo_neon.h \
					voip/rfc3984.c \
					videofilters/mire.c \
					videofilters/extdisplay.c \
					voip/layouts.c voip/layouts.h \
					videofilters/nowebcam.c voip/nowebcam.h 

if ORTP_ENABLED
libmediastreamer_voip_la_SOURCES+=	voip/videostream.c
endif

endif BUILD_VIDEO

endif MS2_FILTERS

if BUILD_UPNP 
libmediastreamer_voip_la_SOURCES+=	upnp/upnp_igd.c \
					upnp/upnp_igd_private.h \
					upnp/upnp_igd_cmd.c \
					upnp/upnp_igd_utils.c \
					upnp/upnp_igd_utils.h
endif

basedescs.h:	Makefile $(libmediastreamer_base_la_SOURCES)
	builddir=`pwd` && cd $(srcdir) && \
	awk 'BEGIN { FS="[()]" ; }; /^\t*MS_FILTER_DESC_EXPORT/{ printf("%s\n", $$2) } '  > $$builddir/basedescs.txt $(libmediastreamer_base_la_SOURCES) && \
	awk 'BEGIN { print("#include \"mediastreamer2/msfilter.h\"\n") } { printf("extern MSFilterDesc %s;\n",$$1) } ' $$builddir/basedescs.txt > $$builddir/$@ && \
	awk 'BEGIN { print("MSFilterDesc * ms_base_filter_descs[]={") } { printf("&%s,\n",$$1) } END{ print("NULL\n};\n") } ' $$builddir/basedescs.txt >> $$builddir/$@

voipdescs.h:	Makefile $(libmediastreamer_voip_la_SOURCES)
	builddir=`pwd` && cd $(srcdir) && \
	awk 'BEGIN { FS="[()]" ; }; /^\t*MS_FILTER_DESC_EXPORT/{ printf("%s\n", $$2) } '  > $$builddir/voipdescs.txt $(libmediastreamer_voip_la_SOURCES) && \
	awk 'BEGIN { print("#include \"mediastreamer2/msfilter.h\"\n") } { printf("extern MSFilterDesc %s;\n",$$1) } ' $$builddir/voipdescs.txt > $$builddir/$@ && \
	awk 'BEGIN { print("MSFilterDesc * ms_voip_filter_descs[]={") } { printf("&%s,\n",$$1) } END{ print("NULL\n};\n") } ' $$builddir/voipdescs.txt >> $$builddir/$@


libmediastreamer_base_la_LIBADD=	$(ORTP_LIBS)
libmediastreamer_base_la_LDFLAGS= -no-undefined -version-info $(LIBMEDIASTREAMER_SO_VERSION)

if !BUILD_WIN32
libmediastreamer_base_la_LDFLAGS+=-rdynamic
endif

if BUILD_WIN32
libmediastreamer_base_la_LIBADD+=	-lole32 \
					-loleaut32\
					-lwinmm \
					-luuid


endif
if BUILD_WIN32_WCE
libmediastreamer_base_la_LIBADD+=	-lmmtimer
endif

AM_CPPFLAGS=\
	-I$(top_srcdir)/include/ \
	-I$(top_srcdir)/src/base \
	-I$(top_srcdir)/src/utils \
	-I$(top_srcdir)/src/voip \
	-I$(top_srcdir)/src/audiofilters \
	-I$(top_srcdir)/src/otherfilters \
	-I$(top_srcdir)/src/videofilters

AM_CFLAGS=\
	$(STRICT_OPTIONS) \
	$(ORTP_CFLAGS)   \
	$(SPEEX_CFLAGS)  \
	$(GSM_CFLAGS)    \
	$(STRICT_OPTIONS) \
	$(LIBPULSE_CFLAGS) \
	$(SPANDSP_CFLAGS)  \
	$(MSSILK_CFLAGS) \
	$(PCAP_CFLAGS)

AM_CXXFLAGS=\
	$(STRICT_OPTIONS) \
	$(ORTP_CFLAGS)

DEFS=@DEFS@  -DPACKAGE_PLUGINS_DIR=\"$(PACKAGE_PLUGINS_DIR)\" -DLOCALEDIR=\"$(localedir)\" -DMS2_INTERNAL

if MS2_FILTERS
DEFS+= -DMS2_FILTERS
endif

if BUILD_VIDEO
AM_CFLAGS+=$(VIDEO_CFLAGS) $(GLEW_CFLAGS)
endif


libmediastreamer_voip_la_LIBADD=	libmediastreamer_base.la \
					$(PORTAUDIO_LIBS) \
					$(ALSA_LIBS) \
					$(ARTS_LIBS) \
					$(LIBPULSE_LIBS) \
					$(SPEEX_LIBS) \
					$(GSM_LIBS) \
					$(LIBV4L1_LIBS) \
					$(LIBV4L2_LIBS) \
					$(SPANDSP_LIBS) \
					$(PCAP_LIBS) \
					$(OPUS_LIBS)

if BUILD_VP8
AM_CFLAGS+=$(VP8_CFLAGS)
libmediastreamer_voip_la_LIBADD+=$(VP8_LIBS)
endif

if BUILD_VIDEO
libmediastreamer_voip_la_LIBADD+=	$(VIDEO_LIBS) \
					$(THEORA_LIBS) \
					$(GLEW_LIBS)
endif

libmediastreamer_voip_la_LDFLAGS=	$(libmediastreamer_base_la_LDFLAGS)

if BUILD_UPNP
AM_CFLAGS+=$(LIBUPNP_CFLAGS) -D_GNU_SOURCE
libmediastreamer_voip_la_LIBADD+=	$(LIBUPNP_LIBS)
endif

if BUILD_IOS 
libmediastreamer_voip_la_LDFLAGS+=	-framework CoreGraphics
endif

if BUILD_MACOSX
libmediastreamer_voip_la_LDFLAGS+=	-framework Cocoa -framework OpenGL -framework QuartzCore
endif



AM_OBJCFLAGS=$(AM_CFLAGS)

imgdir=$(datadir)/images/

img_DATA=voip/nowebcamCIF.jpg

make_gitversion_h:
	if test -d $(top_srcdir)/.git ; then \
		$(ECHO) -n "#define GIT_VERSION " > $(GITVERSION_FILE_TMP) &&\
		$(ECHO) \"`cd $(top_srcdir) && git describe --always`\" >> $(GITVERSION_FILE_TMP) &&\
		if test "`cat $(GITVERSION_FILE_TMP)`" != "`cat $(srcdir)/$(GITVERSION_FILE)`" ; then \
			cp -f $(GITVERSION_FILE_TMP) $(srcdir)/$(GITVERSION_FILE) ; \
		fi \
		&& rm -f $(GITVERSION_FILE_TMP) ;\
	fi
	if ! test -f $(srcdir)/$(GITVERSION_FILE) ; then \
		$(ECHO) -n "#define GIT_VERSION \"unknown\" " > $(srcdir)/$(GITVERSION_FILE) ;\
	fi

$(GITVERSION_FILE):	make_gitversion_h

yuv2rgb.fs.h: yuv2rgb.fs
	builddir=`pwd` && \
	cd $(top_srcdir)/src && \
	xxd -i yuv2rgb.fs | sed s/}\;/,0x00}\;/ > $$builddir/yuv2rgb.fs.h

yuv2rgb.vs.h: yuv2rgb.vs
	builddir=`pwd` && \
	cd $(top_srcdir)/src && \
	xxd -i yuv2rgb.vs | sed s/}\;/,0x00}\;/ > $$builddir/yuv2rgb.vs.h
