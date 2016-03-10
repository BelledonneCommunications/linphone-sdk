#ifndef parser_hh
#define parser_hh

#include <functional>
#include <vector>

#include "belr.hh"


namespace belr{

template<typename _parserElementT>
class AbstractCollector{
public:
	virtual void invokeWithChild(_parserElementT obj, _parserElementT child)=0;
	virtual ~AbstractCollector();
};

template<typename _parserElementT, typename _valueT>
class CollectorBase : public AbstractCollector<_parserElementT>{
public:
	virtual void invoke(_parserElementT obj, _valueT value)=0;
};

template <typename _derivedParserElementT, typename _parserElementT, typename _valueT>
class ParserCollector : public CollectorBase<_parserElementT,_valueT>{
public:
	ParserCollector(const function<void (_derivedParserElementT , _valueT)> &fn) : mFunc(fn){
	}
	virtual void invoke(_parserElementT obj, _valueT value);
	void invokeWithChild(_parserElementT obj, _parserElementT child);
private:
	function<void (_derivedParserElementT, _valueT)> mFunc;
};

template <typename _derivedParserElementT, typename _parserElementT, typename _valueT>
class ParserChildCollector : public CollectorBase<_parserElementT,_valueT>{
public:
	ParserChildCollector(const function<void (_derivedParserElementT , _valueT)> &fn) : mFunc(fn){
	}
	virtual void invoke(_parserElementT obj, _valueT value);
	virtual void invokeWithChild(_parserElementT obj, _parserElementT child);
private:
	function<void (_derivedParserElementT, _valueT)> mFunc;
};

template <typename _parserElementT>
class HandlerContext;

template <typename _parserElementT>
class Parser;

class HandlerContextBase;

template <typename _parserElementT>
class ParserHandlerBase : public enable_shared_from_this<ParserHandlerBase<_parserElementT>>{
	friend class HandlerContext<_parserElementT>;
public:
	virtual _parserElementT invoke(const string &input, size_t begin, size_t count)=0;
	shared_ptr<HandlerContext<_parserElementT>> createContext();
	const string &getRulename()const{
		return mRulename;
	}
protected:
	void releaseContext(const shared_ptr<HandlerContext<_parserElementT>> &ctx);
	ParserHandlerBase(const Parser<_parserElementT> &parser, const string &name);
	void installCollector(const string &rulename, const shared_ptr<AbstractCollector<_parserElementT>> &collector);
	const shared_ptr<AbstractCollector<_parserElementT>> &getCollector(unsigned int rule_id)const;
private:
	map<unsigned int, shared_ptr<AbstractCollector<_parserElementT>> > mCollectors;
	const Parser<_parserElementT> &mParser;
	string mRulename;
	shared_ptr<HandlerContext<_parserElementT>> mCachedContext;
};

template <typename _derivedParserElementT, typename _parserElementT>
class ParserHandler :  public ParserHandlerBase<_parserElementT>{
public:
	ParserHandler(const Parser<_parserElementT> &parser, const string &rulename, const function<_derivedParserElementT ()> &create)
		: ParserHandlerBase<_parserElementT>(parser, rulename), mHandlerCreateFunc(create){
	}
	ParserHandler(const Parser<_parserElementT> &parser, const string &rulename, const function<_derivedParserElementT (const string &, const string &)> &create)
		: ParserHandlerBase<_parserElementT>(parser, rulename), mHandlerCreateDebugFunc(create){
	}
	template <typename _derivedParserElementTChild>
	shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> setCollector(const string &child_rule_name, function<void (_derivedParserElementTChild , const string & )> fn){
		this->installCollector(child_rule_name, make_shared<ParserCollector<_derivedParserElementT,_parserElementT,const string&>>(fn));
		return static_pointer_cast<ParserHandler<_derivedParserElementT,_parserElementT>>(this->shared_from_this());
	}
	template <typename _derivedParserElementTChild>
	shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> setCollector(const string &child_rule_name, function<void (_derivedParserElementTChild , int )> fn){
		this->installCollector(child_rule_name, make_shared<ParserCollector<_derivedParserElementT,_parserElementT,int>>(fn));
		return static_pointer_cast<ParserHandler<_derivedParserElementT,_parserElementT>>(this->shared_from_this());
	}
	template <typename _derivedParserElementTChild, typename _valueT>
	shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> setCollector(const string &child_rule_name, function<void (_derivedParserElementTChild , _valueT)> fn){
		this->installCollector(child_rule_name, make_shared<ParserChildCollector<_derivedParserElementT,_parserElementT,_valueT>>(fn));
		return static_pointer_cast<ParserHandler<_derivedParserElementT,_parserElementT>>(this->shared_from_this());
	}
	_parserElementT invoke(const string &input, size_t begin, size_t count);
private:
	function<_derivedParserElementT ()> mHandlerCreateFunc;
	function<_derivedParserElementT (const string &, const string &)> mHandlerCreateDebugFunc;
};

template <typename _parserElementT>
class Assignment{
private:
	AbstractCollector<_parserElementT> * mCollector;//not a shared_ptr for optimization, the collector cannot disapear
	size_t mBegin;
	size_t mCount;
	shared_ptr<HandlerContext<_parserElementT>> mChild;
public:
	Assignment(const shared_ptr<AbstractCollector<_parserElementT>> &c, size_t begin, size_t count, const shared_ptr<HandlerContext<_parserElementT>> &child)
		: mCollector(c.get()), mBegin(begin), mCount(count), mChild(child)
	{
	}
	void invoke(_parserElementT parent, const string &input);
};

class HandlerContextBase : public enable_shared_from_this<HandlerContextBase>{
public:
	BELR_PUBLIC virtual ~HandlerContextBase();
};

template <typename _parserElementT>
class HandlerContext : public HandlerContextBase{
public:
	HandlerContext(const shared_ptr<ParserHandlerBase<_parserElementT>> &handler);
	void setChild(unsigned int subrule_id, size_t begin, size_t count, const shared_ptr<HandlerContext> &child);
	_parserElementT realize(const string &input, size_t begin, size_t count);
	shared_ptr<HandlerContext<_parserElementT>> branch();
	void merge(const shared_ptr<HandlerContext<_parserElementT>> &other);
	size_t getLastIterator()const;
	void undoAssignments(size_t pos);
	void recycle();
private:
	ParserHandlerBase<_parserElementT> & mHandler;
	vector<Assignment<_parserElementT>> mAssignments;
};

struct ParserLocalContext{
	ParserLocalContext() : mHandlerContext(NULL), mRecognizer(NULL), mAssignmentPos(0) {
	}
	void set(const shared_ptr<HandlerContextBase>& hc, const shared_ptr<Recognizer>& rec, size_t pos){
		mHandlerContext=hc;
		mRecognizer=rec.get();
		mAssignmentPos=pos;
	}
	shared_ptr<HandlerContextBase> mHandlerContext;
	Recognizer * mRecognizer; //not a shared ptr to optimize, the object can't disapear in the context of use of ParserLocalContext.
	size_t mAssignmentPos;
};

class ParserContextBase{
public:
	virtual void beginParse(ParserLocalContext &ctx, const shared_ptr<Recognizer> &rec)=0;
	virtual void endParse(const ParserLocalContext &ctx, const string &input, size_t begin, size_t count)=0;
	virtual shared_ptr<HandlerContextBase> branch()=0;
	virtual void merge(const shared_ptr<HandlerContextBase> &other)=0;
	virtual void removeBranch(const shared_ptr<HandlerContextBase> &other)=0;
};

template <typename _parserElementT>
class ParserContext : public ParserContextBase{
public:
	ParserContext(Parser<_parserElementT> &parser);
	_parserElementT createRootObject(const string &input);
protected:
	virtual void beginParse(ParserLocalContext &ctx, const shared_ptr<Recognizer> &rec);
	virtual void endParse(const ParserLocalContext &ctx, const string &input, size_t begin, size_t count);
	virtual shared_ptr<HandlerContextBase> branch();
	virtual void merge(const shared_ptr<HandlerContextBase> &other);
	virtual void removeBranch(const shared_ptr<HandlerContextBase> &other);
	void  _beginParse(ParserLocalContext &ctx, const shared_ptr<Recognizer> &rec);
	void _endParse(const ParserLocalContext &ctx, const string &input, size_t begin, size_t count);
	shared_ptr<HandlerContext<_parserElementT>> _branch();
	void _merge(const shared_ptr<HandlerContext<_parserElementT>> &other);
	void _removeBranch(const shared_ptr<HandlerContext<_parserElementT>> &other);
private:
	Parser<_parserElementT> & mParser;
	list<shared_ptr<HandlerContext<_parserElementT>>> mHandlerStack;
	shared_ptr<HandlerContext<_parserElementT>> mRoot;
};

/**
 * Parser class.
 * This template class allows to parse a text input using a Grammar object describing the language of the input to be parsed.
 * The template argument _parserElementT must be a base class for all elements that will be created to represent the result of the parsing.
 * This can be 'void*' if the parser is implemented in C, but can also be any C++ class provided that each type representing a parsed entity
 * inherits from this class.
**/
template <typename _parserElementT>
class Parser{
friend class ParserContext<_parserElementT>;
friend class ParserHandlerBase<_parserElementT>;
public:
	Parser(const shared_ptr<Grammar> &grammar);
	template <typename _derivedParserElementT> 
	shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> setHandler(const string &rulename,const function<_derivedParserElementT ()> & handler){
		auto ret=make_shared<ParserHandler<_derivedParserElementT,_parserElementT>>(*this, rulename,handler);
		installHandler(ret);
		return ret;
		
	}
	template <typename _derivedParserElementT> 
	shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> setHandler(const string &rulename,
					const function<_derivedParserElementT (const string &, const string &)> & handler){
		auto ret=make_shared<ParserHandler<_derivedParserElementT,_parserElementT>>(*this, rulename,handler);
		installHandler(ret);
		return ret;
		
	}
	_parserElementT parseInput(const string &rulename, const string &input, size_t *parsed_size);
private:
	shared_ptr<ParserHandlerBase<_parserElementT>> &getHandler(unsigned int);
	void installHandler(const shared_ptr<ParserHandlerBase<_parserElementT>> &handler);
	shared_ptr<Grammar> mGrammar;
	map<unsigned int, shared_ptr<ParserHandlerBase<_parserElementT>>> mHandlers;
	shared_ptr<ParserHandlerBase<_parserElementT>> mNullHandler;
	shared_ptr<AbstractCollector<_parserElementT>> mNullCollector;
};

class DebugElement{
public:
	DebugElement(const string &rulename, const string &value);
	static shared_ptr<DebugElement> create(const string &rulename, const string &value);
	void addChild(const shared_ptr<DebugElement> &e);
	BELR_PUBLIC ostream &tostream(int level, ostream &str)const;
private:
	string mRulename;
	string mValue;
	list<shared_ptr<DebugElement>> mChildren;
};

class DebugParser : protected Parser<shared_ptr<DebugElement>>{
public:
	BELR_PUBLIC DebugParser(const shared_ptr<Grammar> &grammar);
	BELR_PUBLIC void setObservedRules(const list<string> &rules);
	BELR_PUBLIC shared_ptr<DebugElement> parseInput(const string &rulename, const string &input, size_t *parsed_size);
};


template <typename _retT>
function< _retT ()> make_fn(_retT (*arg)()){
	return function<_retT ()>(arg);
}


template <typename _retT, typename _arg1T, typename _arg2T>
function< _retT (_arg1T,_arg2T)> make_fn(_retT (*arg)(_arg1T,_arg2T)){
	return function< _retT (_arg1T,_arg2T)>(arg);
}

template <typename _klassT, typename _argT>
function< void (_klassT*,_argT)> make_fn(void (_klassT::*arg)(_argT)){
	return function< void (_klassT*,_argT)>(mem_fn(arg));
}

template <typename _klassT, typename _argT>
function< void (shared_ptr<_klassT>,_argT)> make_sfn(void (_klassT::*arg)(_argT)){
	return function< void (shared_ptr<_klassT>,_argT)>(mem_fn(arg));
}



}

#endif

