SUBDIRS=grammars
AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES=libbellesip.la

libbellesip_la_SOURCES=	\
			clock_gettime.c clock_gettime.h \
			port.c port.h \
			parserutils.h \
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
			transports/tls_listeningpoint.c \
			transports/tls_channel.c \
			refresher.c \
			dns.c dns.h \
			belle_sip_dict.c \
			generic-uri.c \
			http-provider.c \
			http-message.c \
			http-listener.c \
			bodyhandler.c


if BUILD_APPLE
libbellesip_la_SOURCES+=backgroundtask.m
endif

if BUILD_TUNNEL
libbellesip_la_SOURCES+=transports/tunnel_listeningpoint.c \
			transports/tunnel_channel.c \
			transports/tunnel_wrapper.cc
endif


libbellesip_la_CFLAGS=$(STRICT_OPTIONS) $(STRICT_OPTIONS_CC) $(ANTLR_CFLAGS) $(TLS_CFLAGS) $(TUNNEL_CFLAGS) $(LIBBELLESIP_CFLAGS)
libbellesip_la_CXXFLAGS=$(STRICT_OPTIONS) $(STRICT_OPTIONS_CXX) $(TLS_CFLAGS) $(TUNNEL_CFLAGS) $(LIBBELLESIP_CFLAGS)

libbellesip_la_LIBADD=grammars/libbellesip_generated.la $(ANTLR_LIBS) $(TLS_LIBS) $(TUNNEL_LIBS) $(ZLIB_LIBS)

libbellesip_la_LDFLAGS=-no-undefined -version-info $(BELLESIP_SO_VERSION)


AM_CPPFLAGS=-I$(top_srcdir)/include -I$(builddir)/grammars

discovery:
	touch specs.c
	$(CC) $(CFLAGS) -include $(top_builddir)/config.h $(ANTLR_CFLAGS) $(BCTOOLBOXTESTER_CFLAGS) $(TLS_CFLAGS) -E -P -v -dD specs.c

