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
#include <string.h>

using namespace::std;

string belcard_fold(string input) {
	size_t crlf = 0;
	size_t next_crlf = 0;
	const char *endline = "\r\n";
	
	while (next_crlf != string::npos) {
		next_crlf = input.find(endline, crlf);
		if (next_crlf != string::npos) {
			if (next_crlf - crlf > 75) {
				input.insert(crlf + 74, "\r\n ");
				crlf += 76;
			} else {
				crlf = next_crlf + 2;
			}
		}
	}
	
	return input;
}

string belcard_unfold(string input) {
	const char *endline = "\r\n";
	size_t crlf = input.find(endline);
	
	if (crlf == string::npos) {
		endline = "\n";
		crlf = input.find(endline);
	}
	
	while (crlf != string::npos) {
		if (isspace(input[crlf + strlen(endline)])) {
			input.erase(crlf, strlen(endline) + 1);
		} else {
			crlf += strlen(endline);
		}
		
		crlf = input.find(endline, crlf);
	}
	
	return input;
}