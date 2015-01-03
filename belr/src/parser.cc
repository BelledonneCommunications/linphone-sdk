
#include <parser.hh>
#include <iostream>

namespace belr{
	
CollectorBase::~CollectorBase(){
}

ParserContext::ParserContext(const shared_ptr<Parser> &parser) : mParser(parser), mRootObject(NULL){
}

shared_ptr<HandlerContext> ParserContext::beginParse(const shared_ptr<Recognizer> &rec){
	shared_ptr<HandlerContext> ctx;
	auto it=mParser->mHandlers.find(rec->getName());
	if (it!=mParser->mHandlers.end()){
		ctx=(*it).second->createContext();
		mHandlerStack.push_back(ctx);
	}
	return ctx;
}

void ParserContext::endParse(const shared_ptr<Recognizer> &rec, const shared_ptr<HandlerContext> &ctx, const string &input, size_t begin, size_t count){
	if (ctx){
		/*assign object to parent */
		shared_ptr<HandlerContext> current=mHandlerStack.back();
		mHandlerStack.pop_back();
		if (!mHandlerStack.empty()){
			mHandlerStack.back()->invoke(rec->getName(),current->getObj());
		}
	}else{
		//no specific handler for this rule, check for a collector from parent
		if (!mHandlerStack.empty()){
			shared_ptr<HandlerContext> hctx=mHandlerStack.back();
			hctx->invoke(rec->getName(), input.substr(begin, count));
		}
	}
}

shared_ptr<HandlerContext> ParserHandler::createContext(){
	return make_shared<HandlerContext>(shared_from_this(), invoke());
}
	
Parser::Parser(const shared_ptr<Grammar> &grammar) : mGrammar(grammar){
	
}

void * Parser::parseInput(const string &rulename, const string &input, size_t *parsed_size){
	size_t parsed;
	shared_ptr<Recognizer> rec=mGrammar->getRule(rulename);
	shared_ptr<ParserContext> pctx=make_shared<ParserContext>(shared_from_this());
	
	parsed=rec->feed(pctx, input, 0);
	if (parsed_size) *parsed_size=parsed;
	return pctx->getRootObject();
}

}//end of namespace