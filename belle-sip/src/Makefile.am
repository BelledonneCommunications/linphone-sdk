
grammar_files= \
						$(srcdir)/belle_sip_message.g \
						$(srcdir)/belle_sdp.g

generated_src= \
	belle_sip_messageParser.c belle_sip_messageParser.h \
	belle_sip_messageLexer.c belle_sip_messageLexer.h \
	belle_sdpParser.c belle_sdpParser.h \
	belle_sdpLexer.c belle_sdpLexer.h

$(builddir)/generated_src_stamp : $(grammar_files)
	$(ANTLR)  -make -fo $(builddir) $(grammar_files)
	touch $(builddir)/generated_src_stamp

$(generated_src)	:	$(builddir)/generated_src_stamp
	

BUILT_SOURCES=$(generated_src)

noinst_LTLIBRARIES=libbellesip_generated.la libbellesip_tls.la
nodist_libbellesip_generated_la_SOURCES=$(generated_src)
libbellesip_generated_la_CFLAGS=$(LESS_STRICT_OPTIONS) $(ANTLR_CFLAGS)

#to disbale warning introduced by compatibility between polar 1.2 and 1.3
libbellesip_tls_la_SOURCES= transports/tls_listeningpoint_polarssl.c transports/tls_channel_polarssl.c 
libbellesip_tls_la_CFLAGS=$(LESS_STRICT_OPTIONS) $(TLS_CFLAGS)  $(LIBBELLESIP_CFLAGS)


lib_LTLIBRARIES=libbellesip.la


libbellesip_la_SOURCES=				\
									clock_gettime.c clock_gettime.h \
									port.c port.h \
									belle_sip_uri_impl.c \
									belle_sip_headers_impl.c \
									belle_sip_utils.c belle_sip_internal.h \
									belle_sip_object.c \
									belle_sip_loop.c \
									belle_sip_resolver.c \
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
									siplistener.c \
									ict.c \
									ist.c \
									nict.c \
									nist.c \
									dialog.c \
									auth_event.c \
									transports/udp_listeningpoint.c \
									transports/udp_channel.c \
									transports/stream_channel.c \
									transports/stream_channel.h \
									transports/stream_listeningpoint.c \
									refresher.c \
									dns.c dns.h

if BUILD_TUNNEL
libbellesip_la_SOURCES+=						transports/tunnel_listeningpoint.c \
									transports/tunnel_channel.c \
									transports/tunnel_wrapper.cc
endif


libbellesip_la_CFLAGS=$(STRICT_OPTIONS) $(ANTLR_CFLAGS) $(TLS_CFLAGS) $(TUNNEL_CFLAGS) $(LIBBELLESIP_CFLAGS)
libbellesip_la_CXXFLAGS=$(STRICT_OPTIONS) $(TLS_CFLAGS) $(TUNNEL_CFLAGS) $(LIBBELLESIP_CFLAGS)

libbellesip_la_LIBADD=libbellesip_generated.la libbellesip_tls.la $(ANTLR_LIBS) $(TLS_LIBS) $(TUNNEL_LIBS)

libbellesip_la_LDFLAGS=-no-undefined $(LDFLAGS)


AM_CPPFLAGS=-I$(top_srcdir)/include

EXTRA_DIST=$(grammar_files)

CLEANFILES=$(generated_src) *.tokens generated_src_stamp

discovery:
	touch specs.c
	$(CC) $(CFLAGS) -include $(top_builddir)/config.h $(ANTLR_CFLAGS) $(CUNIT_CFLAGS) $(TLS_CFLAGS) -E -P -v -dD specs.c

