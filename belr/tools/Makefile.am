bin_PROGRAMS=belr-parse
noinst_PROGRAMS=belr-demo

belr_parse_SOURCES=belr-parse.cc

belr_parse_LDADD=$(top_builddir)/src/libbelr.la $(BCTOOLBOX_LIBS)

belr_demo_SOURCES=belr-demo.cc

belr_demo_LDADD=$(top_builddir)/src/libbelr.la $(BCTOOLBOX_LIBS)

AM_CPPFLAGS=-I$(top_srcdir)/include
