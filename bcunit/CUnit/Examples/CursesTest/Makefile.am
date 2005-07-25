exampledir=${datadir}/@PACKAGE@/Examples/Curses

example_PROGRAMS = CursesTest

CursesTest_SOURCES = CursesTest.c CursesTest.h
CursesTest_LDADD = -lcunit -lcunitexamples -lncurses
CursesTest_LDFLAGS = -L.. -lm
INCLUDES = -I..

example_DATA = README
