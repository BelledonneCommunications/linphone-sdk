GITVERSION_FILE=gitversion.h
GITVERSION_FILE_TMP=gitversion.h.tmp

## This command is used to check if the sources are cloned in a git repo.
## We can't only depend on the presence of the .git/ directory anymore,
## because of gits submodule handling.
## We now simply issue a git log on configure.ac and if the output is empty (error or file not tracked), then we are not in git.
GITLOG=`cd "$(top_srcdir)" && git log -1 --pretty=format:%H configure.ac`

ECHO=/bin/echo

ANDROID_SRC_FILES= \
	android/AudioRecord.cpp android/AudioRecord.h \
	android/AudioSystem.cpp android/AudioSystem.h \
	android/AudioTrack.cpp android/AudioTrack.h \
	android/String8.cpp android/String8.h \
	android/hardware_echo_canceller.cpp android/hardware_echo_canceller.h \
	android/androidsound.cpp \
	android/androidsound_depr.cpp \
	android/androidvideo.cpp \
	android/android-display.c \
	android/android-display-bad.cpp \
	android/android-opengl-display.c \
	android/audio.h \
	android/loader.cpp android/loader.h \
	android/mediacodech264dec.c \
   	android/mediacodech264enc.c \
   	android/android_mediacodec.cpp \
   	android/android_mediacodec.h \
	audiofilters/aac-eld-android.cpp

EXTRA_DIST= audiofilters/winsnd2.c audiofilters/winsnd.c videofilters/winvideo.c  \
	videofilters/winvideods.c videofilters/wincevideods.c dxfilter.h dxfilter.cpp \
	audiofilters/winsndds.cpp videofilters/msdscap.cc \
	voip/nowebcamCIF.jpg audiofilters/winsnd3.c utils/vfw-missing.h \
	videofilters/winvideo2.c utils/msjava.c $(ANDROID_SRC_FILES) \
	$(GITVERSION_FILE) yuv2rgb.fs yuv2rgb.vs libsrtp.map

BUILT_SOURCES=basedescs.h $(GITVERSION_FILE) yuv2rgb.fs.h yuv2rgb.vs.h

CLEANFILES=basedescs.h voipdescs.h basedescs.txt voipdescs.txt $(GITVERSION_FILE) yuv2rgb.fs.h yuv2rgb.vs.h

libmediastreamer_voip_la_LIBADD=	libmediastreamer_base.la \
					$(ORTP_LIBS) \
					$(VP8_LIBS) \
					$(PORTAUDIO_LIBS) \
					$(ALSA_LIBS) \
					$(QSA_LIBS) \
					$(ARTS_LIBS) \
					$(LIBPULSE_LIBS) \
					$(SPEEX_LIBS) \
					$(GSM_LIBS) \
					$(LIBV4L1_LIBS) \
					$(LIBV4L2_LIBS) \
					$(LIBV4LCONVERT_LIBS) \
					$(SPANDSP_LIBS) \
					$(PCAP_LIBS) \
					$(OPUS_LIBS) \
					$(SRTP_LIBS) \
					$(BCTOOLBOX_LIBS)


if LIBBZRTP
libmediastreamer_voip_la_LIBADD+=$(LIBBZRTP_LIB)
endif

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
					base/msfactory.c \
					otherfilters/tee.c \
					otherfilters/join.c \
					base/msvideopresets.c \
					base/mswebcam.c \
					base/mtu.c \
					base/msasync.c \
					otherfilters/void.c \
					otherfilters/itc.c
libmediastreamer_voip_la_SOURCES=

#dummy c++ file to force libtool to use c++ linking
nodist_EXTRA_libmediastreamer_base_la_SOURCES = dummy.cxx


if ENABLE_PCAP
libmediastreamer_voip_la_SOURCES += otherfilters/mspcapfileplayer.c
endif

