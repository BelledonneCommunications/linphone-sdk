# Generated automatically from Makefile.in by configure.
# Makefile.in generated automatically by automake 1.4-p5 from Makefile.am

# Copyright (C) 1994, 1995-8, 1999, 2001 Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.


SHELL = /bin/sh

srcdir = /home/anil/c/CUnit-1.1-1/Examples/ConsoleTest
top_srcdir = /home/anil/c/CUnit-1.1-1
VPATH = /home/anil/c/CUnit-1.1-1/Examples/ConsoleTest
prefix = /usr/local
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
libexecdir = ${exec_prefix}/libexec
datadir = ${prefix}/share
sysconfdir = ${prefix}/etc
sharedstatedir = ${prefix}/com
localstatedir = ${prefix}/var
libdir = ${exec_prefix}/lib
infodir = ${prefix}/info
mandir = ${prefix}/man
includedir = ${prefix}/include
oldincludedir = /usr/include

DESTDIR =

pkgdatadir = $(datadir)/CUnit
pkglibdir = $(libdir)/CUnit
pkgincludedir = $(includedir)/CUnit

top_builddir = ../..

ACLOCAL = aclocal
AUTOCONF = autoconf
AUTOMAKE = automake
AUTOHEADER = autoheader

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL} $(AM_INSTALL_PROGRAM_FLAGS)
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_SCRIPT = ${INSTALL_PROGRAM}
transform = s,x,x,

NORMAL_INSTALL = :
PRE_INSTALL = :
POST_INSTALL = :
NORMAL_UNINSTALL = :
PRE_UNINSTALL = :
POST_UNINSTALL = :
CC = gcc
MAKEINFO = makeinfo
PACKAGE = CUnit
RANLIB = ranlib
RELEASE = 1
VERSION = 1.1

exampledir = ${datadir}/CUnit-1.1-1/Example/Console

example_PROGRAMS = ConsoleTest

ConsoleTest_SOURCE = ConsoleTest.c
ConsoleTest_HEADER = ConsoleTest.h
ConsoleTest_LDADD = -lcunit

example_DATA = README
mkinstalldirs = $(SHELL) $(top_srcdir)/mkinstalldirs
CONFIG_HEADER = ../../config.h
CONFIG_CLEAN_FILES = 
PROGRAMS =  $(example_PROGRAMS)


DEFS = -DHAVE_CONFIG_H -I. -I$(srcdir) -I../..
CPPFLAGS = 
LDFLAGS = -L/home/anil/c/CUnit-1.1-1/CUnit/Sources
LIBS = -lc 
ConsoleTest_SOURCES = ConsoleTest.c
ConsoleTest_OBJECTS =  ConsoleTest.o
ConsoleTest_DEPENDENCIES = 
ConsoleTest_LDFLAGS = 
CFLAGS = -DRELEASE=1 -Wall -I/home/anil/c/CUnit-1.1-1/CUnit/Headers
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@
DATA =  $(example_DATA)

DIST_COMMON =  README Makefile.am Makefile.in


DISTFILES = $(DIST_COMMON) $(SOURCES) $(HEADERS) $(TEXINFOS) $(EXTRA_DIST)

TAR = gtar
GZIP_ENV = --best
SOURCES = ConsoleTest.c
OBJECTS = ConsoleTest.o

all: all-redirect
.SUFFIXES:
.SUFFIXES: .S .c .o .s
$(srcdir)/Makefile.in: Makefile.am $(top_srcdir)/configure.in $(ACLOCAL_M4) 
	cd $(top_srcdir) && $(AUTOMAKE) --gnu --include-deps Examples/ConsoleTest/Makefile

Makefile: $(srcdir)/Makefile.in  $(top_builddir)/config.status
	cd $(top_builddir) \
	  && CONFIG_FILES=$(subdir)/$@ CONFIG_HEADERS= $(SHELL) ./config.status


mostlyclean-examplePROGRAMS:

clean-examplePROGRAMS:
	-test -z "$(example_PROGRAMS)" || rm -f $(example_PROGRAMS)

distclean-examplePROGRAMS:

maintainer-clean-examplePROGRAMS:

