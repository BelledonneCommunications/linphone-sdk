#include "belcard/belcard_utils.hpp"

#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char *argv[]) {
	const char *file = NULL;
	
	if (argc < 2) {
		cerr << argv[0] << " <file to fold> - fold the content of a file" << endl;
		return -1;
	}
	file = argv[1];
	
	ifstream istr(file);
	if (!istr.is_open()) {
		return -1;
	}
	
	stringstream vcardStream;
	vcardStream << istr.rdbuf();
	string vcard = vcardStream.str();
	
	vcard = belcard_fold(vcard);
	cout << vcard << endl;
	
	return 0;
}