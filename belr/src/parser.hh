#ifndef parser_hh
#define parser_hh

#include <functional>

#include "belr.hh"

namespace belr{

class CollectorBase{
public:
	virtual ~CollectorBase();
};

template <typename _parserElementT, typename _valueT>
class ParserCollector : public CollectorBase{
public:
	ParserCollector(const function<void (_parserElementT , _valueT)> &fn) : mFunc(fn){
	}
	function<void (_parserElementT, _valueT)> mFunc;
	void invoke(_parserElementT obj, _valueT value){
		mFunc(obj,value);
	}
};

template <typename _parserElementT>
class HandlerContext;

template <typename _parserElementT>
class ParserHandler :  public enable_shared_from_this<ParserHandler<_parserElementT>>{
public:
	friend class HandlerContext<_parserElementT>;
	ParserHandler(const function<_parserElementT ()> &create)
		: mHandlerCreateFunc(create){
	}
	template <typename _valueT>
	shared_ptr<ParserHandler<_parserElementT>> setCollector(const string &child_rule_name, function<void (_parserElementT , _valueT)> fn){
		mCollectors[child_rule_name]=make_shared<ParserCollector<_parserElementT,_valueT>>(fn);
		return this->shared_from_this();
	}
	_parserElementT invoke(){
		return mHandlerCreateFunc();
	}
	shared_ptr<HandlerContext<_parserElementT>> createContext();
private:
	function<_parserElementT ()> mHandlerCreateFunc;
	map<string, shared_ptr<CollectorBase> > mCollectors;
};

template <typename _parserElementT>
class Assignment{
private:
	shared_ptr<CollectorBase> mCollector;
	size_t mBegin;
	size_t mCount;
	shared_ptr<HandlerContext<_parserElementT>> mChild;
public:
	Assignment(const shared_ptr<CollectorBase> &c, size_t begin, size_t count, const shared_ptr<HandlerContext<_parserElementT>> &child)
		: mCollector(c), mBegin(begin), mCount(count), mChild(child)
	{
	}
	void invoke(_parserElementT parent, const string &input);
};

class HandlerContextBase{
};

template <typename _parserElementT>
class HandlerContext : public HandlerContextBase{
public:
	HandlerContext(const shared_ptr<ParserHandler<_parserElementT>> &handler) : 
		mHandler(handler){
	}
	void setChild(const string &subrule_name, size_t begin, size_t count, const shared_ptr<HandlerContext> &child){
		auto it=mHandler->mCollectors.find(subrule_name);
		if (it!=mHandler->mCollectors.end()){
			mAssignments.push_back(Assignment<_parserElementT>((*it).second, begin, count, child));
		}
	}
	_parserElementT realize(const string &input){
		void *ret=mHandler->invoke();
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
	shared_ptr<ParserHandler<_parserElementT>> mHandler;
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
	shared_ptr<ParserHandler<_parserElementT>> setHandler(const string &rulename, function<_parserElementT ()> handler){
		shared_ptr<ParserHandler<_parserElementT>> ret;
		mHandlers[rulename]=ret=make_shared<ParserHandler<_parserElementT>>(handler);
		return ret;
		
	}
	_parserElementT parseInput(const string &rulename, const string &input, size_t *parsed_size);
private:
	shared_ptr<Grammar> mGrammar;
	map<string, shared_ptr<ParserHandler<_parserElementT>>> mHandlers;
};


}

#endif

