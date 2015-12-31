
#include "belr/parser.hh"
#include <iostream>
#include <algorithm>
//#include <chrono>
#include <ctime>

namespace belr{

template <class T, class U>
T universal_pointer_cast(const shared_ptr<U>& sp){
	return static_pointer_cast<typename T::element_type>(sp);
}

template <class T, class U>
T universal_pointer_cast(U * p){
	return static_cast<T>(p);
}
	
template <typename _parserElementT>
AbstractCollector<_parserElementT>::~AbstractCollector(){
}

template <typename _derivedParserElementT, typename _parserElementT, typename _valueT>
void ParserCollector<_derivedParserElementT,_parserElementT, _valueT>::invoke(_parserElementT obj, _valueT value){
	mFunc(universal_pointer_cast<_derivedParserElementT>(obj),value);
}

template <typename _derivedParserElementT, typename _parserElementT, typename _valueT>
void ParserCollector<_derivedParserElementT,_parserElementT, _valueT>::invokeWithChild(_parserElementT obj, _parserElementT child){
	cerr<<"We should never be called in ParserCollector<_derivedParserElementT,_parserElementT, _valueT>::invokeWithChild(_parserElementT obj, _parserElementT child)"<<endl;
	abort();
}

template <typename _derivedParserElementT, typename _parserElementT, typename _valueT>
void ParserChildCollector<_derivedParserElementT,_parserElementT, _valueT>::invokeWithChild(_parserElementT obj, _parserElementT value){
	mFunc(universal_pointer_cast<_derivedParserElementT>(obj),universal_pointer_cast<typename decay<_valueT>::type>(value));
}

template <typename _derivedParserElementT, typename _parserElementT, typename _valueT>
void ParserChildCollector<_derivedParserElementT,_parserElementT, _valueT>::invoke(_parserElementT obj, _valueT value){
	cerr<<"We should never be called in ParserChildCollector<_derivedParserElementT,_parserElementT, _valueT>::invoke(_parserElementT obj, _valueT value)"<<endl;
	abort();
}

template <typename _parserElementT>
void Assignment<_parserElementT>::invoke(_parserElementT parent, const string &input){
	if (mChild){
		mCollector->invokeWithChild(parent, mChild->realize(input,mBegin,mCount));
	}else{
		string value=input.substr(mBegin, mCount);
		CollectorBase<_parserElementT,const string&>* cc1=dynamic_cast<CollectorBase<_parserElementT,const string&>*>(mCollector);
		if (cc1){
			cc1->invoke(parent, value);
			return;
		}
		CollectorBase<_parserElementT,const char*>* cc2=dynamic_cast<CollectorBase<_parserElementT,const char*>*>(mCollector);
		if (cc2){
			cc2->invoke(parent, value.c_str());
			return;
		}
		CollectorBase<_parserElementT,int> *cc3=dynamic_cast<CollectorBase<_parserElementT,int>*>(mCollector);
		if (cc3){
			cc3->invoke(parent, atoi(value.c_str()));
			return;
		}
	}
}


//
// HandlerContext template class implementation
//

template <typename _parserElementT>
HandlerContext<_parserElementT>::HandlerContext(const shared_ptr<ParserHandlerBase<_parserElementT>> &handler) : 
	mHandler(*handler.get()){
}

template <typename _parserElementT>
void HandlerContext<_parserElementT>::setChild(unsigned int subrule_id, size_t begin, size_t count, const shared_ptr<HandlerContext<_parserElementT>> &child){
	auto collector=mHandler.getCollector(subrule_id);
	if (collector){
		mAssignments.push_back(Assignment<_parserElementT>(collector, begin, count, child));
	}
}

template <typename _parserElementT>
_parserElementT HandlerContext<_parserElementT>::realize(const string &input, size_t begin, size_t count){
	_parserElementT ret=mHandler.invoke(input, begin, count);
	for (auto it=mAssignments.begin(); it!=mAssignments.end(); ++it){
		(*it).invoke(ret,input);
	}
	return ret;
}

template <typename _parserElementT>
shared_ptr<HandlerContext<_parserElementT>> HandlerContext<_parserElementT>::branch(){
	return mHandler.createContext();
}

template <typename _parserElementT>
void HandlerContext<_parserElementT>::merge(const shared_ptr<HandlerContext<_parserElementT>> &other){
	for (auto it=other->mAssignments.begin();it!=other->mAssignments.end();++it){
		mAssignments.emplace_back(*it);
	}
}

template <typename _parserElementT>
size_t HandlerContext<_parserElementT>::getLastIterator()const{
	return mAssignments.size();
}

template <typename _parserElementT>
void HandlerContext<_parserElementT>::undoAssignments(size_t pos){
	mAssignments.erase(mAssignments.begin()+pos,mAssignments.end());
}

template <typename _parserElementT>
void HandlerContext< _parserElementT >::recycle(){
	mAssignments.clear();
	mHandler.releaseContext(static_pointer_cast<HandlerContext< _parserElementT >>(shared_from_this()));
}

//
// ParserHandlerBase template class implementation
//

template <typename _parserElementT>
ParserHandlerBase<_parserElementT>::ParserHandlerBase(const Parser<_parserElementT> &parser, const string &name) : mParser(parser), mRulename(tolower(name)), mCachedContext(NULL) {
}

template <typename _parserElementT>
void ParserHandlerBase<_parserElementT>::installCollector(const string &rulename, const shared_ptr<AbstractCollector<_parserElementT>> &collector){
	shared_ptr<Recognizer> rec=mParser.mGrammar->findRule(rulename);
	if (!rec){
		cerr<<"There is no rule '"<<rulename<<"' in the grammar."<<endl;
		return;
	}
	mCollectors[rec->getId()]=collector;
}

template <typename _parserElementT>
const shared_ptr<AbstractCollector<_parserElementT>> & ParserHandlerBase<_parserElementT>::getCollector(unsigned int rule_id)const{
	auto it=mCollectors.find(rule_id);
	if (it!=mCollectors.end()) return (*it).second;
	return mParser.mNullCollector;
}

template <typename _parserElementT>
void ParserHandlerBase< _parserElementT >::releaseContext(const shared_ptr<HandlerContext<_parserElementT>> &ctx){
	mCachedContext=ctx;
}

template <typename _parserElementT>
shared_ptr<HandlerContext<_parserElementT>> ParserHandlerBase<_parserElementT>::createContext(){
	if (mCachedContext) {
		shared_ptr<HandlerContext<_parserElementT>> ret=mCachedContext;
		mCachedContext.reset();
		return ret;
	}
	return make_shared<HandlerContext<_parserElementT>>(this->shared_from_this());
}

//
// ParserHandler template implementation
//

template <typename _derivedParserElementT, typename _parserElementT>
_parserElementT ParserHandler<_derivedParserElementT,_parserElementT>::invoke(const string &input, size_t begin, size_t count){
	if (mHandlerCreateFunc)
		return universal_pointer_cast<_parserElementT>(mHandlerCreateFunc());
	if (mHandlerCreateDebugFunc)
		return universal_pointer_cast<_parserElementT>(mHandlerCreateDebugFunc(this->getRulename(), input.substr(begin, count)));
	return NULL;
}


//
// ParserContext template class implementation
//

template <typename _parserElementT>
ParserContext<_parserElementT>::ParserContext(Parser<_parserElementT> &parser) : mParser(parser) {
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::_beginParse(ParserLocalContext & lctx, const shared_ptr<Recognizer> &rec){
	shared_ptr<HandlerContextBase> ctx;

	auto h=mParser.getHandler(rec->getId());
	if (h){
		ctx=h->createContext();
		mHandlerStack.push_back(static_pointer_cast<HandlerContext<_parserElementT>>(ctx));
	}
	lctx.set(ctx,rec,mHandlerStack.back()->getLastIterator());
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::_endParse(const ParserLocalContext &localctx, const string &input, size_t begin, size_t count){
	if (localctx.mHandlerContext){
		mHandlerStack.pop_back();
		if (count!=string::npos && count>0){
			if (!mHandlerStack.empty()){
				/*assign object to parent */
				mHandlerStack.back()->setChild(localctx.mRecognizer->getId(), begin, count, 
					static_pointer_cast<HandlerContext< _parserElementT >> (localctx.mHandlerContext));
				
			}else{
				/*no parent, this is our root object*/
				mRoot=static_pointer_cast<HandlerContext< _parserElementT >>(localctx.mHandlerContext);
			}
		}else{
			//no match
			static_pointer_cast<HandlerContext< _parserElementT >>(localctx.mHandlerContext)->recycle();
		}
	}else{
		if (count!=string::npos && count>0){
			/*assign string to parent */
			if (localctx.mRecognizer->getId()!=0)
				mHandlerStack.back()->setChild(localctx.mRecognizer->getId(), begin, count, NULL);
		}else{
			mHandlerStack.back()->undoAssignments(localctx.mAssignmentPos);
		}
	}
}

template <typename _parserElementT>
_parserElementT ParserContext<_parserElementT>::createRootObject(const string &input){
	 return mRoot ? mRoot->realize(input,0,input.size()) : NULL;
}

template <typename _parserElementT>
shared_ptr<HandlerContext<_parserElementT>> ParserContext<_parserElementT>::_branch(){
	if (mHandlerStack.empty()){
		cerr<<"Cannot branch while stack is empty"<<endl;
	}
	shared_ptr<HandlerContext<_parserElementT>> ret=mHandlerStack.back()->branch();
	mHandlerStack.push_back(ret);
	return ret;
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::_merge(const shared_ptr<HandlerContext<_parserElementT>> &other){
	if (mHandlerStack.back()!=other){
		cerr<<"The branch being merged is not the last one of the stack !"<<endl;
		abort();
	}
	mHandlerStack.pop_back();
	mHandlerStack.back()->merge(other);
	other->recycle();
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::_removeBranch(const shared_ptr<HandlerContext<_parserElementT>> &other){
	auto it=find(mHandlerStack.rbegin(), mHandlerStack.rend(),other);
	if (it==mHandlerStack.rend()){
		cerr<<"A branch could not be found in the stack while removing it !"<<endl;
		abort();
	}else{
		advance(it,1);
		mHandlerStack.erase(it.base());
	}
	other->recycle();
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::beginParse(ParserLocalContext &ctx, const shared_ptr<Recognizer> &rec){
	_beginParse(ctx, rec);
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::endParse(const ParserLocalContext &localctx, const string &input, size_t begin, size_t count){
	_endParse(localctx, input, begin, count);
}

template <typename _parserElementT>
shared_ptr<HandlerContextBase> ParserContext<_parserElementT>::branch(){
	return _branch();
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::merge(const shared_ptr<HandlerContextBase> &other){
	_merge(static_pointer_cast<HandlerContext<_parserElementT>>(other));
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::removeBranch(const shared_ptr<HandlerContextBase> &other){
	_removeBranch(static_pointer_cast<HandlerContext<_parserElementT>>(other));
}

//
// Parser template class implementation
//

template <typename _parserElementT>
Parser<_parserElementT>::Parser(const shared_ptr<Grammar> &grammar) : mGrammar(grammar) {
	if (!mGrammar->isComplete()){
		cerr<<"Grammar not complete, aborting."<<endl;
		return;
	}
}

template <typename _parserElementT>
shared_ptr<ParserHandlerBase<_parserElementT>> &Parser<_parserElementT>::getHandler(unsigned int rule_id){
	auto it=mHandlers.find(rule_id);
	if (it==mHandlers.end()) return mNullHandler;
	return (*it).second;
}

template <typename _parserElementT>
void Parser<_parserElementT>::installHandler(const shared_ptr<ParserHandlerBase<_parserElementT>> &handler){
	shared_ptr<Recognizer> rec=mGrammar->findRule(handler->getRulename());
	if (rec==NULL){
		cerr<<"There is no rule '"<<handler->getRulename()<<"' in the grammar."<<endl;
		return;
	}
	mHandlers[rec->getId()]=handler;
}

template <typename _parserElementT>
_parserElementT Parser<_parserElementT>::parseInput(const string &rulename, const string &input, size_t *parsed_size){
	size_t parsed;
	shared_ptr<Recognizer> rec=mGrammar->getRule(rulename);
	auto pctx=make_shared<ParserContext<_parserElementT>>(*this);
	
	//auto t_start = std::chrono::high_resolution_clock::now();
	parsed=rec->feed(pctx, input, 0);
	//auto t_end = std::chrono::high_resolution_clock::now();
	//cout<<"Recognition done in "<<std::chrono::duration<double, std::milli>(t_end-t_start).count()<<" milliseconds"<<endl;
	if (parsed_size) *parsed_size=parsed;
	auto ret= pctx->createRootObject(input);
	return ret;
}

}
