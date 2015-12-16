/*
	belcard_parser.hpp
	Copyright (C) 2015  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

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