noinst_PROGRAMS=belcard-tester

belcard_tester_SOURCES=belcard-tester.cpp

belcard_tester_LDADD=$(top_builddir)/src/libbelcard.la -lbelr

EXTRA_DIST=vcardgrammar.txt vcardtest.vcf

AM_CPPFLAGS=-I$(top_srcdir)/include
