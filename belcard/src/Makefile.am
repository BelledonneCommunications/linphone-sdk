
lib_LTLIBRARIES=libbelcard.la

vcard_grammar.cpp: vcard.grammar
	echo "#include \"belcard/vcard_grammar.hpp\"" > vcard_grammar.cpp
	xxd -i vcard.grammar | sed s/}\;/,0x00}\;/ >> vcard_grammar.cpp

libbelcard_la_SOURCES=belcard_parser.cpp \
			belcard_params.cpp \
			belcard_property.cpp \
			belcard.cpp \
			belcard_general.cpp \
			belcard_identification.cpp \
			belcard_addressing.cpp \
			belcard_communication.cpp \
			belcard_geographical.cpp \
			belcard_organizational.cpp \
			belcard_explanatory.cpp \
			belcard_security.cpp \
			belcard_calendar.cpp \
			vcard_grammar.cpp 

AM_CPPFLAGS=-I$(top_srcdir)/include

