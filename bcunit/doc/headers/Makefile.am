## Process this file with automake to produce Makefile.in

dochdrdir = $(prefix)/doc/@PACKAGE@/headers

INCLUDE_FILES = \
	Automated.h \
	Basic.h \
	Console.h \
	CUError.h \
	BCUnit.h \
	BCUnit_intl.h \
	CUCurses.h \
	MyMem.h \
	TestDB.h \
	TestRun.h \
	Util.h \
	Win.h

#dochdr_DATA = $(INCLUDE_FILES)
CLEANFILES = $(INCLUDE_FILES)

$(INCLUDE_FILES) : copy_headers

copy_headers:
	cp -f $(top_srcdir)/BCUnit/Headers/*.h .
	cp -f $(top_srcdir)/BCUnit/Sources/Win/Win.h .

