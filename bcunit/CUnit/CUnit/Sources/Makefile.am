AUTOMATED_OBJECTS = Automated/Automated.lo
BASIC_OBJECTS = Basic/Basic.lo
CONSOLE_OBJECTS = Console/Console.lo
CURSES_OBJECTS = Curses/Curses.lo
FRAMEWORK_OBJECTS = Framework/CUError.lo Framework/MyMem.lo Framework/TestDB.lo Framework/TestRun.lo Framework/Util.lo

OBJECT_FILES = $(FRAMEWORK_OBJECTS)
COMPILE_DIRS = Framework

if ENABLE_BASIC
OBJECT_FILES += $(BASIC_OBJECTS)
COMPILE_DIRS += Basic
endif

if ENABLE_CONSOLE
OBJECT_FILES += $(CONSOLE_OBJECTS)
COMPILE_DIRS += Console
endif

if ENABLE_AUTOMATED
OBJECT_FILES += $(AUTOMATED_OBJECTS)
COMPILE_DIRS += Automated
endif

if ENABLE_CURSES
OBJECT_FILES += $(CURSES_OBJECTS)
COMPILE_DIRS += Curses
endif

COMPILE_DIRS += . 

if ENABLE_TEST
TEST_OBJECTS = \
	Framework/CUError_test.o \
	Framework/MyMem_test.o \
	Framework/TestDB_test.o \
	Framework/TestRun_test.o \
	Framework/Util_test.o
COMPILE_DIRS += Test
endif


SUBDIRS = $(COMPILE_DIRS)

lib_LTLIBRARIES = libcunit.la
libcunit_la_SOURCES = 
libcunit_la_LIBADD = $(OBJECT_FILES)

noinst_LIBRARIES = libcunit_test.a

libcunit_test_a_SOURCES =
libcunit_test_a_LIBADD = $(TEST_OBJECTS)
