## Process this file with automake to produce Makefile.in

exampledir=${datadir}/@PACKAGE@/Examples/Curses

if ENABLE_EXAMPLES
if ENABLE_CURSES

example_PROGRAMS = CursesTest

CursesTest_SOURCES = CursesTest.c CursesTest.h
CursesTest_LDADD = -lcunit -lcunitexamples -lncurses
CursesTest_LDFLAGS = -L.. -lm
INCLUDES = -I..

example_DATA = README

endif
endif
