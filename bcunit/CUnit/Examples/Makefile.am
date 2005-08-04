EXAMPLE_DIRS = . 

if ENABLE_BASIC
EXAMPLE_DIRS += BasicTest
endif

if ENABLE_CONSOLE
EXAMPLE_DIRS += ConsoleTest
endif

if ENABLE_CURSES
EXAMPLE_DIRS += CursesTest
endif

if ENABLE_AUTOMATED
EXAMPLE_DIRS += AutomatedTest
endif

SUBDIRS = ${EXAMPLE_DIRS}

noinst_LIBRARIES = libcunitexamples.a
libcunitexamples_a_SOURCES = ExampleTests.c

