#ifndef belr_hh
#define belr_hh

#include <string>
#include <list>
#include <map>
#include <memory>


#ifdef _MSC_VER
	#ifdef BELR_STATIC
		#define BELR_PUBLIC
	#else
		#ifdef BELR_EXPORTS
			#define BELR_PUBLIC	__declspec(dllexport)
		#else
			#define BELR_PUBLIC	__declspec(dllimport)
		#endif
	#endif
#else
	#define BELR_PUBLIC
#endif

namespace belr{
	
BELR_PUBLIC std::string tolower(const std::string &str);

class ParserContextBase;

struct TransitionMap{
	TransitionMap();
	bool intersect(const TransitionMap *other);
	bool intersect(const TransitionMap *other, TransitionMap *result); //performs a AND operation
	void merge(const TransitionMap *other); //Performs an OR operation
	bool mPossibleChars[256];
};

class Recognizer : public std::enable_shared_from_this<Recognizer>{
public:
	void setName(const std::string &name);
	const std::string &getName()const;
	BELR_PUBLIC size_t feed(const std::shared_ptr<ParserContextBase> &ctx, const std::string &input, size_t pos);
	unsigned int getId()const{
		return mId;
	}
	bool getTransitionMap(TransitionMap *mask);
	void optimize();
	void optimize(int recursionLevel);
	virtual ~Recognizer() { }
protected:
	/*returns true if the transition map is complete, false otherwise*/
	virtual bool _getTransitionMap(TransitionMap *mask);
	virtual void _optimize(int recursionLevel)=0;
	Recognizer();
	virtual size_t _feed(const std::shared_ptr<ParserContextBase> &ctx, const std::string &input, size_t pos)=0;
	std::string mName;
	unsigned int mId;
};

class CharRecognizer : public Recognizer{
public:
	CharRecognizer(int to_recognize, bool caseSensitive=false);
private:
	virtual void _optimize(int recursionLevel);
	virtual size_t _feed(const std::shared_ptr<ParserContextBase> &ctx, const std::string &input, size_t pos);
	int mToRecognize;
	bool mCaseSensitive;
};

class Selector : public Recognizer{
public:
	Selector();
	std::shared_ptr<Selector> addRecognizer(const std::shared_ptr<Recognizer> &element);
protected:
	virtual void _optimize(int recursionLevel);
	virtual size_t _feed(const std::shared_ptr<ParserContextBase> &ctx, const std::string &input, size_t pos);
	size_t _feedExclusive(const std::shared_ptr<ParserContextBase> &ctx, const std::string &input, size_t pos);
	virtual bool _getTransitionMap(TransitionMap *mask);
	std::list<std::shared_ptr<Recognizer>> mElements;
	bool mIsExclusive;
};

/**This is an optimization of the first one for the case where there can be only a single match*/
class ExclusiveSelector : public Selector{
public:
	ExclusiveSelector();
private:
	virtual size_t _feed(const std::shared_ptr<ParserContextBase> &ctx, const std::string &input, size_t pos);
};

class Sequence : public Recognizer{
public:
	Sequence();
	std::shared_ptr<Sequence> addRecognizer(const std::shared_ptr<Recognizer> &element);
	virtual bool _getTransitionMap(TransitionMap *mask);
protected:
	virtual void _optimize(int recursionLevel);
private:
	virtual size_t _feed(const std::shared_ptr<ParserContextBase> &ctx, const std::string &input, size_t pos);
	std::list<std::shared_ptr<Recognizer>> mElements;
};

class Loop : public Recognizer{
public:
	Loop();
	std::shared_ptr<Loop> setRecognizer(const std::shared_ptr<Recognizer> &element, int min=0, int max=-1);
	virtual bool _getTransitionMap(TransitionMap *mask);
protected:
	virtual void _optimize(int recursionLevel);
private:
	virtual size_t _feed(const std::shared_ptr<ParserContextBase> &ctx, const std::string &input, size_t pos);
	std::shared_ptr<Recognizer> mRecognizer;
	int mMin, mMax;
};


class Foundation{
public:
	static std::shared_ptr<CharRecognizer> charRecognizer(int character, bool caseSensitive=false);
	static std::shared_ptr<Selector> selector(bool isExclusive=false);
	static std::shared_ptr<Sequence> sequence();
	static std::shared_ptr<Loop> loop();
};

/*this is an optimization of a selector with multiple individual char recognizer*/
class CharRange : public Recognizer{
public:
	CharRange(int begin, int end);
private:
	virtual void _optimize(int recursionLevel);
	virtual size_t _feed(const std::shared_ptr<ParserContextBase> &ctx, const std::string &input, size_t pos);
	int mBegin,mEnd;
};

class Literal : public Recognizer{
public:
	Literal(const std::string &lit);
	virtual bool _getTransitionMap(TransitionMap *mask);
private:
	virtual void _optimize(int recursionLevel);
	virtual size_t _feed(const std::shared_ptr<ParserContextBase> &ctx, const std::string &input, size_t pos);
	std::string mLiteral;
	size_t mLiteralSize;
};

class Utils{
public:
	static std::shared_ptr<Recognizer> literal(const std::string & lt);
	static std::shared_ptr<Recognizer> char_range(int begin, int end);
};

class RecognizerPointer :  public Recognizer{
public:
	RecognizerPointer();
	std::shared_ptr<Recognizer> getPointed();
	void setPointed(const std::shared_ptr<Recognizer> &r);
private:
	virtual void _optimize(int recursionLevel);
	virtual size_t _feed(const std::shared_ptr<ParserContextBase> &ctx, const std::string &input, size_t pos);
	std::shared_ptr<Recognizer> mRecognizer;
};

/**
 * Grammar class represents an ABNF grammar, with all its rules.
**/
class Grammar{
public:
	/**
	 * Initialize an empty grammar, giving a name for debugging.
	**/
	BELR_PUBLIC Grammar(const std::string &name);
	
