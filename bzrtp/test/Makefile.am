noinst_PROGRAMS=bzrtpTest

bzrtpTest_SOURCES=bzrtpTest.c \
				  testUtils.c \
				  bzrtpCryptoTest.c \
				  bzrtpParserTest.c

bzrtpTest_CFLAGS=$(CUNIT_CFLAGS)
bzrtpTest_LDFLAGS=$(CUNIT_LIBS) $(AM_LFFLAGS)
AM_CPPFLAGS=-I$(top_srcdir)/include
LDADD=	$(top_builddir)/src/libbzrtp.la
AM_LDFLAGS= -lm

test: bzrtpTest 
	./bzrtpTest
