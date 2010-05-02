
plugindir = $(libdir)/mediastreamer/plugins

plugin_LTLIBRARIES=libmsamr.la

libmsamr_la_SOURCES=msamr.c

libmsamr_la_CFLAGS = $(MEDIASTREAMER_CFLAGS) $(OPENCORE_AMRNB_CFLAGS) 

libmsamr_la_LIBADD = $(MEDIASTREAMER_LIBS) $(OPENCORE_AMRNB_LIBS)

libmsamr_la_LDFLAGS = -module
