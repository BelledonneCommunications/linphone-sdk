exampledir=${datadir}/@PACKAGE@/Examples/Automated

example_PROGRAMS = AutomatedTest

AutomatedTest_SOURCES = AutomatedTest.c AutomatedTest.h 
AutomatedTest_LDADD = -lcunit -lcunitexamples
AutomatedTest_LDFLAGS = -L.. -lm
INCLUDES = -I..

example_DATA = README
