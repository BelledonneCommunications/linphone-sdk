ANTLR=antlr3

grammar_files= \
						$(srcdir)/belle_sip_uri.g

generated_src= \
	belle_sip_uriParser.c belle_sip_uriParser.h \
	belle_sip_uriLexer.c belle_sip_uriLexer.h

$(generated_src)	:	$(grammar_files)
	$(ANTLR) -make -fo $(builddir) $(grammar_files)

BUILT_SOURCES=$(generated_src)

noinst_LTLIBRARIES=libbellesip_generated.la

nodist_libbellesip_generated_la_SOURCES=$(generated_src)

libbellesip_generated_la_CFLAGS=$(LESS_STRICT_OPTIONS)

lib_LTLIBRARIES=libbellesip.la


libbellesip_la_SOURCES=belle_sip_uri_impl.c \
									belle_sip_utils.c belle_sip_utils.h

libbellesip_la_CFLAGS=$(STRICT_OPTIONS)

libbellesip_la_LIBADD=libbellesip_generated.la -lantlr3c

INCLUDES=-I$(top_srcdir)/include


EXTRA_DIST=$(grammar_files)

CLEANFILES=$(generated_src) *.tokens
