if BUILD_TESTS

noinst_PROGRAMS=belle_sip_uri_tester cast_test

belle_sip_uri_tester_SOURCES=belle_sip_uri_tester.c
cast_test_SOURCES=cast_test.c

INCLUDES=-I$(top_srcdir)/include

LDADD=$(top_builddir)/src/libbellesip.la -lcunit

AM_CFLAGS=$(STRICT_OPTIONS)

endif
