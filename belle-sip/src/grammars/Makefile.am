
noinst_LTLIBRARIES=libbellesip_generated.la

libbellesip_generated_la_CFLAGS=$(LIBBELLESIP_CFLAGS)  $(ANTLR_CFLAGS) $(STRICT_OPTIONS) $(STRICT_OPTIONS_CC) $(LESS_STRICT_OPTIONS)


nodist_libbellesip_generated_la_SOURCES= \
			belle_sip_messageParser.c belle_sip_messageParser.h \
			belle_sip_messageLexer.c belle_sip_messageLexer.h \
			belle_sdpParser.c belle_sdpParser.h \
			belle_sdpLexer.c belle_sdpLexer.h

$(builddir)/belle_sip_messageParser.c $(builddir)/belle_sip_messageParser.h \
				$(builddir)/belle_sip_messageLexer.c $(builddir)/belle_sip_messageLexer.h  : belle_sip_message.g
	$(ANTLR)  -make -Xmultithreaded -Xconversiontimeout 10000 -fo  $(builddir) $(srcdir)/belle_sip_message.g

$(builddir)/belle_sdpLexer.c \
				$(builddir)/belle_sdpParser.c \
				$(builddir)/belle_sdpLexer.h \
				$(builddir)/belle_sdpParser.h : $(srcdir)/belle_sdp.g
	$(ANTLR)  -make -Xmultithreaded -Xconversiontimeout 10000 -fo $(builddir)  $<


AM_CPPFLAGS=-I$(top_srcdir)/include -I$(top_srcdir)/src

EXTRA_DIST= belle_sdp.g belle_sip_message.g

CLEANFILES=$(nodist_libbellesip_generated_la_SOURCES) *.tokens

.NOTPARALLEL:

