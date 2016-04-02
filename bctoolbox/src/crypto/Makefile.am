

lib_LTLIBRARIES=libbctoolbox.la

libbctoolbox_la_SOURCES=

if ENABLE_POLARSSL

libbctoolbox_la_SOURCES += polarssl.c

endif

if ENABLE_POLARSSL12

libbctoolbox_la_SOURCES += polarssl1.2.c

endif

if ENABLE_MBEDTLS

libbctoolbox_la_SOURCES += mbedtls.c

endif

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src

AM_CFLAGS = $(POLARSSL_CFLAGS) $(MBEDTLS_CFLAGS)

libbctoolbox_la_LIBADD = $(POLARSSL_LIBS) $(MBEDTLS_LIBS)

libbctoolbox_la_LDFLAGS = -version-info $(BCTOOLBOX_SO_VERSION) -no-undefined