	BELR_PUBLIC ~Grammar();
	
	/**
	 * Include another grammar into this grammar.
	**/
	BELR_PUBLIC void include(const std::shared_ptr<Grammar>& grammar);
	/**
	 * Add arule to the grammar.
	 * @param name the name of the rule
	 * @param rule the rule recognier, must be an instance of belr::Recognizer.
	 * @return the rule (the recognizer). The recognizer is given the name of the rule.
	 * @note The grammar takes ownership of the recognizer, which must not be used outside of this grammar.
	 * TODO: use unique_ptr to enforce this, or make a copy ?
	**/
	template <typename _recognizerT>
	std::shared_ptr<_recognizerT> addRule(const std::string & name, const std::shared_ptr<_recognizerT> &rule){
		assignRule(name, rule);
		return rule;
	}
	/**
	 * Extend a rule from the grammar.
	 * This corresponds to the '/=' operator of ABNF definition.
	 * @param name the name of the rule to extend.
	 * @param rule the recognizer of the extension.
	 * @return the rule.
	**/
	template <typename _recognizerT>
	std::shared_ptr<_recognizerT> extendRule(const std::string & name, const std::shared_ptr<_recognizerT> &rule){
		_extendRule(name, rule);
		return rule;
	}
	/**
	 * Find a rule from the grammar, given its name.
	 * @param name the name of the rule
	 * @return the recognizer implementing this rule. Is NULL if the rule doesn't exist in the grammar.
	**/
	BELR_PUBLIC std::shared_ptr<Recognizer> findRule(const std::string &name);
	/**
	 * Find a rule from the grammar, given its name.
	 * Unlike findRule(), getRule() never returns NULL. 
	 * If the rule is not (yet) defined, it returns an undefined pointer, that will be set later if the rule gets defined.
	 * This mechanism is required to allow defining rules in any order, and defining rules that call themselve recursively.
	 * @param name the name of the rule to get
	 * @return the recognizer implementing the rule, or a RecognizerPointer if the rule isn't yet defined.
	**/
	BELR_PUBLIC std::shared_ptr<Recognizer> getRule(const std::string &name);
	/**
	 * Returns true if the grammar is complete, that is all rules are defined.
	 * In other words, a grammar is complete if no rule depends on another rule which is not defined.
	**/
	BELR_PUBLIC bool isComplete()const;
	/**
	 * Optimize the grammar. This is required to obtain good performance of the recognizers implementing the rule.
	 * The optimization step consists in checking whether belr::Selector objects in the grammar are exclusive or not.
	 * A selector is said exclusive when a single sub-rule can match. Knowing this in advance optimizes the processing because no branch
	 * context is to be created to explore the different choices of the selector recognizer.
	**/ 
	void optimize();
	/**
	 * Return the number of rules in this grammar.
	**/
	int getNumRules()const;
private:
	void assignRule(const std::string &name, const std::shared_ptr<Recognizer> &rule);
	void _extendRule(const std::string &name, const std::shared_ptr<Recognizer> &rule);
	std::map<std::string,std::shared_ptr<Recognizer>> mRules;
	std::list<std::shared_ptr<RecognizerPointer>> mRecognizerPointers;
	std::string mName;
};




}

#endif
