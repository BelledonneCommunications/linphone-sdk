if ENABLE_TESTS

noinst_PROGRAMS=echo ring mtudiscover bench tones

if BUILD_VIDEO
noinst_PROGRAMS+=videodisplay test_x11window
endif



echo_SOURCES=echo.c
ring_SOURCES=ring.c
videodisplay_SOURCES=videodisplay.c
mtudiscover_SOURCES=mtudiscover.c
bench_SOURCES=bench.c
test_x11window_SOURCES=test_x11window.c
tones_SOURCES=tones.c


TEST_DEPLIBS=\
	$(top_builddir)/src/libmediastreamer.la \
	$(ORTP_LIBS) \
	$(ALSA_LIBS) \
	$(ARTS_LIBS) \
	$(SPEEX_LIBS) \
	$(GSM_LIBS) \
	$(THEORA_LIBS) \
	$(VP8_LIBS) \
	$(VIDEO_LIBS)

bin_PROGRAMS=mediastream

mediastream_SOURCES = mediastream.c 

mediastream_LDADD=$(TEST_DEPLIBS)

if BUILD_MACOSX

noinst_LTLIBRARIES=libmediastream_cocoa.la

libmediastream_cocoa_la_SOURCES = mediastream_cocoa.m

mediastream_LDADD+=libmediastream_cocoa.la

endif

LDADD=$(TEST_DEPLIBS)

INCLUDES=-I$(top_srcdir)/include/

AM_CFLAGS=$(ORTP_CFLAGS) $(STRICT_OPTIONS) $(VIDEO_CFLAGS)
AM_LDFLAGS=-export-dynamic

endif 
