
#ifndef grammarbuilder_hh
#define grammarbuilder_hh


#include "parser.hh"
#include <vector>

namespace belr{
class ABNFAlternation;
	
class ABNFBuilder{
public:
	virtual ~ABNFBuilder();
	virtual shared_ptr<Recognizer> buildRecognizer(const shared_ptr<Grammar> &grammar)=0;
};

class ABNFRule : public ABNFBuilder{
public:
	ABNFRule();
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

class ABNFNumval : public ABNFBuilder{
public:
	ABNFNumval();
	static shared_ptr<ABNFNumval> create();
	shared_ptr<Recognizer> buildRecognizer(const shared_ptr<Grammar> &grammar);
	void setDecVal(const string &decval);
	void setHexVal(const string &hexval);
	void setBinVal(const string &binval);
private:
	void parseValues(const string &val, int base);
	vector<int> mValues;
	bool mIsRange;
};

class ABNFElement : public ABNFBuilder{
public:
	ABNFElement();
	static shared_ptr<ABNFElement> create();
	shared_ptr<Recognizer> buildRecognizer(const shared_ptr<Grammar> &grammar);
	void setElement(const shared_ptr<ABNFBuilder> &e);
	void setRulename(const string &rulename);
	void setCharVal(const string &charval);
	void setProseVal(const string &prose);
private:
	shared_ptr<ABNFBuilder> mElement;
	string mRulename;
	string mCharVal;
};

class ABNFGroup : public ABNFBuilder{
public:
	ABNFGroup();
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

class ABNFOption : public ABNFBuilder{
public:
	ABNFOption();
	static shared_ptr<ABNFOption> create();
	void setAlternation(const shared_ptr<ABNFAlternation> &a);
	shared_ptr<Recognizer> buildRecognizer(const shared_ptr<Grammar> &grammar);
private:
	shared_ptr<ABNFAlternation> mAlternation;
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

/**
 * The ABNFGrammarBuilder builds a Grammar object from an ABNF grammar defined in a text file.
**/
class ABNFGrammarBuilder{
public:
	/**
	 * Initialize the builder.
	**/
	BELR_PUBLIC ABNFGrammarBuilder();
	/**
	 * Create a grammar from an ABNF grammar defined in the string pointed by abnf.
	 * An optional Grammar argument corresponding to a grammar to include can be passed.
	 * Usually the belr::CoreRules grammar is required for most IETF text protocols.
	 * The returned grammar can be used to instanciate a belr::Parser object capable of parsing
	 * the protocol or language described in the grammar.
	 * @param abnf the string that contains the abnf grammar.
	 * @param grammar an optional grammar to include.
	 * @return the Grammar object corresponding to the text definition loaded, NULL if an error occured.
	**/
	BELR_PUBLIC shared_ptr<Grammar> createFromAbnf(const string &abnf, const shared_ptr<Grammar> &grammar=NULL);
	/**
	 * Create a grammar from an ABNF grammar defined in the text file pointed by path.
	 * An optional Grammar argument corresponding to a grammar to include can be passed.
	 * Usually the belr::CoreRules grammar is required for most IETF text protocols.
	 * The returned grammar can be used to instanciate a belr::Parser object capable of parsing
	 * the protocol or language described in the grammar.
	 * @param path the path from where to load the abnf definition.
	 * @param grammar an optional grammar to include.
	 * @return the Grammar object corresponding to the text definition loaded, NULL if an error occured.
	**/
	BELR_PUBLIC shared_ptr<Grammar> createFromAbnfFile(const string &path, const shared_ptr<Grammar> &grammar=NULL);
private:
	Parser<shared_ptr<ABNFBuilder>> mParser;
};

}

#endif
