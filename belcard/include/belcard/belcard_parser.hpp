#ifndef belcard_parser_hpp
#define belcard_parser_hpp

#include "belcard.hpp"

#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

namespace belr {
	class BelCardParser {
	private:
		ABNFGrammarBuilder _grammar_builder;
		shared_ptr<Grammar> _grammar;
		
	public:
		BelCardParser();
		~BelCardParser();
		shared_ptr<belcard::BelCard> parse(const string &input);
	};
}

#endif