if ORTP_ENABLED
BUILT_SOURCES+=voipdescs.h
libmediastreamer_voip_la_SOURCES+=	voip/private.h \
					voip/msvoip.c \
					voip/mediastream.c \
					voip/audiostream.c \
					voip/ringstream.c \
					voip/rfc4103_textstream.c \
					otherfilters/rfc4103_source.c \
					otherfilters/rfc4103_sink.c \
					voip/msmediaplayer.c \
					voip/ice.c \
					otherfilters/msrtp.c \
					otherfilters/msudp.c \
					voip/qualityindicator.c \
					voip/audioconference.c \
					voip/bitratedriver.c \
					voip/qosanalyzer.c voip/qosanalyzer.h \
					voip/bitratecontrol.c \
					crypto/zrtp.c \
					voip/stun.c \
					crypto/ms_srtp.c \
					crypto/dtls_srtp.c \
					voip/msiframerequestslimiter.c \
					utils/pcap_sender.c utils/pcap_sender.h
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
					audiofilters/g711.c audiofilters/g711.h \
					audiofilters/msvolume.c \
					utils/dsptools.c \
					utils/kiss_fft.c \
					utils/_kiss_fft_guts.h \
					utils/kiss_fft.h \
					utils/kiss_fftr.c \
					utils/kiss_fftr.h \
					utils/audiodiff.c \
					audiofilters/equalizer.c \
					audiofilters/chanadapt.c \
					audiofilters/audiomixer.c \
					audiofilters/tonedetector.c \
					audiofilters/devices.c \
					utils/g722.h \
					utils/g722_decode.c \
					utils/g722_encode.c \
					audiofilters/msg722.c \
					audiofilters/l16.c \
					audiofilters/genericplc.h \
					audiofilters/genericplc.c \
					audiofilters/msgenericplc.c \
					audiofilters/msfileplayer.c \
					audiofilters/msfilerec.c \
					audiofilters/asyncrw.c \
					audiofilters/asyncrw.h \
					audiofilters/waveheader.h \
					audiofilters/flowcontrol.c \
					audiofilters/msvaddtx.c

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

if BUILD_QSA
libmediastreamer_voip_la_SOURCES+=	audiofilters/qsa.c
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

libmediastreamer_voip_la_LIBTOOLFLAGS= --tag=CC

if BUILD_VIDEO

GLDISPLAY_FILES = utils/shaderUtil.c utils/shaderUtil.h utils/debug.h utils/opengles_display.c utils/opengles_display.h

if BUILD_MACOSX
libmediastreamer_voip_la_SOURCES+=	videofilters/videotoolbox.c

noinst_LTLIBRARIES=libqtcapture_cocoa.la
libqtcapture_cocoa_la_LIBTOOLFLAGS= --tag=CC
libqtcapture_cocoa_la_SOURCES = videofilters/qtcapture.m \
					videofilters/msosxdisplay.m \
					$(GLDISPLAY_FILES)

libmediastreamer_voip_la_LIBADD+=libqtcapture_cocoa.la
nodist_libmediastreamer_voip_la_SOURCES = yuv2rgb.fs.h yuv2rgb.fs.h
endif

if BUILD_V4L1
libmediastreamer_voip_la_SOURCES+=	videofilters/msv4l.c
endif

if BUILD_V4L2
libmediastreamer_voip_la_SOURCES+=	videofilters/msv4l2.c
endif

if BUILD_WIN32
libmediastreamer_voip_la_SOURCES+=	videofilters/drawdib-display.c
endif

if BUILD_THEORA
libmediastreamer_voip_la_SOURCES+=	videofilters/theora.c
endif

if BUILD_VP8
libmediastreamer_voip_la_SOURCES+=	videofilters/vp8.c
endif

if BUILD_MATROSKA
libmediastreamer_voip_la_SOURCES+=	videofilters/mkv.c \
					utils/mkv_reader.c utils/mkv_reader.h
endif

if BUILD_AVCODEC
libmediastreamer_voip_la_SOURCES+=	videofilters/videoenc.c \
					videofilters/videodec.c \
					utils/swscale.h \
					utils/ffmpeg-priv.h \
					utils/ffmpeg-priv.c \
					videofilters/h264dec.c \
					videofilters/ffmpegjpegwriter.c
endif

if BUILD_SDL
libmediastreamer_voip_la_SOURCES+=	videofilters/videoout.c
endif

if BUILD_X11_XV
libmediastreamer_voip_la_SOURCES+=	videofilters/x11video.c
endif

if BUILD_X11_GL
libmediastreamer_voip_la_SOURCES+=	videofilters/glxvideo.c $(GLDISPLAY_FILES)
endif

