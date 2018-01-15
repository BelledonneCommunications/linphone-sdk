/*
 * Copyright (C) 2017  Belledonne Communications SARL
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _PARSER_H_
#define _PARSER_H_

#include <algorithm>
#include <functional>
#include <iostream>
#include <vector>
#include <sstream>

#include "belr.h"

// =============================================================================

namespace belr {

template<typename _parserElementT>
class AbstractCollector{
public:
	virtual ~AbstractCollector() = default;

	virtual void invokeWithChild(_parserElementT obj, _parserElementT child)=0;
};

template<typename _parserElementT, typename _valueT>
class CollectorBase : public AbstractCollector<_parserElementT>{
public:
	virtual void invoke(_parserElementT obj, _valueT value)=0;
};

template <typename _derivedParserElementT, typename _parserElementT, typename _valueT>
class ParserCollector : public CollectorBase<_parserElementT,_valueT>{
public:
	ParserCollector(const std::function<void (_derivedParserElementT , _valueT)> &fn) : mFunc(fn) {}

	void invoke(_parserElementT obj, _valueT value) override;
	void invokeWithChild(_parserElementT obj, _parserElementT child) override;

private:
	std::function<void (_derivedParserElementT, _valueT)> mFunc;
};

template <typename _derivedParserElementT, typename _parserElementT, typename _valueT>
class ParserChildCollector : public CollectorBase<_parserElementT,_valueT>{
public:
	ParserChildCollector(const std::function<void (_derivedParserElementT , _valueT)> &fn) : mFunc(fn){}

	void invoke(_parserElementT obj, _valueT value) override;
	void invokeWithChild(_parserElementT obj, _parserElementT child) override;

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
	const std::string &getRulename() const {
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
		: ParserHandlerBase<_parserElementT>(parser, rulename), mHandlerCreateFunc(create){}
	ParserHandler(const Parser<_parserElementT> &parser, const std::string &rulename, const std::function<_derivedParserElementT (const std::string &, const std::string &)> &create)
		: ParserHandlerBase<_parserElementT>(parser, rulename), mHandlerCreateDebugFunc(create){}

	_parserElementT invoke(const std::string &input, size_t begin, size_t count) override;

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

private:
	std::function<_derivedParserElementT ()> mHandlerCreateFunc;
	std::function<_derivedParserElementT (const std::string &, const std::string &)> mHandlerCreateDebugFunc;
};

template <typename _parserElementT>
class Assignment{
public:
	Assignment(const std::shared_ptr<AbstractCollector<_parserElementT>> &c, size_t begin, size_t count, const std::shared_ptr<HandlerContext<_parserElementT>> &child)
	: mCollector(c.get()), mBegin(begin), mCount(count), mChild(child) {}

	void invoke(_parserElementT parent, const std::string &input);

private:
	AbstractCollector<_parserElementT> * mCollector;//not a shared_ptr for optimization, the collector cannot disapear
	size_t mBegin;
	size_t mCount;
	std::shared_ptr<HandlerContext<_parserElementT>> mChild;
};

class HandlerContextBase : public std::enable_shared_from_this<HandlerContextBase>{
public:
	BELR_PUBLIC virtual ~HandlerContextBase() = default;
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
	void set(const std::shared_ptr<HandlerContextBase>& hc, const std::shared_ptr<Recognizer>& rec, size_t pos){
		mHandlerContext=hc;
		mRecognizer=rec.get();
		mAssignmentPos=pos;
	}
	std::shared_ptr<HandlerContextBase> mHandlerContext;
	Recognizer * mRecognizer = nullptr; //not a shared ptr to optimize, the object can't disapear in the context of use of ParserLocalContext.
	size_t mAssignmentPos = 0;
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
	void beginParse(ParserLocalContext &ctx, const std::shared_ptr<Recognizer> &rec) override;
	void endParse(const ParserLocalContext &ctx, const std::string &input, size_t begin, size_t count) override;
	std::shared_ptr<HandlerContextBase> branch() override;
	void merge(const std::shared_ptr<HandlerContextBase> &other) override;
	void removeBranch(const std::shared_ptr<HandlerContextBase> &other) override;

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
	BELR_PUBLIC void findChildren(const std::string &rulename, std::list<std::shared_ptr<DebugElement>> &retlist)const;
	BELR_PUBLIC std::ostream &tostream(int level, std::ostream &str)const;
	BELR_PUBLIC const std::string &getValue()const;
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

// =============================================================================
// Parser impl.
// =============================================================================

template <class T, class U>
T universal_pointer_cast(const std::shared_ptr<U>& sp){
	return std::static_pointer_cast<typename T::element_type>(sp);
}

template <class T, class U>
T universal_pointer_cast(U * p){
	return static_cast<T>(p);
}

BELR_PUBLIC void fatal(const char *message);

template <typename _derivedParserElementT, typename _parserElementT, typename _valueT>
void ParserCollector<_derivedParserElementT,_parserElementT, _valueT>::invoke(_parserElementT obj, _valueT value){
	mFunc(universal_pointer_cast<_derivedParserElementT>(obj),value);
}

template <typename _derivedParserElementT, typename _parserElementT, typename _valueT>
void ParserCollector<_derivedParserElementT,_parserElementT, _valueT>::invokeWithChild(_parserElementT obj, _parserElementT child){
	fatal("We should never be called in ParserCollector<_derivedParserElementT,_parserElementT, _valueT>::invokeWithChild(_parserElementT obj, _parserElementT child)");
}

template <typename _derivedParserElementT, typename _parserElementT, typename _valueT>
void ParserChildCollector<_derivedParserElementT,_parserElementT, _valueT>::invokeWithChild(_parserElementT obj, _parserElementT value){
	mFunc(universal_pointer_cast<_derivedParserElementT>(obj),universal_pointer_cast<typename std::decay<_valueT>::type>(value));
}

template <typename _derivedParserElementT, typename _parserElementT, typename _valueT>
void ParserChildCollector<_derivedParserElementT,_parserElementT, _valueT>::invoke(_parserElementT obj, _valueT value){
	fatal("We should never be called in ParserChildCollector<_derivedParserElementT,_parserElementT, _valueT>::invoke(_parserElementT obj, _valueT value)");
}

template <typename _parserElementT>
void Assignment<_parserElementT>::invoke(_parserElementT parent, const std::string &input){
	if (mChild){
		mCollector->invokeWithChild(parent, mChild->realize(input,mBegin,mCount));
	}else{
		std::string value=input.substr(mBegin, mCount);
		CollectorBase<_parserElementT,const std::string&>* cc1=dynamic_cast<CollectorBase<_parserElementT,const std::string&>*>(mCollector);
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
HandlerContext<_parserElementT>::HandlerContext(const std::shared_ptr<ParserHandlerBase<_parserElementT>> &handler) :
	mHandler(*handler.get()){
}

template <typename _parserElementT>
void HandlerContext<_parserElementT>::setChild(unsigned int subrule_id, size_t begin, size_t count, const std::shared_ptr<HandlerContext<_parserElementT>> &child){
	auto collector=mHandler.getCollector(subrule_id);
	if (collector){
		mAssignments.push_back(Assignment<_parserElementT>(collector, begin, count, child));
	}
}

template <typename _parserElementT>
_parserElementT HandlerContext<_parserElementT>::realize(const std::string &input, size_t begin, size_t count){
	_parserElementT ret=mHandler.invoke(input, begin, count);
	for (auto it=mAssignments.begin(); it!=mAssignments.end(); ++it){
		(*it).invoke(ret,input);
	}
	return ret;
}

template <typename _parserElementT>
std::shared_ptr<HandlerContext<_parserElementT>> HandlerContext<_parserElementT>::branch(){
	return mHandler.createContext();
}

template <typename _parserElementT>
void HandlerContext<_parserElementT>::merge(const std::shared_ptr<HandlerContext<_parserElementT>> &other){
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
	mHandler.releaseContext(std::static_pointer_cast<HandlerContext< _parserElementT >>(shared_from_this()));
}

//
// ParserHandlerBase template class implementation
//

template <typename _parserElementT>
ParserHandlerBase<_parserElementT>::ParserHandlerBase(const Parser<_parserElementT> &parser, const std::string &name) : mParser(parser), mRulename(tolower(name)), mCachedContext(nullptr) {
}

template <typename _parserElementT>
void ParserHandlerBase<_parserElementT>::installCollector(const std::string &rulename, const std::shared_ptr<AbstractCollector<_parserElementT>> &collector){
	std::shared_ptr<Recognizer> rec=mParser.mGrammar->findRule(rulename);
	if (!rec){
		std::ostringstream ostr;
		ostr<<"There is no rule '"<<rulename<<"' in the grammar.";
		fatal(ostr.str().c_str());
		return;
	}
	mCollectors[rec->getId()]=collector;
}

template <typename _parserElementT>
const std::shared_ptr<AbstractCollector<_parserElementT>> & ParserHandlerBase<_parserElementT>::getCollector(unsigned int rule_id)const{
	auto it=mCollectors.find(rule_id);
	if (it!=mCollectors.end()) return (*it).second;
	return mParser.mNullCollector;
}

template <typename _parserElementT>
void ParserHandlerBase< _parserElementT >::releaseContext(const std::shared_ptr<HandlerContext<_parserElementT>> &ctx){
	mCachedContext=ctx;
}

template <typename _parserElementT>
std::shared_ptr<HandlerContext<_parserElementT>> ParserHandlerBase<_parserElementT>::createContext(){
	if (mCachedContext) {
		std::shared_ptr<HandlerContext<_parserElementT>> ret=mCachedContext;
		mCachedContext.reset();
		return ret;
	}
	return std::make_shared<HandlerContext<_parserElementT>>(this->shared_from_this());
}

//
// ParserHandler template implementation
//

template <typename _derivedParserElementT, typename _parserElementT>
_parserElementT ParserHandler<_derivedParserElementT,_parserElementT>::invoke(const std::string &input, size_t begin, size_t count){
	if (mHandlerCreateFunc)
		return universal_pointer_cast<_parserElementT>(mHandlerCreateFunc());
	if (mHandlerCreateDebugFunc)
		return universal_pointer_cast<_parserElementT>(mHandlerCreateDebugFunc(this->getRulename(), input.substr(begin, count)));
	return nullptr;
}


//
// ParserContext template class implementation
//

template <typename _parserElementT>
ParserContext<_parserElementT>::ParserContext(Parser<_parserElementT> &parser) : mParser(parser) {
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::_beginParse(ParserLocalContext & lctx, const std::shared_ptr<Recognizer> &rec){
	std::shared_ptr<HandlerContextBase> ctx;

	auto h=mParser.getHandler(rec->getId());
	if (h){
		ctx=h->createContext();
		mHandlerStack.push_back(std::static_pointer_cast<HandlerContext<_parserElementT>>(ctx));
	}
	if (mHandlerStack.empty()){
		fatal("Cannot parse when mHandlerStack is empty. You must define a top-level rule handler.");
	}
	lctx.set(ctx,rec,mHandlerStack.back()->getLastIterator());
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::_endParse(const ParserLocalContext &localctx, const std::string &input, size_t begin, size_t count){
	if (localctx.mHandlerContext){
		mHandlerStack.pop_back();
		if (count!=std::string::npos && count>0){
			if (!mHandlerStack.empty()){
				/*assign object to parent */
				mHandlerStack.back()->setChild(localctx.mRecognizer->getId(), begin, count,
					std::static_pointer_cast<HandlerContext< _parserElementT >> (localctx.mHandlerContext));

			}else{
				/*no parent, this is our root object*/
				mRoot=std::static_pointer_cast<HandlerContext< _parserElementT >>(localctx.mHandlerContext);
			}
		}else{
			//no match
			std::static_pointer_cast<HandlerContext< _parserElementT >>(localctx.mHandlerContext)->recycle();
		}
	}else{
		if (count!=std::string::npos && count>0){
			/*assign std::string to parent */
			if (localctx.mRecognizer->getId()!=0)
				mHandlerStack.back()->setChild(localctx.mRecognizer->getId(), begin, count, nullptr);
		}else{
			mHandlerStack.back()->undoAssignments(localctx.mAssignmentPos);
		}
	}
}

