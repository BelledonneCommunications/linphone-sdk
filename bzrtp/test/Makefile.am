if ENABLE_TESTS
noinst_PROGRAMS=bzrtpTest

bzrtpTest_SOURCES=bzrtpTest.c \
				  testUtils.c \
				  bzrtpCryptoTest.c \
				  bzrtpParserTest.c

AM_LDFLAGS= -lm
bzrtpTest_CFLAGS=$(CUNIT_CFLAGS)  $(LIBXML2_CFLAGS)
bzrtpTest_LDFLAGS=$(CUNIT_LIBS)  $(LIBXML2_LIBS) $(AM_LDFLAGS)
AM_CPPFLAGS=-I$(top_srcdir)/include
LDADD=	$(top_builddir)/src/libbzrtp.la

test: bzrtpTest 
	./bzrtpTest
endif
