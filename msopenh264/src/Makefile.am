pluginsdir=$(PACKAGE_PLUGINS_DIR)

plugins_LTLIBRARIES=libmsopenh264.la

libmsopenh264_la_SOURCES= \
	msopenh264.cpp msopenh264.h \
	msopenh264dec.cpp msopenh264dec.h \
	msopenh264enc.cpp msopenh264enc.h

libmsopenh264_la_CPPFLAGS= $(MEDIASTREAMER_CFLAGS)

libmsopenh264_la_LIBADD=$(MEDIASTREAMER_LIBS) $(OPENH264_LIBS)

libmsopenh264_la_LDFLAGS=-module -no-undefined

AM_CPPFLAGS=$(MEDIASTREAMER_CFLAGS)
AM_CFLAGS=$(MEDIASTREAMER_CFLAGS)
