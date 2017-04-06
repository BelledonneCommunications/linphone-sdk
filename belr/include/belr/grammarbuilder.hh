
#ifndef grammarbuilder_hh
#define grammarbuilder_hh


#include "parser.hh"
#include <vector>

namespace belr{
class ABNFAlternation;
	
class ABNFBuilder{
public:
	virtual ~ABNFBuilder();
	virtual std::shared_ptr<Recognizer> buildRecognizer(const std::shared_ptr<Grammar> &grammar)=0;
};

class ABNFRule : public ABNFBuilder{
public:
	ABNFRule();
	static std::shared_ptr<ABNFRule> create();
	void setName(const std::string &name);
	void setDefinedAs(const std::string &defined_as);
	void setAlternation(const std::shared_ptr<ABNFAlternation> &a);
	std::shared_ptr<Recognizer> buildRecognizer(const std::shared_ptr<Grammar> &grammar);
	bool isExtension()const;
	const std::string &getName()const{
		return mName;
	}
private:
	std::shared_ptr<ABNFAlternation> mAlternation;
	std::string mName;
	std::string mDefinedAs;
};

class ABNFRuleList : public ABNFBuilder{
public:
	static std::shared_ptr<ABNFRuleList> create();
	void addRule(const std::shared_ptr<ABNFRule> & rule);
	std::shared_ptr<Recognizer> buildRecognizer(const std::shared_ptr<Grammar> &grammar);
private:
	std::list<std::shared_ptr<ABNFRule>> mRules;
};

class ABNFNumval : public ABNFBuilder{
public:
	ABNFNumval();
	static std::shared_ptr<ABNFNumval> create();
	std::shared_ptr<Recognizer> buildRecognizer(const std::shared_ptr<Grammar> &grammar);
	void setDecVal(const std::string &decval);
	void setHexVal(const std::string &hexval);
	void setBinVal(const std::string &binval);
private:
	void parseValues(const std::string &val, int base);
	std::vector<int> mValues;
	bool mIsRange;
};

class ABNFElement : public ABNFBuilder{
public:
	ABNFElement();
	static std::shared_ptr<ABNFElement> create();
	std::shared_ptr<Recognizer> buildRecognizer(const std::shared_ptr<Grammar> &grammar);
	void setElement(const std::shared_ptr<ABNFBuilder> &e);
	void setRulename(const std::string &rulename);
	void setCharVal(const std::string &charval);
	void setProseVal(const std::string &prose);
private:
	std::shared_ptr<ABNFBuilder> mElement;
	std::string mRulename;
	std::string mCharVal;
};

class ABNFGroup : public ABNFBuilder{
public:
	ABNFGroup();
	static std::shared_ptr<ABNFGroup> create();
	std::shared_ptr<Recognizer> buildRecognizer(const std::shared_ptr<Grammar> &grammar);
	void setAlternation(const std::shared_ptr<ABNFAlternation> &a);
private:
	std::shared_ptr<ABNFAlternation> mAlternation;
};

class ABNFRepetition : public ABNFBuilder{
public:
	ABNFRepetition();
	static std::shared_ptr<ABNFRepetition> create();
	void setRepeat(const std::string &r);
	void setMin(int min);
	void setMax(int max);
	void setCount(int count);
	void setElement(const std::shared_ptr<ABNFElement> &e);
	std::shared_ptr<Recognizer> buildRecognizer(const std::shared_ptr<Grammar> &grammar);
private:
	int mMin, mMax, mCount;
	std::string mRepeat;
	std::shared_ptr<ABNFElement> mElement;
};

class ABNFOption : public ABNFBuilder{
public:
	ABNFOption();
	static std::shared_ptr<ABNFOption> create();
	void setAlternation(const std::shared_ptr<ABNFAlternation> &a);
	std::shared_ptr<Recognizer> buildRecognizer(const std::shared_ptr<Grammar> &grammar);
private:
	std::shared_ptr<ABNFAlternation> mAlternation;
};

class ABNFConcatenation : public ABNFBuilder{
public:
	static std::shared_ptr<ABNFConcatenation> create();
	std::shared_ptr<Recognizer> buildRecognizer(const std::shared_ptr<Grammar> &grammar);
	void addRepetition(const std::shared_ptr<ABNFRepetition> &r);
private:
	std::list<std::shared_ptr<ABNFRepetition>> mRepetitions;
};

class ABNFAlternation : public ABNFBuilder{
public:
	static std::shared_ptr<ABNFAlternation> create();
	void addConcatenation(const std::shared_ptr<ABNFConcatenation> &c);
	std::shared_ptr<Recognizer> buildRecognizer(const std::shared_ptr<Grammar> &grammar);
	std::shared_ptr<Recognizer> buildRecognizerNoOptim(const std::shared_ptr<Grammar> &grammar);
private:
	std::list<std::shared_ptr<ABNFConcatenation>> mConcatenations;
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
	BELR_PUBLIC std::shared_ptr<Grammar> createFromAbnf(const std::string &abnf, const std::shared_ptr<Grammar> &grammar=NULL);
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
	BELR_PUBLIC std::shared_ptr<Grammar> createFromAbnfFile(const std::string &path, const std::shared_ptr<Grammar> &grammar=NULL);
private:
	Parser<std::shared_ptr<ABNFBuilder>> mParser;
};

}

#endif