libmediastreamer_voip_la_SOURCES+=	voip/rfc2429.h \
					videofilters/pixconv.c  \
					videofilters/sizeconv.c \
					voip/msvideo.c \
					voip/msvideo_neon.c \
					voip/msvideo_neon.h \
					voip/rfc3984.c \
					voip/videostarter.c \
					voip/vp8rtpfmt.c \
					voip/vp8rtpfmt.h \
					videofilters/mire.c \
					videofilters/nowebcam.c videofilters/ffmpegnowebcam.c voip/nowebcam.h \
					videofilters/extdisplay.c \
					utils/bits_rw.c \
					utils/x11_helper.c \
					utils/stream_regulator.c utils/stream_regulator.h \
					utils/h264utils.c utils/h264utils.h \
					voip/layouts.c voip/layouts.h \
					voip/video_preset_high_fps.c

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
	cd $(srcdir) && \
	awk 'BEGIN { FS="[()]" ; }; /^\t*MS_FILTER_DESC_EXPORT/{ printf("%s\n", $$2) } '  > $(abs_builddir)/basedescs.txt $(libmediastreamer_base_la_SOURCES) && \
	awk 'BEGIN { print("#include \"mediastreamer2/msfilter.h\"\n") } { printf("extern MSFilterDesc %s;\n",$$1) } ' $(abs_builddir)/basedescs.txt > $(abs_builddir)/$@ && \
	awk 'BEGIN { print("MSFilterDesc * ms_base_filter_descs[]={") } { printf("&%s,\n",$$1) } END{ print("NULL\n};\n") } ' $(abs_builddir)/basedescs.txt >> $(abs_builddir)/$@

voipdescs.h:	Makefile $(libmediastreamer_voip_la_SOURCES) $(libqtcapture_cocoa_la_SOURCES)
	cd $(srcdir) && \
	awk 'BEGIN { FS="[()]" ; }; /^\t*MS_FILTER_DESC_EXPORT/{ printf("%s\n", $$2) } '  > $(abs_builddir)/voipdescs.txt $(libmediastreamer_voip_la_SOURCES) $(libqtcapture_cocoa_la_SOURCES) && \
	awk 'BEGIN { print("#include \"mediastreamer2/msfilter.h\"\n") } { printf("extern MSFilterDesc %s;\n",$$1) } ' $(abs_builddir)/voipdescs.txt > $(abs_builddir)/$@ && \
	awk 'BEGIN { print("MSFilterDesc * ms_voip_filter_descs[]={") } { printf("&%s,\n",$$1) } END{ print("NULL\n};\n") } ' $(abs_builddir)/voipdescs.txt >> $(abs_builddir)/$@


libmediastreamer_base_la_LIBADD = $(ORTP_LIBS)
libmediastreamer_base_la_LDFLAGS = -no-undefined -version-info $(LIBMEDIASTREAMER_SO_VERSION)
libmediastreamer_voip_la_LDFLAGS = -no-undefined -version-info $(LIBMEDIASTREAMER_SO_VERSION)

if HAVE_LD_OUTPUT_DEF
libmediastreamer_base_la_LDFLAGS += -Wl,--output-def,libmediastreamer_base-$(LIBMEDIASTREAMER_SO_CURRENT).def
libmediastreamer_voip_la_LDFLAGS += -Wl,--output-def,libmediastreamer_voip-$(LIBMEDIASTREAMER_SO_CURRENT).def
defexecdir = $(libdir)
defexec_DATA = libmediastreamer_base-$(LIBMEDIASTREAMER_SO_CURRENT).def libmediastreamer_voip-$(LIBMEDIASTREAMER_SO_CURRENT).def
CLEANFILES += $(defexec_DATA)

libmediastreamer_base-$(LIBMEDIASTREAMER_SO_CURRENT).def: libmediastreamer_base.la
libmediastreamer_voip-$(LIBMEDIASTREAMER_SO_CURRENT).def: libmediastreamer_voip.la

if BUILD_WIN32
defexec_DATA += libmediastreamer_base-$(LIBMEDIASTREAMER_SO_CURRENT).lib libmediastreamer_voip-$(LIBMEDIASTREAMER_SO_CURRENT).lib
libmediastreamer_base-$(LIBMEDIASTREAMER_SO_CURRENT).lib: libmediastreamer_base-$(LIBMEDIASTREAMER_SO_CURRENT).def libmediastreamer_base.la
	$(DLLTOOL) --dllname libmediastreamer_base-$(LIBMEDIASTREAMER_SO_CURRENT).dll --input-def libmediastreamer_base-$(LIBMEDIASTREAMER_SO_CURRENT).def --output-lib $@ libmediastreamer_base.la
