AUTOMATED_OBJECTS = Automated/Automated.o
CONSOLE_OBJECTS = Console/Console.o
CURSES_OBJECTS = Curses/Curses.o
FRAMEWORK_OBJECTS = Framework/AssertImpl.o Framework/MyMem.o Framework/TestDB.o Framework/TestRun.o Framework/Util.o 

# Default configuration without enabling anything
OBJECT_FILES = $(FRAMEWORK_OBJECTS) $(CONSOLE_OBJECTS) $(CURSES_OBJECTS) $(AUTOMATED_OBJECTS)

COMPILE_DIRS = Framework Console Curses Automated

SUBDIRS = $(COMPILE_DIRS)

lib_LIBRARIES = libcunit.a

libcunit_a_SOURCES = 

libcunit_a_LIBADD = $(OBJECT_FILES)
