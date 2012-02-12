if BUILD_TESTS

noinst_PROGRAMS=belle_sip_tester cast_test basic_test belle_sip_object_describe

belle_sip_tester_SOURCES=	belle_sip_tester.c \
				belle_sdp_tester.c \
				belle_sip_uri_tester.c \
				belle_sip_headers_tester.c \
				belle_sip_message_tester.c
belle_sip_tester_CFLAGS=$(CUNIT_CFLAGS)

belle_sip_tester_LDFLAGS=$(CUNIT_LIBS)

cast_test_SOURCES=cast_test.c

basic_test_SOURCES=basic_test.c

belle_sip_object_describe_SOURCES=describe.c

INCLUDES=-I$(top_srcdir)/include -I$(top_srcdir)/src

LDADD=$(top_builddir)/src/libbellesip.la 

AM_LDFLAGS=-export-dynamic 

AM_CFLAGS=$(STRICT_OPTIONS)

endif
