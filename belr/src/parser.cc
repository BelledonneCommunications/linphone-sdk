
#include <parser.hh>
#include <iostream>
#include <algorithm>

namespace belr{
	
CollectorBase::~CollectorBase(){
}

void Assignment::invoke(void *parent, const string &input){
	if (mChild){
		shared_ptr<ParserCollector<void*>> cc=dynamic_pointer_cast<ParserCollector<void*>>(mCollector);
		if (cc){
			cc->invoke(parent, mChild->realize(input));
		}
	}else{
		string value=input.substr(mBegin, mCount);
		shared_ptr<ParserCollector<const string&>> cc1=dynamic_pointer_cast<ParserCollector<const string&>>(mCollector);
		if (cc1){
			cc1->invoke(parent, value);
			return;
		}
		shared_ptr<ParserCollector<const char*>> cc2=dynamic_pointer_cast<ParserCollector<const char*>>(mCollector);
		if (cc2){
			cc2->invoke(parent, value.c_str());
			return;
		}
		shared_ptr<ParserCollector<int>> cc3=dynamic_pointer_cast<ParserCollector<int>>(mCollector);
		if (cc3){
			cc3->invoke(parent, atoi(value.c_str()));
			return;
		}
	}
}

ParserContext::ParserContext(Parser &parser) : mParser(parser){
}

shared_ptr<HandlerContext> ParserContext::beginParse(const shared_ptr<Recognizer> &rec){
	shared_ptr<HandlerContext> ctx;
	auto it=mParser.mHandlers.find(rec->getName());
	if (it!=mParser.mHandlers.end()){
		ctx=(*it).second->createContext();
		mHandlerStack.push_back(ctx);
	}
	return ctx;
}

void ParserContext::endParse(const shared_ptr<Recognizer> &rec, const shared_ptr<HandlerContext> &ctx, const string &input, size_t begin, size_t count){
	if (ctx){
		mHandlerStack.pop_back();
	}
	if (!mHandlerStack.empty()){
		/*assign object to parent */
		mHandlerStack.back()->setChild(rec->getName(), begin, count, ctx);
		
	}else{
		/*no parent, this is our root object*/
		mRoot=ctx;
	}
}

void *ParserContext::createRootObject(const string &input){
	 return mRoot ? mRoot->realize(input) : NULL;
}

shared_ptr<HandlerContext> ParserContext::branch(){
	shared_ptr<HandlerContext> ret=mHandlerStack.back()->branch();
	mHandlerStack.push_back(ret);
	return ret;
}

void ParserContext::merge(const shared_ptr<HandlerContext> &other){
	if (mHandlerStack.back()!=other){
		cerr<<"The branch being merged is not the last one of the stack !"<<endl;
		abort();
	}
	mHandlerStack.pop_back();
	return mHandlerStack.back()->merge(other);
}

void ParserContext::removeBranch(const shared_ptr<HandlerContext> &other){
	auto it=find(mHandlerStack.rbegin(), mHandlerStack.rend(),other);
	if (it==mHandlerStack.rend()){
		cerr<<"A branch could not be found in the stack while removing it !"<<endl;
		abort();
	}else{
		advance(it,1);
		mHandlerStack.erase(it.base());
		
	}
}

shared_ptr<HandlerContext> ParserHandler::createContext(){
	return make_shared<HandlerContext>(shared_from_this());
}
	
Parser::Parser(const shared_ptr<Grammar> &grammar) : mGrammar(grammar){
	if (!mGrammar->isComplete()){
		cerr<<"Grammar not complete, aborting."<<endl;
		return;
	}
}

void * Parser::parseInput(const string &rulename, const string &input, size_t *parsed_size){
	size_t parsed;
	shared_ptr<Recognizer> rec=mGrammar->getRule(rulename);
	shared_ptr<ParserContext> pctx=make_shared<ParserContext>(*this);
	
	parsed=rec->feed(pctx, input, 0);
	if (parsed_size) *parsed_size=parsed;
	return pctx->createRootObject(input);
}


}//end of namespace