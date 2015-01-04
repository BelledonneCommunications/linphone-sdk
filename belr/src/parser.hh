#ifndef parser_hh
#define parser_hh

#include <functional>

#include "belr.hh"

namespace belr{

class CollectorBase{
public:
	virtual ~CollectorBase();
};

template <typename _valueT>
class ParserCollector : public CollectorBase{
public:
	ParserCollector(const function<void (void *, _valueT)> &fn) : mFunc(fn){
	}
	function<void (void *, _valueT)> mFunc;
	void invoke(void *obj, _valueT value){
		mFunc(obj,value);
	}
};

class HandlerContext;

class ParserHandler :  public enable_shared_from_this<ParserHandler>{
public:
	friend class HandlerContext;
	ParserHandler(const function<void * ()> &create)
		: mHandlerCreateFunc(create){
	}
	template <typename _valueT>
	shared_ptr<ParserHandler> setCollector(const string &child_rule_name, function<void (void * , _valueT)> fn){
		mCollectors[child_rule_name]=make_shared<ParserCollector<_valueT>>(fn);
		return shared_from_this();
	}
	void *invoke(){
		return mHandlerCreateFunc();
	}
	shared_ptr<HandlerContext> createContext();
private:
	function<void * ()> mHandlerCreateFunc;
	map<string, shared_ptr<CollectorBase> > mCollectors;
};

class Assignment{
private:
	shared_ptr<CollectorBase> mCollector;
	size_t mBegin;
	size_t mCount;
	shared_ptr<HandlerContext> mChild;
public:
	Assignment(const shared_ptr<CollectorBase> &c, size_t begin, size_t count, const shared_ptr<HandlerContext> &child)
		: mCollector(c), mBegin(begin), mCount(count), mChild(child)
	{
	}
	void invoke(void *parent, const string &input);
};

class HandlerContext{
public:
	HandlerContext(const shared_ptr<ParserHandler> &handler) : 
		mHandler(handler){
	}
	void setChild(const string &subrule_name, size_t begin, size_t count, const shared_ptr<HandlerContext> &child){
		auto it=mHandler->mCollectors.find(subrule_name);
		if (it!=mHandler->mCollectors.end()){
			mAssignments.push_back(Assignment((*it).second, begin, count, child));
		}
	}
	void *realize(const string &input){
		void *ret=mHandler->invoke();
		for (auto it=mAssignments.begin(); it!=mAssignments.end(); ++it){
			(*it).invoke(ret,input);
		}
		return ret;
	}
	shared_ptr<HandlerContext> branch(){
		return make_shared<HandlerContext>(mHandler);
	}
	void merge(const shared_ptr<HandlerContext> &other){
		mAssignments.splice(mAssignments.begin(), other->mAssignments);
	}
private:
	shared_ptr<ParserHandler> mHandler;
	list<Assignment> mAssignments;
};

class Parser;

class ParserContext{
public:
	ParserContext(Parser &parser);
	shared_ptr<HandlerContext> beginParse(const shared_ptr<Recognizer> &rec);
	void endParse(const shared_ptr<Recognizer> &rec, const shared_ptr<HandlerContext> &ctx, const string &input, size_t begin, size_t count);
	shared_ptr<HandlerContext> branch();
	void merge(const shared_ptr<HandlerContext> &other);
	void removeBranch(const shared_ptr<HandlerContext> &other);
	void *createRootObject(const string &input);
private:
	Parser & mParser;
	list<shared_ptr<HandlerContext>> mHandlerStack;
	shared_ptr<HandlerContext> mRoot;
};

class Parser{
friend class ParserContext;
public:
	Parser(const shared_ptr<Grammar> &grammar);
	shared_ptr<ParserHandler> setHandler(const string &rulename, function<void* ()> handler){
		shared_ptr<ParserHandler> ret;
		mHandlers[rulename]=ret=make_shared<ParserHandler>(handler);
		return ret;
		
	}
	void * parseInput(const string &rulename, const string &input, size_t *parsed_size);
private:
	shared_ptr<Grammar> mGrammar;
	map<string, shared_ptr<ParserHandler>> mHandlers;
};


}

#endif

