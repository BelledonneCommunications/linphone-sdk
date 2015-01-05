
#ifndef grammarbuilder_hh
#define grammarbuilder_hh


#include "parser.hh"

namespace belr{

class ABNFGrammarBuilder{
public:
	ABNFGrammarBuilder();
	shared_ptr<Grammar> createFromAbnf(const string &path);
private:
	void addRule(void *list, void *rule);
	void *createRuleList();
	void *createRule();
	Parser<void*> mParser;
};

}

#endif
