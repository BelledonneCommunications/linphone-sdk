
noinst_LTLIBRARIES=libbellesip_generated.la

libbellesip_generated_la_CFLAGS=$(ANTLR_CFLAGS)

						
nodist_libbellesip_generated_la_SOURCES= \
			belle_sip_messageParser.c belle_sip_messageParser.h \
			belle_sip_lexer.c belle_sip_lexer.h \
			belle_sdpParser.c belle_sdpParser.h \
			belle_sdpLexer.c belle_sdpLexer.h \
	 		belle_sip_message_http.c  belle_sip_message_http.h \
	 		belle_sip_message_sip.c  belle_sip_message_sip.h \
	 		belle_sip_message_common.c  belle_sip_message_common.h 	

.NOTPARALLEL 	$(builddir)/belle_sip_lexer.c \
 				$(builddir)/ belle_sip_lexer.h : $(srcdir)/belle_sip_lexer.g
	$(ANTLR)  -make -fo $(builddir) $<
	
.NOTPARALLEL 	$(builddir)/belle_sip_messageParser.c $(builddir)/belle_sip_messageParser.h \
				$(builddir)/belle_sip_message_http.c  $(builddir)/belle_sip_message_http.h \
				$(builddir)/belle_sip_message_sip.c  $(builddir)/belle_sip_message_sip.h \
				$(builddir)/belle_sip_message_common.c $(builddir)/belle_sip_message_common.h  : belle_sip_message.g sip.g http.g common.g
	$(ANTLR)  -make -fo $(builddir) $(srcdir)/belle_sip_message.g

.NOTPARALLEL 	$(builddir)/belle_sdpLexer.c \
				$(builddir)/belle_sdpParser.c \
				$(builddir)/belle_sdpLexer.h \
				$(builddir)/belle_sdpParser.h : $(srcdir)/belle_sdp.g
	$(ANTLR)  -make -fo $(builddir)  $<


AM_CPPFLAGS=-I$(top_srcdir)/include -I$(top_srcdir)/src 

EXTRA_DIST= belle_sip_lexer.g belle_sdp.g belle_sip_message.g sip.g http.g common.g 

CLEANFILES=$(nodist_libbellesip_generated_la_SOURCES) *.tokens 

