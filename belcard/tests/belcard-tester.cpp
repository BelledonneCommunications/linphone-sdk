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
		->setCollector("FN", make_sfn(&BelCard::setFN));
		
	parser.setHandler("FN", make_fn(&BelCardFN::create))
		->setCollector("FN-value", make_sfn(&BelCardFN::setValue));
		
	ifstream istr("vcardtest.vcf");
	if (!istr.is_open()) {
		return -1;
	}
	stringstream vcard;
	vcard << istr.rdbuf();
		
	size_t parsedSize = 0;
	shared_ptr<BelCardGeneric> ret = parser.parseInput("vcard", vcard.str(), &parsedSize);
	shared_ptr<BelCard> belCard = dynamic_pointer_cast<BelCard>(ret);
	
	cout << "FN is " << belCard->getFN()->getValue() << endl;
	
	return 0;
}