dochdrdir = $(prefix)/doc/@PACKAGE@/headers

INCLUDE_FILES = \
	Automated.h \
	Basic.h \
	Console.h \
	CUError.h \
	CUnit.h \
	Curses.h \
	MyMem.h \
	TestDB.h \
	TestRun.h \
	Util.h \
	Win.h

dochdr_DATA = $(INCLUDE_FILES)
CLEANFILES = $(INCLUDE_FILES)

#Automated.h : copy_headers
#Basic.h     : copy_headers
#Console.h   : copy_headers
#CUError.h   : copy_headers
#CUnit.h     : copy_headers
#Curses.h    : copy_headers
#MyMem.h     : copy_headers
#TestDB.h    : copy_headers
#TestRun.h   : copy_headers
#Util.h      : copy_headers
$(INCLUDE_FILES) : copy_headers

copy_headers:
	cp -uf ../../CUnit/Headers/*.h .
	cp -uf ../../CUnit/Sources/Win/Win.h .

