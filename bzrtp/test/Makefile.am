noinst_PROGRAMS=bzrtpTest

bzrtpTest_SOURCES=bzrtpTest.c \
				  testUtils.c \
				  bzrtpCryptoTest.c \
				  bzrtpParserTest.c

AM_LDFLAGS= -lm
bzrtpTest_CFLAGS=$(CUNIT_CFLAGS)
bzrtpTest_LDFLAGS=$(CUNIT_LIBS) $(AM_LDFLAGS)
AM_CPPFLAGS=-I$(top_srcdir)/include
LDADD=	$(top_builddir)/src/libbzrtp.la

test: bzrtpTest 
	./bzrtpTest
