ACLOCAL_AMFLAGS = -I m4

pluginsdir = $(libdir)/mediastreamer/plugins

plugins_LTLIBRARIES=libmscodec2.la

libmscodec2_la_SOURCES = mscodec2.c

libmscodec2_la_CFLAGS = $(ORTP_CFLAGS) $(MEDIASTREAMER_CFLAGS) $(CODEC2_CFLAGS) \
	$(MSCODEC2_CFLAGS)

libmscodec2_la_LIBADD = $(ORTP_LIBS) $(MEDIASTREAMER_LIBS) $(CODEC2_LIBS)

.phony: rpm

rpm:
	$(MAKE) dist
	TAR_OPTIONS=--wildcards rpmbuild -ta --clean --rmsource --rmspec $(PACKAGE)-$(VERSION).tar.gz