install-examplePROGRAMS: $(example_PROGRAMS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(exampledir)
	@list='$(example_PROGRAMS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo "  $(INSTALL_PROGRAM) $$p $(DESTDIR)$(exampledir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`"; \
	     $(INSTALL_PROGRAM) $$p $(DESTDIR)$(exampledir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`; \
	  else :; fi; \
	done

uninstall-examplePROGRAMS:
	@$(NORMAL_UNINSTALL)
	list='$(example_PROGRAMS)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(exampledir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`; \
	done

.c.o:
	$(COMPILE) -c $<

.s.o:
	$(COMPILE) -c $<

.S.o:
	$(COMPILE) -c $<

mostlyclean-compile:
	-rm -f *.o core *.core

clean-compile:

distclean-compile:
	-rm -f *.tab.c

maintainer-clean-compile:

ConsoleTest: $(ConsoleTest_OBJECTS) $(ConsoleTest_DEPENDENCIES)
	@rm -f ConsoleTest
	$(LINK) $(ConsoleTest_LDFLAGS) $(ConsoleTest_OBJECTS) $(ConsoleTest_LDADD) $(LIBS)

install-exampleDATA: $(example_DATA)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(exampledir)
	@list='$(example_DATA)'; for p in $$list; do \
	  if test -f $(srcdir)/$$p; then \
	    echo " $(INSTALL_DATA) $(srcdir)/$$p $(DESTDIR)$(exampledir)/$$p"; \
	    $(INSTALL_DATA) $(srcdir)/$$p $(DESTDIR)$(exampledir)/$$p; \
	  else if test -f $$p; then \
	    echo " $(INSTALL_DATA) $$p $(DESTDIR)$(exampledir)/$$p"; \
	    $(INSTALL_DATA) $$p $(DESTDIR)$(exampledir)/$$p; \
	  fi; fi; \
	done

uninstall-exampleDATA:
	@$(NORMAL_UNINSTALL)
	list='$(example_DATA)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(exampledir)/$$p; \
	done

tags: TAGS

ID: $(HEADERS) $(SOURCES) $(LISP)
	list='$(SOURCES) $(HEADERS)'; \
	unique=`for i in $$list; do echo $$i; done | \
	  awk '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	here=`pwd` && cd $(srcdir) \
	  && mkid -f$$here/ID $$unique $(LISP)

TAGS:  $(HEADERS) $(SOURCES)  $(TAGS_DEPENDENCIES) $(LISP)
	tags=; \
	here=`pwd`; \
	list='$(SOURCES) $(HEADERS)'; \
	unique=`for i in $$list; do echo $$i; done | \
	  awk '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	test -z "$(ETAGS_ARGS)$$unique$(LISP)$$tags" \
	  || (cd $(srcdir) && etags $(ETAGS_ARGS) $$tags  $$unique $(LISP) -o $$here/TAGS)

mostlyclean-tags:

clean-tags:

distclean-tags:
	-rm -f TAGS ID

maintainer-clean-tags:

distdir = $(top_builddir)/$(PACKAGE)-$(VERSION)/$(subdir)

subdir = Examples/ConsoleTest

distdir: $(DISTFILES)
	@for file in $(DISTFILES); do \
	  d=$(srcdir); \
	  if test -d $$d/$$file; then \
	    cp -pr $$d/$$file $(distdir)/$$file; \
	  else \
	    test -f $(distdir)/$$file \
	    || ln $$d/$$file $(distdir)/$$file 2> /dev/null \
	    || cp -p $$d/$$file $(distdir)/$$file || :; \
	  fi; \
	done
info-am:
info: info-am
dvi-am:
dvi: dvi-am
check-am: all-am
check: check-am
installcheck-am:
installcheck: installcheck-am
install-exec-am:
install-exec: install-exec-am

install-data-am: install-examplePROGRAMS install-exampleDATA
install-data: install-data-am

install-am: all-am
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am
install: install-am
uninstall-am: uninstall-examplePROGRAMS uninstall-exampleDATA
uninstall: uninstall-am
all-am: Makefile $(PROGRAMS) $(DATA)
all-redirect: all-am
install-strip:
	$(MAKE) $(AM_MAKEFLAGS) AM_INSTALL_PROGRAM_FLAGS=-s install
installdirs:
	$(mkinstalldirs)  $(DESTDIR)$(exampledir) $(DESTDIR)$(exampledir)


mostlyclean-generic:

clean-generic:

distclean-generic:
	-rm -f Makefile $(CONFIG_CLEAN_FILES)
	-rm -f config.cache config.log stamp-h stamp-h[0-9]*

maintainer-clean-generic:
mostlyclean-am:  mostlyclean-examplePROGRAMS mostlyclean-compile \
		mostlyclean-tags mostlyclean-generic

mostlyclean: mostlyclean-am

clean-am:  clean-examplePROGRAMS clean-compile clean-tags clean-generic \
		mostlyclean-am

clean: clean-am

distclean-am:  distclean-examplePROGRAMS distclean-compile \
		distclean-tags distclean-generic clean-am

distclean: distclean-am

maintainer-clean-am:  maintainer-clean-examplePROGRAMS \
		maintainer-clean-compile maintainer-clean-tags \
		maintainer-clean-generic distclean-am
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."

maintainer-clean: maintainer-clean-am

.PHONY: mostlyclean-examplePROGRAMS distclean-examplePROGRAMS \
clean-examplePROGRAMS maintainer-clean-examplePROGRAMS \
uninstall-examplePROGRAMS install-examplePROGRAMS mostlyclean-compile \
distclean-compile clean-compile maintainer-clean-compile \
uninstall-exampleDATA install-exampleDATA tags mostlyclean-tags \
distclean-tags clean-tags maintainer-clean-tags distdir info-am info \
dvi-am dvi check check-am installcheck-am installcheck install-exec-am \
install-exec install-data-am install-data install-am install \
uninstall-am uninstall all-redirect all-am all installdirs \
mostlyclean-generic distclean-generic clean-generic \
maintainer-clean-generic clean mostlyclean distclean maintainer-clean


# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
