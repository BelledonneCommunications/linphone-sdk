if ENABLE_TESTS
noinst_PROGRAMS=bzrtpTest

bzrtpTest_SOURCES=bzrtpTest.c \
				  testUtils.c testUtils.h \
				  bzrtpCryptoTest.c bzrtpCryptoTest.h \
				  bzrtpParserTest.c bzrtpParserTest.h\
				bzrtpConfigsTest.c \
				bzrtpZidCacheTest.c

bzrtpTest_CFLAGS=$(BCTOOLBOXTESTER_CFLAGS) $(LIBXML2_CFLAGS)
bzrtpTest_LDADD=$(top_builddir)/src/libbzrtp.la $(BCTOOLBOX_LIBS) $(BCTOOLBOXTESTER_LIBS) $(SQLITE3_LIBS) $(LIBXML2_LIBS) -lm

AM_CPPFLAGS=-I$(top_srcdir)/include

test: bzrtpTest 
	./bzrtpTest
endif
