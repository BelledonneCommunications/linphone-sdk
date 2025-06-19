## Process this file with automake to produce Makefile.in

if ENABLE_TEST
testdir=${datadir}/@PACKAGE@/Test

test_PROGRAMS = test_bcunit
test_bcunit_SOURCES = test_bcunit.c
test_bcunit_LDADD = ../libbcunit_test.a
test_bcunit_LDFLAGS = -L..

INCLUDES = -I../../Headers -DMEMTRACE -DBCUNIT_BUILD_TESTS -DBCUNIT_DO_NOT_DEFINE_UNLESS_BUILDING_TESTS
endif
