bin_PROGRAMS=belcard-folder belcard-unfolder
noinst_PROGRAMS=belcard-tester

belcard_tester_SOURCES=belcard-tester.cpp
belcard_tester_LDADD=$(top_builddir)/src/libbelcard.la -lbelr

belcard_folder_SOURCES=belcard-folder.cpp
belcard_folder_LDADD=$(top_builddir)/src/libbelcard.la -lbelr

belcard_unfolder_SOURCES=belcard-unfolder.cpp
belcard_unfolder_LDADD=$(top_builddir)/src/libbelcard.la -lbelr

EXTRA_DIST=vcardgrammar.txt vcardtest.vcf

AM_CPPFLAGS=-I$(top_srcdir)/include
