
lib_LTLIBRARIES=libbelcard.la

vcard_grammar.cpp: vcard.grammar
	echo "#include \"belcard/vcard_grammar.hpp\"" > vcard_grammar.cpp
	xxd -i vcard.grammar | sed s/}\;/,0x00}\;/ >> vcard_grammar.cpp

libbelcard_la_SOURCES=belcard_parser.cpp vcard_grammar.cpp

AM_CPPFLAGS=-I$(top_srcdir)/include

