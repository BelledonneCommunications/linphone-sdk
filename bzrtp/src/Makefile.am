lib_LTLIBRARIES = libbzrtp.la

libbzrtp_la_LIBADD= $(LIBXML2_LIBS)  $(BCTOOLBOX_LIBS)
libbzrtp_la_SOURCES= bzrtp.c cryptoUtils.c packetParser.c zidCache.c stateMachine.c pgpwords.c 

AM_CPPFLAGS= -I$(top_srcdir)/include 

AM_CFLAGS= $(LIBXML2_CFLAGS) $(BCTOOLBOX_CFLAGS)

libbzrtp_la_LDFLAGS=-fvisibility=hidden -no-undefined


