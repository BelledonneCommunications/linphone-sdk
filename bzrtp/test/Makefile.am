if ENABLE_TESTS
noinst_PROGRAMS=bzrtpTest

bzrtpTest_SOURCES=bzrtpTest.c \
				  testUtils.c \
				  bzrtpCryptoTest.c \
				  bzrtpParserTest.c


bzrtpTest_CFLAGS=$(CUNIT_CFLAGS)  $(LIBXML2_CFLAGS)
bzrtpTest_LDADD=$(top_builddir)/src/libbzrtp.la $(CUNIT_LIBS) $(LIBXML2_LIBS) -lm

AM_CPPFLAGS=-I$(top_srcdir)/include

test: bzrtpTest 
	./bzrtpTest
endif
