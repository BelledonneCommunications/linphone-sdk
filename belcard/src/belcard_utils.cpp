/*
    belcard_utils.cpp
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

#include "belcard/belcard_utils.hpp"
#include "bctoolbox/logging.h"
#include "bctoolbox/utils.hh"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string.h>

using namespace ::std;

string belcard_fold(const string &input) {
	return bctoolbox::Utils::fold(input);
}

string belcard_unfold(const string &input) {
	return bctoolbox::Utils::unfold(input);
}

string belcard_read_file(const string &filename) {
	const char *fName = filename.c_str();
	ifstream istr(fName, ifstream::in | ifstream::binary);

	if (!istr || !istr.is_open() || istr.fail()) {
		bctbx_error("[BelCard] Couldn't open file %s", fName);
		return string();
	}

	string vcard;
	istr.seekg(0, ios::end);
	vcard.resize((size_t)istr.tellg());
	istr.seekg(0, ios::beg);
	istr.read(&vcard[0], vcard.size());
	istr.close();
	return vcard;
}
