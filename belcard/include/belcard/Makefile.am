belcard_includedir=$(includedir)/belcard

vcard_grammar.hpp: vcard.grammar
	xxd -i vcard.grammar | sed s/}\;/,0x00}\;/ > vcard_grammar.hpp

belcard_include_HEADERS = belcard.hpp belcard_generic.hpp belcard_identification.hpp belcard_parser.hpp vcard_grammar.hpp

EXTRA_DIST=$(belcard_include_HEADERS)
