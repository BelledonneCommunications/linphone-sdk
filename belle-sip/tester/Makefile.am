
if BUILD_TESTS

noinst_PROGRAMS=belle_sip_tester  belle_sip_object_describe
TESTS=belle_sip_tester

belle_sip_tester_SOURCES=	belle_sip_tester.c \
				belle_sdp_tester.c \
				belle_sip_uri_tester.c \
				belle_sip_headers_tester.c \
				belle_sip_message_tester.c \
				auth_helper_tester.c \
				cast_test.c \
				belle_sip_register_tester.c \
				belle_sip_dialog_tester.c \
				belle_sip_refresher_tester.c

belle_sip_tester_CFLAGS=$(CUNIT_CFLAGS) $(STRICT_OPTIONS)

belle_sip_tester_LDFLAGS=$(CUNIT_LIBS) $(AM_LDFLAGS)

belle_sip_object_describe_SOURCES=describe.c

AM_CPPFLAGS=-I$(top_srcdir)/include -I$(top_srcdir)/src

LDADD=$(top_builddir)/src/libbellesip.la 

AM_LDFLAGS=-no-undefined -export-dynamic 

AM_CFLAGS=$(STRICT_OPTIONS) 


test: belle_sip_tester
	./belle_sip_tester $(TEST_OPTIONS)
	
endif
