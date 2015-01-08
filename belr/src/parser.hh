#ifndef parser_hh
#define parser_hh

#include <functional>

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
class ParserHandlerBase : public enable_shared_from_this<ParserHandlerBase<_parserElementT>>{
	friend class HandlerContext<_parserElementT>;
public:
	virtual _parserElementT invoke()=0;
	shared_ptr<HandlerContext<_parserElementT>> createContext();
protected:
	map<string, shared_ptr<AbstractCollector<_parserElementT>> > mCollectors;
};

template <typename _derivedParserElementT, typename _parserElementT>
class ParserHandler :  public ParserHandlerBase<_parserElementT>{
public:
	ParserHandler(const function<_derivedParserElementT ()> &create)
		: mHandlerCreateFunc(create){
	}
	shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> setCollector(const string &child_rule_name, function<void (_derivedParserElementT , const string & )> fn){
		this->mCollectors[child_rule_name]=make_shared<ParserCollector<_derivedParserElementT,_parserElementT,const string&>>(fn);
		return static_pointer_cast<ParserHandler<_derivedParserElementT,_parserElementT>>(this->shared_from_this());
	}
	shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> setCollector(const string &child_rule_name, function<void (_derivedParserElementT , int )> fn){
		this->mCollectors[child_rule_name]=make_shared<ParserCollector<_derivedParserElementT,_parserElementT,int>>(fn);
		return static_pointer_cast<ParserHandler<_derivedParserElementT,_parserElementT>>(this->shared_from_this());
	}
	template <typename _valueT>
	shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> setCollector(const string &child_rule_name, function<void (_derivedParserElementT , _valueT)> fn){
		this->mCollectors[child_rule_name]=make_shared<ParserChildCollector<_derivedParserElementT,_parserElementT,_valueT>>(fn);
		return static_pointer_cast<ParserHandler<_derivedParserElementT,_parserElementT>>(this->shared_from_this());
	}
	_parserElementT invoke();
private:
	function<_derivedParserElementT ()> mHandlerCreateFunc;
};

template <typename _parserElementT>
class Assignment{
private:
	shared_ptr<AbstractCollector<_parserElementT>> mCollector;
	size_t mBegin;
	size_t mCount;
	shared_ptr<HandlerContext<_parserElementT>> mChild;
public:
	Assignment(const shared_ptr<AbstractCollector<_parserElementT>> &c, size_t begin, size_t count, const shared_ptr<HandlerContext<_parserElementT>> &child)
		: mCollector(c), mBegin(begin), mCount(count), mChild(child)
	{
	}
	void invoke(_parserElementT parent, const string &input);
};

class HandlerContextBase{
public:
	virtual ~HandlerContextBase();
};

template <typename _parserElementT>
class HandlerContext : public HandlerContextBase{
public:
	HandlerContext(const shared_ptr<ParserHandlerBase<_parserElementT>> &handler) : 
		mHandler(handler){
	}
	void setChild(const string &subrule_name, size_t begin, size_t count, const shared_ptr<HandlerContext> &child){
		auto it=mHandler->mCollectors.find(subrule_name);
		if (it!=mHandler->mCollectors.end()){
			mAssignments.push_back(Assignment<_parserElementT>((*it).second, begin, count, child));
		}
	}
	_parserElementT realize(const string &input){
		_parserElementT ret=mHandler->invoke();
		for (auto it=mAssignments.begin(); it!=mAssignments.end(); ++it){
			(*it).invoke(ret,input);
		}
		return ret;
	}
	shared_ptr<HandlerContext<_parserElementT>> branch(){
		return make_shared<HandlerContext>(mHandler);
	}
	void merge(const shared_ptr<HandlerContext<_parserElementT>> &other){
		mAssignments.splice(mAssignments.begin(), other->mAssignments);
	}
private:
	shared_ptr<ParserHandlerBase<_parserElementT>> mHandler;
	list<Assignment<_parserElementT>> mAssignments;
};

template <typename _parserElementT>
class Parser;

class ParserContextBase{
public:
	virtual shared_ptr<HandlerContextBase> beginParse(const shared_ptr<Recognizer> &rec)=0;
	virtual void endParse(const shared_ptr<Recognizer> &rec, const shared_ptr<HandlerContextBase> &ctx, const string &input, size_t begin, size_t count)=0;
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
	virtual shared_ptr<HandlerContextBase> beginParse(const shared_ptr<Recognizer> &rec);
	virtual void endParse(const shared_ptr<Recognizer> &rec, const shared_ptr<HandlerContextBase> &ctx, const string &input, size_t begin, size_t count);
	virtual shared_ptr<HandlerContextBase> branch();
	virtual void merge(const shared_ptr<HandlerContextBase> &other);
	virtual void removeBranch(const shared_ptr<HandlerContextBase> &other);
	shared_ptr<HandlerContext<_parserElementT>> _beginParse(const shared_ptr<Recognizer> &rec);
	void _endParse(const shared_ptr<Recognizer> &rec, const shared_ptr<HandlerContext<_parserElementT>> &ctx, const string &input, size_t begin, size_t count);
	shared_ptr<HandlerContext<_parserElementT>> _branch();
	void _merge(const shared_ptr<HandlerContext<_parserElementT>> &other);
	void _removeBranch(const shared_ptr<HandlerContext<_parserElementT>> &other);
private:
	Parser<_parserElementT> & mParser;
	list<shared_ptr<HandlerContext<_parserElementT>>> mHandlerStack;
	shared_ptr<HandlerContext<_parserElementT>> mRoot;
};

template <typename _parserElementT>
class Parser{
friend class ParserContext<_parserElementT>;
public:
	Parser(const shared_ptr<Grammar> &grammar);
	template <typename _derivedParserElementT> 
	shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> setHandler(const string &rulename,const function<_derivedParserElementT ()> & handler){
		shared_ptr<ParserHandler<_derivedParserElementT,_parserElementT>> ret;
		mHandlers[rulename]=ret=make_shared<ParserHandler<_derivedParserElementT,_parserElementT>>(handler);
		return ret;
		
	}
	_parserElementT parseInput(const string &rulename, const string &input, size_t *parsed_size);
private:
	shared_ptr<Grammar> mGrammar;
	map<string, shared_ptr<ParserHandlerBase<_parserElementT>>> mHandlers;
};


template <typename _retT>
function< _retT ()> make_fn(_retT (*arg)()){
	return function<_retT ()>(arg);
}

template <typename _arg1T, typename _arg2T>
function< void (_arg1T,_arg2T)> make_fn(void (*arg)(_arg1T,_arg2T)){
	return function< void (_arg1T,_arg2T)>(arg);
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

