#ifndef parser_hh
#define parser_hh

#include <functional>
#include <vector>

#include "belr.hh"


namespace belr {

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
	ParserCollector(const std::function<void (_derivedParserElementT , _valueT)> &fn) : mFunc(fn){
	}
	virtual void invoke(_parserElementT obj, _valueT value);
	void invokeWithChild(_parserElementT obj, _parserElementT child);
private:
	std::function<void (_derivedParserElementT, _valueT)> mFunc;
};

template <typename _derivedParserElementT, typename _parserElementT, typename _valueT>
class ParserChildCollector : public CollectorBase<_parserElementT,_valueT>{
public:
	ParserChildCollector(const std::function<void (_derivedParserElementT , _valueT)> &fn) : mFunc(fn){
	}
	virtual void invoke(_parserElementT obj, _valueT value);
	virtual void invokeWithChild(_parserElementT obj, _parserElementT child);
private:
	std::function<void (_derivedParserElementT, _valueT)> mFunc;
};

template <typename _parserElementT>
class HandlerContext;

template <typename _parserElementT>
class Parser;

class HandlerContextBase;

template <typename _parserElementT>
class ParserHandlerBase : public std::enable_shared_from_this<ParserHandlerBase<_parserElementT>>{
	friend class HandlerContext<_parserElementT>;
public:
	virtual _parserElementT invoke(const std::string &input, size_t begin, size_t count)=0;
	std::shared_ptr<HandlerContext<_parserElementT>> createContext();
	const std::string &getRulename()const{
		return mRulename;
	}
protected:
	void releaseContext(const std::shared_ptr<HandlerContext<_parserElementT>> &ctx);
	ParserHandlerBase(const Parser<_parserElementT> &parser, const std::string &name);
	void installCollector(const std::string &rulename, const std::shared_ptr<AbstractCollector<_parserElementT>> &collector);
	const std::shared_ptr<AbstractCollector<_parserElementT>> &getCollector(unsigned int rule_id)const;
private:
	std::map<unsigned int, std::shared_ptr<AbstractCollector<_parserElementT>> > mCollectors;
	const Parser<_parserElementT> &mParser;
	std::string mRulename;
	std::shared_ptr<HandlerContext<_parserElementT>> mCachedContext;
};

template <typename _derivedParserElementT, typename _parserElementT>
class ParserHandler :  public ParserHandlerBase<_parserElementT>{
public:
	ParserHandler(const Parser<_parserElementT> &parser, const std::string &rulename, const std::function<_derivedParserElementT ()> &create)
		: ParserHandlerBase<_parserElementT>(parser, rulename), mHandlerCreateFunc(create){
	}
	ParserHandler(const Parser<_parserElementT> &parser, const std::string &rulename, const std::function<_derivedParserElementT (const std::string &, const std::string &)> &create)
		: ParserHandlerBase<_parserElementT>(parser, rulename), mHandlerCreateDebugFunc(create){
	}
	template <typename _derivedParserElementTChild>
	std::shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> setCollector(const std::string &child_rule_name, std::function<void (_derivedParserElementTChild , const std::string & )> fn){
		this->installCollector(child_rule_name, std::make_shared<ParserCollector<_derivedParserElementT,_parserElementT,const std::string&>>(fn));
		return std::static_pointer_cast<ParserHandler<_derivedParserElementT,_parserElementT>>(this->shared_from_this());
	}
	template <typename _derivedParserElementTChild>
	std::shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> setCollector(const std::string &child_rule_name, std::function<void (_derivedParserElementTChild , int )> fn){
		this->installCollector(child_rule_name, std::make_shared<ParserCollector<_derivedParserElementT,_parserElementT,int>>(fn));
		return std::static_pointer_cast<ParserHandler<_derivedParserElementT,_parserElementT>>(this->shared_from_this());
	}
	template <typename _derivedParserElementTChild, typename _valueT>
	std::shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> setCollector(const std::string &child_rule_name, std::function<void (_derivedParserElementTChild , _valueT)> fn){
		this->installCollector(child_rule_name, std::make_shared<ParserChildCollector<_derivedParserElementT,_parserElementT,_valueT>>(fn));
		return std::static_pointer_cast<ParserHandler<_derivedParserElementT,_parserElementT>>(this->shared_from_this());
	}
	_parserElementT invoke(const std::string &input, size_t begin, size_t count);
private:
	std::function<_derivedParserElementT ()> mHandlerCreateFunc;
	std::function<_derivedParserElementT (const std::string &, const std::string &)> mHandlerCreateDebugFunc;
};

