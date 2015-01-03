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
	ParserHandler(const function<void * ()> &fn) : mHandlerFunc(fn){
	}
	template <typename _valueT>
	shared_ptr<ParserHandler> setCollector(const string &child_rule_name, const function<void (void * , const _valueT)> &fn){
		mCollectors[child_rule_name]=make_shared<ParserCollector<_valueT>>(fn);
		return shared_from_this();
	}
	void *invoke(){
		return mHandlerFunc();
	}
	shared_ptr<HandlerContext> createContext();
private:
	function<void * ()> mHandlerFunc;
	map<string, shared_ptr<CollectorBase> > mCollectors;
};

class HandlerContext{
public:
	HandlerContext(const shared_ptr<ParserHandler> &handler, void *obj) : 
		mHandler(handler), mObj(obj){
	}
	void invoke(const string &subrule_name, void *subobj){
		auto it=mHandler->mCollectors.find(subrule_name);
		if (it!=mHandler->mCollectors.end()){
			shared_ptr<CollectorBase> c=(*it).second;
			shared_ptr<ParserCollector<void*>> cc=dynamic_pointer_cast<ParserCollector<void*>>(c);
			if (cc){
				cc->invoke(mObj, subobj);
			}
		}
	}
	void invoke(const string &subrule_name, const string &value){
		auto it=mHandler->mCollectors.find(subrule_name);
		if (it!=mHandler->mCollectors.end()){
			shared_ptr<CollectorBase> c=(*it).second;
			shared_ptr<ParserCollector<const string&>> cc1=dynamic_pointer_cast<ParserCollector<const string&>>(c);
			if (cc1){
				cc1->invoke(mObj, value);
				return;
			}
			shared_ptr<ParserCollector<const char*>> cc2=dynamic_pointer_cast<ParserCollector<const char*>>(c);
			if (cc2){
				cc2->invoke(mObj, value.c_str());
				return;
			}
			shared_ptr<ParserCollector<int>> cc3=dynamic_pointer_cast<ParserCollector<int>>(c);
			if (cc3){
				cc3->invoke(mObj, atoi(value.c_str()));
				return;
			}
		}
	}
	void *getObj()const{
		return mObj;
	}
private:
	shared_ptr<ParserHandler> mHandler;
	void *mObj;
};

class Parser;

class ParserContext{
public:
	ParserContext(const shared_ptr<Parser> &parser);
	shared_ptr<HandlerContext> beginParse(const shared_ptr<Recognizer> &rec);
	void endParse(const shared_ptr<Recognizer> &rec, const shared_ptr<HandlerContext> &ctx, const string &input, size_t begin, size_t count);
	void *getRootObject()const{
		return mRootObject;
	}
private:
	shared_ptr<Parser> mParser;
	list<shared_ptr<HandlerContext>> mHandlerStack;
	void *mRootObject;
};

class Parser : enable_shared_from_this<Parser>{
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
	
#if 0
class CollectorBase{
public:
};

template <typename _ParserElementT, typename _valueT>
class ParserCollector : public CollectorBase{
public:
	ParserCollector(const function<void (_ParserElementT, const _valueT)> &fn) : mFunc(fn){
	}
	function<void (_ParserElementT, const _valueT)> mFunc;
};

class ParserHandlerBase : public enable_shared_from_this<ParserHandlerBase>{
public:
};

template <typename _ElementT>
class ParserHandler : public ParserHandlerBase{
public:
	ParserHandler(const function<_ParserElementT ()> &fn) : mHandlerFunc(fn){
	}
	shared_ptr<ParserHandler<_ElementT>> setCollector(const string &child_rule_name, const shared_ptr<ParserCollector<_ElementT>> & collector);
private:
	function<_ParserElementT ()> mHandlerFunc;
	map<string, shared_ptr<CollectorBase> > mCollectors;
};


class ParserBase{
public:
	ParserBase(const shared_ptr<Grammar> &grammar);
private:
	map<string,shared_ptr<ParserHandler>> mHandlers;
};

class Parser : public ParserBase{
public:
	Parser(const shared_ptr<Grammar> &grammar);
	template <typename _ElementT>
	shared_ptr<ParserHandler<_ElementT>> setHandler(const string &rulename, function<_ElementT ()> handler);
	template <typename _ElementT>
	_ElementT parseInput(const string &rulename, const string &input, size_t *parsed_size);
};

#endif

}

#endif

