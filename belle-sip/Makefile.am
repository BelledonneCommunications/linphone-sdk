#ACLOCAL_AMFLAGS = -I m4

SUBDIRS=include src tester

all-local: belle-sip.spec

belle-sip.spec: belle-sip.spec.in

test:
	cd tester 
	$(MAKE) -C tester test

rpm:
	$(MAKE) dist
	TAR_OPTIONS=--wildcards rpmbuild -ta --clean --rmsource --rmspec $(PACKAGE)-$(VERSION).tar.gz

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = belle-sip.pc

EXTRA_DIST=belle-sip.spec belle-sip.spec.in autogen.sh
EXTRA_DIST += CMakeLists.txt \
cmake/BelleSIPConfig.cmake.in \
cmake/FindAntlr3.cmake \
cmake/FindZlib.cmake \
config.h.cmake \
include/CMakeLists.txt \
src/CMakeLists.txt \
tester/CMakeLists.txt
