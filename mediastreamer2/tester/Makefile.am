EXTRA_DIST=sounds/arpeggio_8000_mono.wav  sounds/chimes_48000_stereo.wav \
	sounds/nylon_48000_mono.wav  sounds/piano_8000_stereo.wav \
	sounds/bird_44100_stereo.wav   sounds/laserrocket_16000_mono.wav  \
	sounds/owl_44100_mono.wav    sounds/punch_16000_stereo.wav


if BUILD_TESTS

noinst_PROGRAMS=mediastreamer2_tester

mediastreamer2_tester_SOURCES=	\
	mediastreamer2_tester.c mediastreamer2_tester.h \
	mediastreamer2_tester_private.c mediastreamer2_tester_private.h \
	mediastreamer2_basic_audio_tester.c \
	mediastreamer2_sound_card_tester.c \
	mediastreamer2_adaptive_tester.c \
	mediastreamer2_audio_stream_tester.c \
	mediastreamer2_text_stream_tester.c \
	mediastreamer2_framework_tester.c \
	mediastreamer2_player_tester.c \
	mediastreamer2_neon_tester.c \
	mediastreamer2_jitterbuffer_tester.c

if ENABLE_PCAP
mediastreamer2_tester_SOURCES += mediastreamer2_codec_impl_testers.c
endif

AM_CPPFLAGS=\
	-DBC_CONFIG_FILE=\"mediastreamer-config.h\" \
	-I$(top_srcdir)/ \
	-I$(top_srcdir)/include/ \
	-I$(top_srcdir)/src/base \
	-I$(top_srcdir)/src/utils \
	-I$(top_srcdir)/src/voip \
	-I$(top_srcdir)/src/audiofilters \
	-I$(top_srcdir)/src/otherfilters \
	-I$(top_srcdir)/src/videofilters \
	-I$(top_srcdir)/src \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/tester \
	-I$(top_srcdir)/tester/common \
	$(ORTP_CFLAGS) \
	$(SPEEX_CFLAGS)

LDADD=$(top_builddir)/src/libmediastreamer_base.la \
	$(top_builddir)/src/libmediastreamer_voip.la \
	$(ORTP_LIBS) \
	$(BCTOOLBOXTESTER_LIBS) \
	$(SPEEX_LIBS)

if BUILD_MACOSX
noinst_LTLIBRARIES=libmediastreamer2_tester_cocoa.la
libmediastreamer2_tester_cocoa_la_LIBTOOLFLAGS = --tag=CC
libmediastreamer2_tester_cocoa_la_SOURCES = mediastreamer2_tester_cocoa.m
LDADD+=libmediastreamer2_tester_cocoa.la
endif BUILD_MACOSX

if BUILD_VIDEO
mediastreamer2_tester_SOURCES+=	mediastreamer2_video_stream_tester.c
endif


AM_LDFLAGS=-no-undefined -export-dynamic
#AM_LDFLAGS+=-Wl,-rpath -Wl,$(libdir)

AM_CFLAGS=$(STRICT_OPTIONS) $(STRICT_OPTIONS_CC) $(ORTP_CFLAGS) $(BCTOOLBOXTESTER_CFLAGS)

if BUILD_VIDEO
AM_CFLAGS+=$(VIDEO_CFLAGS) $(GLEW_CFLAGS)
endif

if BUILD_MATROSKA
AM_CFLAGS+=-DHAVE_MATROSKA
endif


if BUILD_IOS

noinst_LTLIBRARIES=libmediastreamer2_tester_ios.la
libmediastreamer2_tester_ios_la_SOURCES      = mediastreamer2_tester_ios.m
libmediastreamer2_tester_ios_la_LIBTOOLFLAGS = --tag=CC
LDADD+=libmediastreamer2_tester_ios.la

AM_CFLAGS    += $(SPEEX_CFLAGS)
AM_OBJCFLAGS  = $(AM_CFLAGS)
endif

test: mediastreamer2_tester
	./mediastreamer2_tester $(TEST_OPTIONS)

else

test:
	@echo "bctoolbox-tester must be installed to be able to run the tests!"

endif
