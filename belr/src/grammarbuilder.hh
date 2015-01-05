
#ifndef grammarbuilder_hh
#define grammarbuilder_hh


#include "parser.hh"

namespace belr{
	
class ABNFBuilder{
public:
	virtual ~ABNFBuilder();
	virtual shared_ptr<Recognizer> buildRecognizer()=0;
};

class ABNFRule : public ABNFBuilder{
public:
	static ABNFRule *create();
	shared_ptr<Recognizer> buildRecognizer();
};

class ABNFRuleList : public ABNFBuilder{
public:
	static ABNFRuleList *create();
	void addRule(ABNFRule *rule);
	shared_ptr<Recognizer> buildRecognizer();
};

class ABNFGrammarBuilder{
public:
	ABNFGrammarBuilder();
	shared_ptr<Grammar> createFromAbnf(const string &path);
private:
	Parser<ABNFBuilder*> mParser;
};

}

#endif
