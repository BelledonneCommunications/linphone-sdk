
#ifndef grammarbuilder_hh
#define grammarbuilder_hh


#include "parser.hh"

namespace belr{
class ABNFAlternation;
	
class ABNFBuilder{
public:
	virtual ~ABNFBuilder();
	virtual shared_ptr<Recognizer> buildRecognizer(const shared_ptr<Grammar> &grammar)=0;
};

class ABNFRule : public ABNFBuilder{
public:
	static shared_ptr<ABNFRule> create();
	void setName(const string &name);
	void setDefinedAs(const string &defined_as);
	void setAlternation(const shared_ptr<ABNFAlternation> &a);
	shared_ptr<Recognizer> buildRecognizer(const shared_ptr<Grammar> &grammar);
	bool isExtension()const;
	const string &getName()const{
		return mName;
	}
private:
	shared_ptr<ABNFAlternation> mAlternation;
	string mName;
	string mDefinedAs;
};

class ABNFRuleList : public ABNFBuilder{
public:
	static shared_ptr<ABNFRuleList> create();
	void addRule(const shared_ptr<ABNFRule> & rule);
	shared_ptr<Recognizer> buildRecognizer(const shared_ptr<Grammar> &grammar);
private:
	list<shared_ptr<ABNFRule>> mRules;
};

class ABNFElement : public ABNFBuilder{
public:
	static shared_ptr<ABNFElement> create();
	shared_ptr<Recognizer> buildRecognizer(const shared_ptr<Grammar> &grammar);
	void setElement(const shared_ptr<ABNFBuilder> &e);
	void setRulename(const string &rulename);
private:
	shared_ptr<ABNFBuilder> mElement;
	string mRulename;
};

class ABNFGroup : public ABNFBuilder{
public:
	static shared_ptr<ABNFGroup> create();
	shared_ptr<Recognizer> buildRecognizer(const shared_ptr<Grammar> &grammar);
	void setAlternation(const shared_ptr<ABNFAlternation> &a);
private:
	shared_ptr<ABNFAlternation> mAlternation;
};

class ABNFRepetition : public ABNFBuilder{
public:
	ABNFRepetition();
	static shared_ptr<ABNFRepetition> create();
	void setRepeat(const string &r);
	void setMin(int min);
	void setMax(int max);
	void setCount(int count);
	void setElement(const shared_ptr<ABNFElement> &e);
	shared_ptr<Recognizer> buildRecognizer(const shared_ptr<Grammar> &grammar);
private:
	int mMin, mMax, mCount;
	string mRepeat;
	shared_ptr<ABNFElement> mElement;
};

class ABNFConcatenation : public ABNFBuilder{
public:
	static shared_ptr<ABNFConcatenation> create();
	shared_ptr<Recognizer> buildRecognizer(const shared_ptr<Grammar> &grammar);
	void addRepetition(const shared_ptr<ABNFRepetition> &r);
private:
	list<shared_ptr<ABNFRepetition>> mRepetitions;
};

class ABNFAlternation : public ABNFBuilder{
public:
	static shared_ptr<ABNFAlternation> create();
	void addConcatenation(const shared_ptr<ABNFConcatenation> &c);
	shared_ptr<Recognizer> buildRecognizer(const shared_ptr<Grammar> &grammar);
	shared_ptr<Recognizer> buildRecognizerNoOptim(const shared_ptr<Grammar> &grammar);
private:
	list<shared_ptr<ABNFConcatenation>> mConcatenations;
};

class ABNFGrammarBuilder{
public:
	ABNFGrammarBuilder();
	shared_ptr<Grammar> createFromAbnf(const string &path);
private:
	Parser<shared_ptr<ABNFBuilder>> mParser;
};

}

#endif
