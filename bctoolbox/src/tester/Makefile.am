lib_LTLIBRARIES=libbctoolbox-tester.la

libbctoolbox_tester_la_SOURCES = utils.c

libbctoolbox_tester_la_LIBADD = $(CUNIT_LIBS)

libbctoolbox_tester_la_LDFLAGS= -version-info $(BCTOOLBOX_SO_VERSION) -no-undefined

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src $(CUNIT_CFLAGS)