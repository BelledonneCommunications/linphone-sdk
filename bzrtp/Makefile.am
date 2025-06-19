ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src include test

EXTRA_DIST= \
	CMakeLists.txt \
	cmake/BZRTPConfig.cmake.in \
	cmake/FindXML2.cmake \
	config.h.cmake \
	include/CMakeLists.txt \
	src/CMakeLists.txt \
	test/CMakeLists.txt \
	README.md

test:
	cd test && $(MAKE) test

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libbzrtp.pc


