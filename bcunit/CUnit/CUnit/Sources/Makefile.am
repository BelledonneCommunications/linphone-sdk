AUTOMATED_OBJECTS = Automated/Automated.o
BASIC_OBJECTS = Basic/Basic.o
CONSOLE_OBJECTS = Console/Console.o
CURSES_OBJECTS = Curses/Curses.o
FRAMEWORK_OBJECTS = Framework/CUError.o Framework/MyMem.o Framework/TestDB.o Framework/TestRun.o Framework/Util.o

OBJECT_FILES = $(FRAMEWORK_OBJECTS) $(BASIC_OBJECTS) $(AUTOMATED_OBJECTS) $(CONSOLE_OBJECTS) $(CURSES_OBJECTS)

TEST_OBJECTS = \
	Framework/CUError_test.o \
	Framework/MyMem_test.o \
	Framework/TestDB_test.o \
	Framework/TestRun_test.o \
	Framework/Util_test.o

COMPILE_DIRS = Framework Automated Basic Console Curses . Test

SUBDIRS = $(COMPILE_DIRS)

lib_LIBRARIES = libcunit.a
libcunit_a_SOURCES = 
libcunit_a_LIBADD = $(OBJECT_FILES)

noinst_LIBRARIES = libcunit_test.a
libcunit_test_a_SOURCES =
libcunit_test_a_LIBADD = $(TEST_OBJECTS)
