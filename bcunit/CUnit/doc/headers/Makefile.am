dochdrdir = $(prefix)/doc/@PACKAGE@/headers

INCLUDE_FILES = \
	Automated.h \
	Basic.h \
	Console.h \
	CUError.h \
	CUnit.h \
	CUCurses.h \
	MyMem.h \
	TestDB.h \
	TestRun.h \
	Util.h \
	Win.h

dochdr_DATA = $(INCLUDE_FILES)
CLEANFILES = $(INCLUDE_FILES)                   

$(INCLUDE_FILES) : copy_headers

copy_headers:
	cp -uf ../../CUnit/Headers/*.h .
	cp -uf ../../CUnit/Sources/Win/Win.h .

