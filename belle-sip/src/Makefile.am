
grammar_files= \
						$(srcdir)/belle_sip_message.g \
						$(srcdir)/belle_sdp.g

generated_src= \
	belle_sip_messageParser.c belle_sip_messageParser.h \
	belle_sip_messageLexer.c belle_sip_messageLexer.h \
	belle_sdpParser.c belle_sdpParser.h \
	belle_sdpLexer.c belle_sdpLexer.h

$(builddir)/generated_src_stamp : $(grammar_files)
	$(ANTLR) -make -fo $(builddir) $(grammar_files)
	touch $(builddir)/generated_src_stamp

$(generated_src)	:	$(builddir)/generated_src_stamp
	

BUILT_SOURCES=$(generated_src)

noinst_LTLIBRARIES=libbellesip_generated.la

nodist_libbellesip_generated_la_SOURCES=$(generated_src)

libbellesip_generated_la_CFLAGS=$(LESS_STRICT_OPTIONS) $(ANTLR_CFLAGS)

lib_LTLIBRARIES=libbellesip.la


libbellesip_la_SOURCES=				\
									clock_gettime.c clock_gettime.h \
									belle_sip_uri_impl.c \
									belle_sip_headers_impl.c \
									belle_sip_utils.c belle_sip_internal.h \
									belle_sip_object.c \
									belle_sip_loop.c \
									belle_sip_resolver.c belle_sip_resolver.h \
									belle_sip_parameters.c \
									belle_sdp_impl.c \
									transaction.c \
									listeningpoint.c listeningpoint_internal.h \
									sipstack.c \
									provider.c \
									channel.c channel.h \
									message.c \
									md5.c md5.h \
									auth_helper.c \
									ict.c \
									transports/udp_listeningpoint.c \
									transports/udp_channel.c \
									transports/stream_channel.c \
									transports/stream_listeningpoint.c 

libbellesip_la_CFLAGS=$(STRICT_OPTIONS) $(ANTLR_CFLAGS)

libbellesip_la_LIBADD=libbellesip_generated.la $(ANTLR_LIBS)


INCLUDES=-I$(top_srcdir)/include

EXTRA_DIST=$(grammar_files)

CLEANFILES=$(generated_src) *.tokens generated_src_stamp

discovery:
	touch specs.cpp
	$(CC) $(CFLAGS) $(ANTLR_CFLAGS) $(CUNIT_CFLAGS) -E -P -v -dD specs.cpp
