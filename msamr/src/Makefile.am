
plugindir = $(libdir)/mediastreamer/plugins

plugin_LTLIBRARIES = libmsamr.la

libmsamr_la_CFLAGS = $(MEDIASTREAMER_CFLAGS) $(OPENCORE_AMRNB_CFLAGS) $(OPENCORE_AMRWB_CFLAGS) $(VO_AMRWBENC_CFLAGS)

libmsamr_la_LIBADD = $(MEDIASTREAMER_LIBS) $(OPENCORE_AMRNB_LIBS) $(OPENCORE_AMRWB_LIBS) $(VO_AMRWBENC_LIBS)

libmsamr_la_LDFLAGS = -module -no-undefined

libmsamr_la_SOURCES = msamr.c

if BUILD_AMRNB
libmsamr_la_SOURCES += amrnb.c

libmsamr_la_CFLAGS += -DHAVE_AMRNB=1
endif

if BUILD_AMRWB
libmsamr_la_SOURCES += amrwb.c

libmsamr_la_CFLAGS += -DHAVE_AMRWB=1
endif
