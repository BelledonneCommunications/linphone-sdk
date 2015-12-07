#include "belcard/belcard_utils.hpp"

using namespace::std;

string belcard_fold(string input) {
	size_t crlf = 0;
	size_t next_crlf = 0;
	
	while (next_crlf != string::npos) {
		next_crlf = input.find("\r\n", crlf);
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
	size_t crlf = input.find("\r\n");
	
	while (crlf != string::npos) {
		if (isspace(input[crlf + 2])) {
			input.erase(crlf, 3);
		} else {
			crlf += 2;
		}
		
		crlf = input.find("\r\n", crlf);
	}
	
	return input;
}