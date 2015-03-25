lib_LTLIBRARIES = libbzrtp.la

libbzrtp_la_LIBADD= $(POLARSSL_LIBS) $(LIBXML2_LIBS)
libbzrtp_la_SOURCES=	bzrtp.c cryptoPolarssl.c cryptoUtils.c packetParser.c zidCache.c stateMachine.c pgpwords.c

libbzrtp_la_LDFLAGS=-fvisibility=hidden -no-undefined

AM_CPPFLAGS= -I$(top_srcdir)/include

AM_CFLAGS= $(POLARSSL_CFLAGS) $(LIBXML2_CFLAGS)


