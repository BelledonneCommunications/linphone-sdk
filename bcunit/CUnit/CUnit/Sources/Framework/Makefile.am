noinst_LIBRARIES = libcunitfmk.a libcunittestfmk.a

libcunitfmk_a_SOURCES = \
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
TEST_DEFINES = -DCUNIT_BUILD_TESTS -DCUNIT_DO_NOT_DEFINE_UNLESS_BUILDING_TESTS -DMEMTRACE
TEST_INCLUDES = -I../Test

CUError_test.o: CUError.c
	$(COMPILE) $(TEST_INCLUDES) $(TEST_DEFINES) -o CUError_test.o -c CUError.c

MyMem_test.o: MyMem.c
	$(COMPILE) $(TEST_INCLUDES) $(TEST_DEFINES) -o MyMem_test.o -c MyMem.c

TestDB_test.o: TestDB.c
	$(COMPILE) $(TEST_INCLUDES) $(TEST_DEFINES) -o TestDB_test.o -c TestDB.c

TestRun_test.o: TestRun.c
	$(COMPILE) $(TEST_INCLUDES) $(TEST_DEFINES) -o TestRun_test.o -c TestRun.c

Util_test.o: Util.c
	$(COMPILE) $(TEST_INCLUDES) $(TEST_DEFINES) -o Util_test.o -c Util.c