template <typename _parserElementT>
class Assignment{
private:
	AbstractCollector<_parserElementT> * mCollector;//not a shared_ptr for optimization, the collector cannot disapear
	size_t mBegin;
	size_t mCount;
	std::shared_ptr<HandlerContext<_parserElementT>> mChild;
public:
	Assignment(const std::shared_ptr<AbstractCollector<_parserElementT>> &c, size_t begin, size_t count, const std::shared_ptr<HandlerContext<_parserElementT>> &child)
		: mCollector(c.get()), mBegin(begin), mCount(count), mChild(child)
	{
	}
	void invoke(_parserElementT parent, const std::string &input);
};

class HandlerContextBase : public std::enable_shared_from_this<HandlerContextBase>{
public:
	BELR_PUBLIC virtual ~HandlerContextBase();
};

template <typename _parserElementT>
class HandlerContext : public HandlerContextBase{
public:
	HandlerContext(const std::shared_ptr<ParserHandlerBase<_parserElementT>> &handler);
	void setChild(unsigned int subrule_id, size_t begin, size_t count, const std::shared_ptr<HandlerContext> &child);
	_parserElementT realize(const std::string &input, size_t begin, size_t count);
	std::shared_ptr<HandlerContext<_parserElementT>> branch();
	void merge(const std::shared_ptr<HandlerContext<_parserElementT>> &other);
	size_t getLastIterator()const;
	void undoAssignments(size_t pos);
	void recycle();
private:
	ParserHandlerBase<_parserElementT> & mHandler;
	std::vector<Assignment<_parserElementT>> mAssignments;
};

struct ParserLocalContext{
	ParserLocalContext() : mHandlerContext(NULL), mRecognizer(NULL), mAssignmentPos(0) {
	}
	void set(const std::shared_ptr<HandlerContextBase>& hc, const std::shared_ptr<Recognizer>& rec, size_t pos){
		mHandlerContext=hc;
		mRecognizer=rec.get();
		mAssignmentPos=pos;
	}
	std::shared_ptr<HandlerContextBase> mHandlerContext;
	Recognizer * mRecognizer; //not a shared ptr to optimize, the object can't disapear in the context of use of ParserLocalContext.
	size_t mAssignmentPos;
};

class ParserContextBase{
public:
	virtual void beginParse(ParserLocalContext &ctx, const std::shared_ptr<Recognizer> &rec)=0;
	virtual void endParse(const ParserLocalContext &ctx, const std::string &input, size_t begin, size_t count)=0;
	virtual std::shared_ptr<HandlerContextBase> branch()=0;
	virtual void merge(const std::shared_ptr<HandlerContextBase> &other)=0;
	virtual void removeBranch(const std::shared_ptr<HandlerContextBase> &other)=0;
};

