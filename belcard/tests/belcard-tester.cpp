#include "belcard/belcard_parser.hpp"

#include <iostream>
#include <fstream>
#include <sstream>

using namespace::belr;
using namespace::belcard;

int main(int argc, char *argv[]) {
	ifstream istr("vcardtest.vcf");
	if (!istr.is_open()) {
		return -1;
	}
	stringstream vcardStream;
	vcardStream << istr.rdbuf();
	string vcard = vcardStream.str();
	
	BelCardParser *parser = new BelCardParser();
	shared_ptr<BelCard> belCard = parser->parse(vcard);
	
	if (belCard) {
		string outputVCard = belCard->toString();
		cout << outputVCard << endl;
	} else {
		cerr << "Error: returned pointer is null" << endl;
	}
	
	return 0;
}