template <typename _parserElementT>
_parserElementT ParserContext<_parserElementT>::createRootObject(const std::string &input){
	 return mRoot ? mRoot->realize(input,0,input.size()) : nullptr;
}

template <typename _parserElementT>
std::shared_ptr<HandlerContext<_parserElementT>> ParserContext<_parserElementT>::_branch(){
	if (mHandlerStack.empty()){
		fatal("Cannot branch while stack is empty");
	}
	std::shared_ptr<HandlerContext<_parserElementT>> ret=mHandlerStack.back()->branch();
	mHandlerStack.push_back(ret);
	return ret;
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::_merge(const std::shared_ptr<HandlerContext<_parserElementT>> &other){
	if (mHandlerStack.back()!=other){
		fatal("The branch being merged is not the last one of the stack !");
	}
	mHandlerStack.pop_back();
	mHandlerStack.back()->merge(other);
	other->recycle();
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::_removeBranch(const std::shared_ptr<HandlerContext<_parserElementT>> &other){
	auto it=find(mHandlerStack.rbegin(), mHandlerStack.rend(),other);
	if (it==mHandlerStack.rend()){
		fatal("A branch could not be found in the stack while removing it !");
	}else{
		advance(it,1);
		mHandlerStack.erase(it.base());
	}
	other->recycle();
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::beginParse(ParserLocalContext &ctx, const std::shared_ptr<Recognizer> &rec){
	_beginParse(ctx, rec);
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::endParse(const ParserLocalContext &localctx, const std::string &input, size_t begin, size_t count){
	_endParse(localctx, input, begin, count);
}

template <typename _parserElementT>
std::shared_ptr<HandlerContextBase> ParserContext<_parserElementT>::branch(){
	return _branch();
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::merge(const std::shared_ptr<HandlerContextBase> &other){
	_merge(std::static_pointer_cast<HandlerContext<_parserElementT>>(other));
}

template <typename _parserElementT>
void ParserContext<_parserElementT>::removeBranch(const std::shared_ptr<HandlerContextBase> &other){
	_removeBranch(std::static_pointer_cast<HandlerContext<_parserElementT>>(other));
}

//
// Parser template class implementation
//

template <typename _parserElementT>
Parser<_parserElementT>::Parser(const std::shared_ptr<Grammar> &grammar) : mGrammar(grammar) {
	if (!mGrammar->isComplete()){
		fatal("Grammar not complete, aborting.");
		return;
	}
}

template <typename _parserElementT>
std::shared_ptr<ParserHandlerBase<_parserElementT>> &Parser<_parserElementT>::getHandler(unsigned int rule_id){
	auto it=mHandlers.find(rule_id);
	if (it==mHandlers.end()) return mNullHandler;
	return (*it).second;
}

template <typename _parserElementT>
void Parser<_parserElementT>::installHandler(const std::shared_ptr<ParserHandlerBase<_parserElementT>> &handler){
	std::shared_ptr<Recognizer> rec=mGrammar->findRule(handler->getRulename());
	if (!rec){
		std::ostringstream str;
		str<<"There is no rule '"<<handler->getRulename()<<"' in the grammar.";
		fatal(str.str().c_str());
	}
	mHandlers[rec->getId()]=handler;
}

template <typename _parserElementT>
_parserElementT Parser<_parserElementT>::parseInput(const std::string &rulename, const std::string &input, size_t *parsed_size){
	size_t parsed;
	std::shared_ptr<Recognizer> rec=mGrammar->getRule(rulename);
	auto pctx=std::make_shared<ParserContext<_parserElementT>>(*this);

	//auto t_start = std::chrono::high_resolution_clock::now();
	parsed=rec->feed(pctx, input, 0);
	//auto t_end = std::chrono::high_resolution_clock::now();
	//cout<<"Recognition done in "<<std::chrono::duration<double, std::milli>(t_end-t_start).count()<<" milliseconds"<<std::endl;
	if (parsed_size) *parsed_size=parsed;
	auto ret= pctx->createRootObject(input);
	return ret;
}
}

#endif