template <typename _parserElementT>
class ParserContext : public ParserContextBase{
public:
	ParserContext(Parser<_parserElementT> &parser);
	_parserElementT createRootObject(const std::string &input);
protected:
	virtual void beginParse(ParserLocalContext &ctx, const std::shared_ptr<Recognizer> &rec);
	virtual void endParse(const ParserLocalContext &ctx, const std::string &input, size_t begin, size_t count);
	virtual std::shared_ptr<HandlerContextBase> branch();
	virtual void merge(const std::shared_ptr<HandlerContextBase> &other);
	virtual void removeBranch(const std::shared_ptr<HandlerContextBase> &other);
	void  _beginParse(ParserLocalContext &ctx, const std::shared_ptr<Recognizer> &rec);
	void _endParse(const ParserLocalContext &ctx, const std::string &input, size_t begin, size_t count);
	std::shared_ptr<HandlerContext<_parserElementT>> _branch();
	void _merge(const std::shared_ptr<HandlerContext<_parserElementT>> &other);
	void _removeBranch(const std::shared_ptr<HandlerContext<_parserElementT>> &other);
private:
	Parser<_parserElementT> & mParser;
	std::list<std::shared_ptr<HandlerContext<_parserElementT>>> mHandlerStack;
	std::shared_ptr<HandlerContext<_parserElementT>> mRoot;
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
	Parser(const std::shared_ptr<Grammar> &grammar);
	template <typename _derivedParserElementT> 
	std::shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> setHandler(const std::string &rulename,const std::function<_derivedParserElementT ()> & handler){
		auto ret=std::make_shared<ParserHandler<_derivedParserElementT,_parserElementT>>(*this, rulename,handler);
		installHandler(ret);
		return ret;
		
	}
	template <typename _derivedParserElementT> 
	std::shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> setHandler(const std::string &rulename,
					const std::function<_derivedParserElementT (const std::string &, const std::string &)> & handler){
		auto ret=std::make_shared<ParserHandler<_derivedParserElementT,_parserElementT>>(*this, rulename,handler);
		installHandler(ret);
		return ret;
		
	}
	_parserElementT parseInput(const std::string &rulename, const std::string &input, size_t *parsed_size);
private:
	std::shared_ptr<ParserHandlerBase<_parserElementT>> &getHandler(unsigned int);
	void installHandler(const std::shared_ptr<ParserHandlerBase<_parserElementT>> &handler);
	std::shared_ptr<Grammar> mGrammar;
	std::map<unsigned int, std::shared_ptr<ParserHandlerBase<_parserElementT>>> mHandlers;
	std::shared_ptr<ParserHandlerBase<_parserElementT>> mNullHandler;
	std::shared_ptr<AbstractCollector<_parserElementT>> mNullCollector;
};

class DebugElement{
public:
	DebugElement(const std::string &rulename, const std::string &value);
	static std::shared_ptr<DebugElement> create(const std::string &rulename, const std::string &value);
	void addChild(const std::shared_ptr<DebugElement> &e);
	BELR_PUBLIC std::ostream &tostream(int level, std::ostream &str)const;
private:
	std::string mRulename;
	std::string mValue;
	std::list<std::shared_ptr<DebugElement>> mChildren;
};

class DebugParser : protected Parser<std::shared_ptr<DebugElement>>{
public:
	BELR_PUBLIC DebugParser(const std::shared_ptr<Grammar> &grammar);
	BELR_PUBLIC void setObservedRules(const std::list<std::string> &rules);
	BELR_PUBLIC std::shared_ptr<DebugElement> parseInput(const std::string &rulename, const std::string &input, size_t *parsed_size);
};


template <typename _retT>
std::function< _retT ()> make_fn(_retT (*arg)()){
	return std::function<_retT ()>(arg);
}


template <typename _retT, typename _arg1T, typename _arg2T>
std::function< _retT (_arg1T,_arg2T)> make_fn(_retT (*arg)(_arg1T,_arg2T)){
	return std::function< _retT (_arg1T,_arg2T)>(arg);
}

template <typename _klassT, typename _argT>
std::function< void (_klassT*,_argT)> make_fn(void (_klassT::*arg)(_argT)){
	return std::function< void (_klassT*,_argT)>(std::mem_fn(arg));
}

template <typename _klassT, typename _argT>
std::function< void (std::shared_ptr<_klassT>,_argT)> make_sfn(void (_klassT::*arg)(_argT)){
	return std::function< void (std::shared_ptr<_klassT>,_argT)>(std::mem_fn(arg));
}



}

#endif

