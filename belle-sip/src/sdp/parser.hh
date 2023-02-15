/*
 * Copyright (c) 2012-2021 Belledonne Communications SARL.
 *
 * This file is part of belle-sip.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <belr/abnf.h>
#include <belr/grammarbuilder.h>

using namespace std;

namespace bellesip {
namespace SDP {
class Parser {
public:
	static Parser *getInstance();
	void *parse(const string &input, const string &rule);

private:
	static Parser *instance;
	Parser();
	~Parser();
	shared_ptr<belr::Grammar> loadGrammar();
	shared_ptr<belr::Parser<void *>> _parser;
};
} // namespace SDP
} // namespace bellesip
