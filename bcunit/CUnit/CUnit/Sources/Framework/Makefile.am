noinst_LTLIBRARIES = libcunitfmk.la
noinst_LIBRARIES = libcunittestfmk.a

TEST_DEFINES = -DCUNIT_BUILD_TESTS -DCUNIT_DO_NOT_DEFINE_UNLESS_BUILDING_TESTS -DMEMTRACE
TEST_INCLUDES = -I../Test


%_test.o: %.c
	$(COMPILE) $(TEST_INCLUDES) $(TEST_DEFINES) -o $@ -c $<

libcunitfmk_la_SOURCES = \
	CUError.c \
	MyMem.c \
	TestDB.c \
	TestRun.c \
	Util.c

TEST_OBJECTS = \
	CUError_test.o \
	MyMem_test.o \
	TestDB_test.o \
	TestRun_test.o \
	Util_test.o

libcunittestfmk_a_SOURCES =
libcunittestfmk_a_LIBADD = $(TEST_OBJECTS)
