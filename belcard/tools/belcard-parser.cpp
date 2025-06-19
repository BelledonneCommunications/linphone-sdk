/*
    belcard-parser.cpp
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

#include "belcard/belcard.hpp"
#include "belcard/belcard_parser.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace ::std;
using namespace ::belr;
using namespace ::belcard;

int main(int argc, char *argv[]) {
	const char *file = NULL;
	bool useV3 = false;

	if (argc < 2) {
		cerr << argv[0] << " [-v3] <file to parse> - parse the content of a file" << endl;
		return -1;
	}
	if (argc < 3) {
		file = argv[1];
	} else {
		useV3 = true;
		file = argv[2];
	}

	auto t_start = std::chrono::high_resolution_clock::now();
	BelCardParser *parser = new BelCardParser(useV3);
	auto t_end = std::chrono::high_resolution_clock::now();

	auto t_start_2 = std::chrono::high_resolution_clock::now();
	shared_ptr<BelCardList> belCards = parser->parseFile(file);
	auto t_end_2 = std::chrono::high_resolution_clock::now();

	if (belCards) {
		cout << *belCards << endl;
	} else {
		cerr << "Failure: couldn't parse input file " << file << endl;
	}
	cout << "Parser initialized in " << std::chrono::duration<double, std::milli>(t_end - t_start).count()
	     << " milliseconds" << endl;
	cout << "Parsing done in " << std::chrono::duration<double, std::milli>(t_end_2 - t_start_2).count()
	     << " milliseconds" << endl;

	delete (parser);

	return 0;
}