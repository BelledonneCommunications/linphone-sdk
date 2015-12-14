#ifndef belcard_parser_hpp
#define belcard_parser_hpp

#include "belcard.hpp"
#include "belcard_params.hpp"

#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

using namespace::belr;

namespace belcard {
	class BelCardParser {
	private:
		ABNFGrammarBuilder _grammar_builder;
		shared_ptr<Grammar> _grammar;
		
		shared_ptr<BelCardGeneric> _parse(const string &input, const string &rule);
		
	public:
		BelCardParser();
		~BelCardParser();
		
		shared_ptr<BelCard> parseOne(const string &input);
		shared_ptr<BelCardList> parse(const string &input);
		shared_ptr<BelCardList> parseFile(const string &filename);
	};
}

#endif