## Process this file with automake to produce Makefile.in

SHARED_SOURCES = \
	CUError.c \
	MyMem.c \
	TestDB.c \
	TestRun.c \
	Util.c

noinst_LTLIBRARIES = libbcunitfmk.la
libbcunitfmk_la_SOURCES = $(SHARED_SOURCES)

AM_CPPFLAGS=-I$(top_srcdir)/BCUnit/Headers

if ENABLE_TEST

TEST_DEFINES = -DBCUNIT_BUILD_TESTS -DBCUNIT_DO_NOT_DEFINE_UNLESS_BUILDING_TESTS -DMEMTRACE
TEST_INCLUDES = -I../Test

%_test.o: %.c
	$(COMPILE) $(TEST_INCLUDES) $(TEST_DEFINES) -o $@ -c $<

TEST_OBJECTS = \
	CUError_test.o \
	MyMem_test.o \
	TestDB_test.o \
	TestRun_test.o \
	Util_test.o

noinst_LIBRARIES = libbcunittestfmk.a
libbcunittestfmk_a_SOURCES =
libbcunittestfmk_a_LIBADD = $(TEST_OBJECTS)
endif
