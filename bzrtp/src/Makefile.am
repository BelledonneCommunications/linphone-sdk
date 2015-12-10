lib_LTLIBRARIES = libbzrtp.la

libbzrtp_la_LIBADD= $(LIBXML2_LIBS)
libbzrtp_la_SOURCES=	bzrtp.c cryptoUtils.c packetParser.c zidCache.c stateMachine.c pgpwords.c

AM_CPPFLAGS= -I$(top_srcdir)/include

AM_CFLAGS= $(LIBXML2_CFLAGS)

if LIBPOLARSSL
libbzrtp_la_LIBADD += $(POLARSSL_LIBS)
libbzrtp_la_SOURCES +=	cryptoPolarssl.c
AM_CFLAGS += $(POLARSSL_CFLAGS)
endif

if LIBMBEDTLS
libbzrtp_la_LIBADD += $(MBEDTLS_LIBS)
libbzrtp_la_SOURCES +=	cryptoMbedtls.c
AM_CFLAGS += $(MBEDTLS_CFLAGS)
endif

libbzrtp_la_LDFLAGS=-fvisibility=hidden -no-undefined


