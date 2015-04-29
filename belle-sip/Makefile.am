#ACLOCAL_AMFLAGS = -I m4

SUBDIRS=include src tester

EXTRA_DIST = belle-sip.spec

test:
	cd tester 
	$(MAKE) -C tester test

rpm:
	$(MAKE) dist
	TAR_OPTIONS=--wildcards rpmbuild -ta --clean --rmsource --rmspec $(PACKAGE)-$(VERSION).tar.gz

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = belle-sip.pc

EXTRA_DIST=autogen.sh
