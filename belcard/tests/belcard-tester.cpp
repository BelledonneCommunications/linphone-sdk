#include "belcard/belcard.hpp"

#include <belr/grammarbuilder.hh>
#include <belr/abnf.hh>
#include <belr/parser-impl.cc>

#include <iostream>
#include <fstream>
#include <sstream>

using namespace::belr;
using namespace::belcard;

int main(int argc, char *argv[]) {
	ABNFGrammarBuilder builder;
	shared_ptr<Grammar> grammar = builder.createFromAbnf("vcardgrammar.txt", make_shared<CoreRules>());
	if (!grammar) {
		cerr << "Could not build grammar from vcardgrammar.txt" << endl;
		return -1;
	}
	
	Parser<shared_ptr<BelCardGeneric>> parser(grammar);
	parser.setHandler("vcard", make_fn(&BelCard::create))
		->setCollector("FN", make_sfn(&BelCard::setFN))
		->setCollector("N", make_sfn(&BelCard::setN));
		
	parser.setHandler("FN", make_fn(&BelCardFN::create))
		->setCollector("group", make_sfn(&BelCardFN::setGroup))
		->setCollector("FN-value", make_sfn(&BelCardFN::setValue));
		
	parser.setHandler("N", make_fn(&BelCardN::create))
		->setCollector("group", make_sfn(&BelCardN::setGroup))
		->setCollector("N-value", make_sfn(&BelCardN::setValue));
		
	ifstream istr("vcardtest.vcf");
	if (!istr.is_open()) {
		return -1;
	}
	stringstream vcardStream;
	vcardStream << istr.rdbuf();
	string vcard = vcardStream.str();
		
	size_t parsedSize = 0;
	shared_ptr<BelCardGeneric> ret = parser.parseInput("vcard", vcard, &parsedSize);
	shared_ptr<BelCard> belCard = dynamic_pointer_cast<BelCard>(ret);
	
	if (belCard) {
		string outputVCard = belCard->toString();
		cout << outputVCard << endl;
	} else {
		cerr << "Error: returned pointer is null" << endl;
	}
	
	return 0;
}