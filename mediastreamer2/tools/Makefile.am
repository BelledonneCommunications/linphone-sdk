if BUILD_TOOLS
if !BUILD_IOS

noinst_PROGRAMS=mtudiscover tones

if ORTP_ENABLED
if MS2_FILTERS

noinst_PROGRAMS+=echo ring bench

if BUILD_VIDEO
noinst_PROGRAMS+=videodisplay test_x11window mkvstream
endif

endif MS2_FILTERS
endif ORTP_ENABLED


echo_SOURCES=echo.c
ring_SOURCES=ring.c
videodisplay_SOURCES=videodisplay.c
mtudiscover_SOURCES=mtudiscover.c
mkvstream_SOURCES=mkvstream.c
bench_SOURCES=bench.c
test_x11window_SOURCES=test_x11window.c
tones_SOURCES=tones.c


TEST_DEPLIBS=\
	$(top_builddir)/src/libmediastreamer_base.la

if BUILD_VOIP_LIBRARY
TEST_DEPLIBS+= $(top_builddir)/src/libmediastreamer_voip.la
endif

TEST_DEPLIBS+=\
	$(ORTP_LIBS) \
	$(ALSA_LIBS) \
	$(ARTS_LIBS) \
	$(SPEEX_LIBS) \
	$(GSM_LIBS) \
	$(THEORA_LIBS) \
	$(VP8_LIBS) \
	$(VIDEO_LIBS) \
	$(PCAP_LIBS) \
	$(BCTOOLBOX_LIBS)

if ORTP_ENABLED
if MS2_FILTERS
bin_PROGRAMS=mediastream msaudiocmp

if ENABLE_PCAP
bin_PROGRAMS+=pcap_playback
endif

mediastream_SOURCES = mediastream.c common.c common.h
pcap_playback_SOURCES = pcap_playback.c common.c common.h

mediastream_LDADD=$(TEST_DEPLIBS)
pcap_playback_LDADD=$(TEST_DEPLIBS)

msaudiocmp_SOURCES = msaudiocmp.c
msaudiocmp_LDADD=$(TEST_DEPLIBS)

if BUILD_MACOSX

noinst_LTLIBRARIES=libmediastream_cocoa.la

libmediastream_cocoa_la_LIBTOOLFLAGS = --tag=CC
libmediastream_cocoa_la_SOURCES = mediastream_cocoa.m

mediastream_LDADD+=libmediastream_cocoa.la
pcap_playback_LDADD+=libmediastream_cocoa.la

endif BUILD_MACOSX


endif MS2_FILTERS
endif ORTP_ENABLED

LDADD=$(TEST_DEPLIBS)

AM_CPPFLAGS=\
	-I$(top_srcdir)/include/ \
	$(ORTP_CFLAGS) 

if !ORTP_ENABLED
AM_CPPFLAGS+=-I$(top_srcdir)/src/ortp-deps/
endif

AM_CFLAGS=\
	$(STRICT_OPTIONS) \
	$(STRICT_OPTIONS_CC) \
	$(VIDEO_CFLAGS) \
	$(PCAP_CFLAGS)

AM_LDFLAGS=-export-dynamic
if BUILD_MACOSX
AM_LDFLAGS+=-Wl,-rpath -Wl,$(libdir)
endif
endif !BUILD_IOS
endif
