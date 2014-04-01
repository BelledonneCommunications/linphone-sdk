pluginsdir=$(PACKAGE_PLUGINS_DIR)

plugins_LTLIBRARIES=libmsopenh264.la

libmsopenh264_la_SOURCES=msopenh264.c

libmsopenh264_la_LIBADD=$(MEDIASTREAMER_LIBS) $(OPENH264_LIBS)

libmsopenh264_la_LDFLAGS=-module -no-undefined

AM_CFLAGS=$(MEDIASTREAMER_CFLAGS)