libmediastreamer_voip-$(LIBMEDIASTREAMER_SO_CURRENT).lib: libmediastreamer_voip-$(LIBMEDIASTREAMER_SO_CURRENT).def libmediastreamer_voip.la
	$(DLLTOOL) --dllname libmediastreamer_voip-$(LIBMEDIASTREAMER_SO_CURRENT).dll --input-def libmediastreamer_voip-$(LIBMEDIASTREAMER_SO_CURRENT).def --output-lib $@ libmediastreamer_voip.la
endif
endif

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

#put ORTP_CFLAGS in AM_CPPFLAGS so that inner oRTP submodules is taken in priority rather than installed one, but after MS2 include path to avoid same issue for MS2
AM_CPPFLAGS=\
	-I$(top_srcdir)/include/ \
	-I$(top_srcdir)/src/base \
	-I$(top_srcdir)/src/utils \
	-I$(top_srcdir)/src/voip \
	-I$(top_srcdir)/src/audiofilters \
	-I$(top_srcdir)/src/otherfilters \
	-I$(top_srcdir)/src/videofilters \
	$(ORTP_CFLAGS)

AM_CFLAGS=\
	$(STRICT_OPTIONS) \
	$(STRICT_OPTIONS_CC) \
	$(SPEEX_CFLAGS)  \
	$(GSM_CFLAGS)    \
	$(LIBPULSE_CFLAGS) \
	$(SPANDSP_CFLAGS)  \
	$(MSSILK_CFLAGS) \
	$(PCAP_CFLAGS) \
	$(OPUS_CFLAGS) \
	$(SRTP_CFLAGS) \
	$(BCTOOLBOX_CFLAGS)

AM_CXXFLAGS=\
	$(STRICT_OPTIONS) \
	$(STRICT_OPTIONS_CXX) \
	$(ORTP_CFLAGS)

DEFS=@DEFS@  -DPACKAGE_PLUGINS_DIR=\"$(PACKAGE_PLUGINS_DIR)\" -DLOCALEDIR=\"$(localedir)\" -DMS2_INTERNAL

if MS2_FILTERS
DEFS+= -DMS2_FILTERS
endif

if BUILD_VIDEO
AM_CFLAGS+=$(VIDEO_CFLAGS) $(GLEW_CFLAGS)
endif

if LIBBZRTP
AM_CFLAGS+=$(LIBBZRTP_CFLAGS)
endif

if BUILD_WIN32
libmediastreamer_voip_la_LIBADD+=       -lole32 \
										-loleaut32\
										-lwinmm \
										-luuid
endif

if BUILD_VP8
AM_CFLAGS+=$(VP8_CFLAGS)
libmediastreamer_voip_la_LIBADD+=$(VP8_LIBS)
endif

if BUILD_MATROSKA
AM_CFLAGS+=				$(EBML_CFLAGS) \
					$(MATROSKA_CFLAGS) \
					-DHAVE_MATROSKA

libmediastreamer_voip_la_LIBADD+=	$(MATROSKA_LIBS) \
					$(EBML_LIBS)
endif

if BUILD_VIDEO
libmediastreamer_voip_la_LIBADD+=	$(VIDEO_LIBS) \
					$(THEORA_LIBS) \
					$(GLEW_LIBS)
endif

if BUILD_UPNP
AM_CFLAGS+=$(LIBUPNP_CFLAGS) -D_GNU_SOURCE
libmediastreamer_voip_la_LIBADD+=	$(LIBUPNP_LIBS)
endif

if BUILD_MACOSX
libmediastreamer_voip_la_LDFLAGS+=	-framework Cocoa -framework OpenGL -framework QuartzCore
if BUILD_VIDEO
libmediastreamer_voip_la_LDFLAGS+=	-framework VideoToolbox -framework CoreMedia
endif
endif

AM_OBJCFLAGS=$(AM_CFLAGS)

imgdir=$(datadir)/images/

img_DATA=voip/nowebcamCIF.jpg

make_gitversion_h:
	if test -n "$(GITLOG)" ; then \
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
	cd $(abs_srcdir) && \
	xxd -i yuv2rgb.fs | sed s/}\;/,0x00}\;/ > $(abs_builddir)/yuv2rgb.fs.h

yuv2rgb.vs.h: yuv2rgb.vs
	cd $(abs_srcdir) && \
	xxd -i yuv2rgb.vs | sed s/}\;/,0x00}\;/ > $(abs_builddir)/yuv2rgb.vs.h

#because make bundle serahc in this dir
install-data-local:
	$(MKDIR_P) $(DESTDIR)$(libdir)/mediastreamer/plugins
