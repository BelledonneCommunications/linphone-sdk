EXTRA_DIST=vcards/vcard.vcf vcards/unfoldtest.vcf vcards/foldtest.vcf

bin_PROGRAMS=belcard-folder belcard-unfolder belcard-parser

if BUILD_TESTS

noinst_PROGRAMS=belcard-tester
belcard_tester_SOURCES=belcard-tester.cpp belcard-tester.hpp common/bc_tester_utils.c common/bc_tester_utils.h \
			belcard-general-tester.cpp belcard-identification-tester.cpp belcard-addressing-tester.cpp \
			belcard-vcard-tester.cpp
belcard_tester_LDADD=$(top_builddir)/src/libbelcard.la -lbelr $(CUNIT_LIBS)

belcard_tester_AM_CPPFLAGS=-I$(top_srcdir)/include \
        -I$(top_srcdir)/tester \
        -I$(top_srcdir)/tester/common

endif # BUILD_TESTS

belcard_folder_SOURCES=belcard-folder.cpp
belcard_folder_LDADD=$(top_builddir)/src/libbelcard.la -lbelr

belcard_unfolder_SOURCES=belcard-unfolder.cpp
belcard_unfolder_LDADD=$(top_builddir)/src/libbelcard.la -lbelr

belcard_parser_SOURCES=belcard-parser.cpp
belcard_parser_LDADD=$(top_builddir)/src/libbelcard.la -lbelr

AM_CPPFLAGS=-I$(top_srcdir)/include
