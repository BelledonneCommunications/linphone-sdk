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

#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>

#ifndef BELCARD_PUBLIC
#if defined(_MSC_VER)
#define BELCARD_PUBLIC	__declspec(dllexport)
#else
#define BELCARD_PUBLIC
#endif
#endif

using namespace::belr;

namespace belcard {
	class BelCardGeneric;
	class BelCardList;
	class BelCard;
	
	class BelCardParser {
		
	friend class BelCardProperty;
	private:
		Parser<shared_ptr<BelCardGeneric>> *_parser;
		
	protected:
		shared_ptr<BelCardGeneric> _parse(const string &input, const string &rule);
		
	public:
		BELCARD_PUBLIC static shared_ptr<BelCardParser> getInstance();
		BELCARD_PUBLIC BelCardParser();
		BELCARD_PUBLIC ~BelCardParser();
		
		BELCARD_PUBLIC shared_ptr<BelCard> parseOne(const string &input);
		BELCARD_PUBLIC shared_ptr<BelCardList> parse(const string &input);
		BELCARD_PUBLIC shared_ptr<BelCardList> parseFile(const string &filename);
	};
}

#endif
