

#include "belr.hh"
#include <algorithm>
#include <iostream>

namespace belr{

Recognizer::Recognizer(){
}

void Recognizer::reset(){
	_reset();
}

void Recognizer::setName(const string& name){
	mName=name;
}

size_t Recognizer::feed(const string &input, size_t pos){
	cout<<"Trying rule "<<mName<<endl;
	size_t match=_feed(input, pos);
	if (match!=string::npos && match>0 && mName.size()>0){
		string matched=input.substr(pos,match);
		cout<<"Matched recognizer '"<<mName<<"' with sequence '"<<matched<<"'."<<endl;
	}
	return match;
}

CharRecognizer::CharRecognizer(char to_recognize) : mToRecognize(to_recognize){
}

void CharRecognizer::_reset(){
}

size_t CharRecognizer::_feed(const string &input, size_t pos){
	return input[pos]==mToRecognize ? 1 : string::npos;
}

Selector::Selector(){
}

shared_ptr<Selector> Selector::addRecognizer(const shared_ptr<Recognizer> &r){
	mElements.push_back(r);
	return shared_from_this();
}

void Selector::_reset(){
	for_each(mElements.begin(),mElements.end(),bind(&Recognizer::reset,placeholders::_1));
}

size_t Selector::_feed(const string &input, size_t pos){
	size_t matched=0;
	
	cout<<"Selector"<<endl;
	for (auto it=mElements.begin(); it!=mElements.end(); ++it){
		matched=(*it)->feed(input, pos);
		if (matched!=string::npos && matched>0) break;
	}
	if (matched==string::npos || matched==0) return string::npos;
	return matched;
}

Sequence::Sequence(){
}

shared_ptr<Sequence> Sequence::addRecognizer(const shared_ptr<Recognizer> &element){
	mElements.push_back(element);
	return shared_from_this();
}

void Sequence::_reset(){
	for_each(mElements.begin(),mElements.end(),bind(&Recognizer::reset,placeholders::_1));
}

size_t Sequence::_feed(const string &input, size_t pos){
	size_t matched=0;
	size_t total=0;
	
	cout<<"Sequence"<<endl;
	for (auto it=mElements.begin(); it!=mElements.end(); ++it){
		matched=(*it)->feed(input, pos);
		if (matched==string::npos){
			return string::npos;
		}
		pos+=matched;
		total+=matched;
		cout<<"Sequence: matched="<<matched<<endl;
	}
	return total;
}

Loop::Loop(){
	mMin=0;
	mMax=-1;
}

shared_ptr<Loop> Loop::setRecognizer(const shared_ptr<Recognizer> &element, int min, int max){
	mMin=min;
	mMax=max;
	mRecognizer=element;
	return shared_from_this();
}

void Loop::_reset(){
	mRecognizer.reset();
}

size_t Loop::_feed(const string &input, size_t pos){
	size_t matched=0;
	size_t total=0;
	int repeat;
	
	cout<<"Loop"<<endl;
	for(repeat=0;mMax!=-1 ? repeat<mMax : true;repeat++){
		matched=mRecognizer->feed(input,pos);
		if (matched==string::npos) break;
		total+=matched;
		pos+=matched;
		cout<<"Loop: repeat="<<repeat<<" matched="<<matched<<endl;
	}
	if (repeat<mMin) return string::npos;
	return total;
}

shared_ptr<CharRecognizer> Foundation::charRecognizer(char character){
	return make_shared<CharRecognizer>(character);
}

shared_ptr<Selector> Foundation::selector(){
	return make_shared<Selector>();
}

shared_ptr<Sequence> Foundation::sequence(){
	return make_shared<Sequence>();
}

shared_ptr<Loop> Foundation::loop(){
	return make_shared<Loop>();
}

shared_ptr<Recognizer> Utils::literal(const string & lt){
	shared_ptr<Sequence> seq=Foundation::sequence();
	size_t i;
	for (i=0;i<lt.size();++i){
		seq->addRecognizer(Foundation::charRecognizer(lt[i]));
	}
	return seq;
}

shared_ptr<Recognizer> Utils::char_range(int begin, int end){
	auto sel=Foundation::selector();
	for(int i=begin; i<=end; i++){
		sel->addRecognizer(
			Foundation::charRecognizer((char)i)
		);
	}
	return sel;
}

RecognizerPointer::RecognizerPointer() : mRecognizer(NULL){
}

shared_ptr<Recognizer> RecognizerPointer::getPointed(){
	return mRecognizer;
}

void RecognizerPointer::_reset(){
	if (mRecognizer) mRecognizer->reset();
}

size_t RecognizerPointer::_feed(const string &input, size_t pos){
	if (mRecognizer){
		return mRecognizer->feed(input,pos);
	}else{
		cerr<<"RecognizerPointer is undefined"<<endl;
		abort();
	}
	return string::npos;
}

void RecognizerPointer::setPointed(const shared_ptr<Recognizer> &r){
	mRecognizer=r;
}

Grammar::Grammar(const string& name) : mName(name){

}


void Grammar::assignRule(const string &argname, const shared_ptr<Recognizer> &rule){
	string name=toLower(argname);
	rule->setName(name);
	auto it=mRules.find(name);
	if (it!=mRules.end()){
		shared_ptr<RecognizerPointer> pointer=dynamic_pointer_cast<RecognizerPointer>((*it).second);
		if (pointer){
			pointer->setPointed(rule);
		}else{
			cerr<<"Error: rule '"<<name<<"' is being redefined !"<<endl;
			abort();
		}
	}else{
		mRules[name]=rule;
	}
}

shared_ptr<Recognizer> Grammar::getRule(const string &argname){
	shared_ptr<Recognizer> ret;
	string name=toLower(argname);
	auto it=mRules.find(name);
	if (it!=mRules.end()){
		shared_ptr<RecognizerPointer> pointer=dynamic_pointer_cast<RecognizerPointer>((*it).second);
		if (pointer){
			if (pointer->getPointed()){/*if pointer is defined return the pointed value directly*/
				return pointer->getPointed();
			}else{
				return pointer;
			}
		}
		return (*it).second;
	}else{/*the rule doesn't exist yet: return a pointer*/
		ret=make_shared<RecognizerPointer>();
		mRules[name]=ret;
	}
	return ret;
}

void Grammar::include(const shared_ptr<Grammar>& grammar){
	for(auto it=grammar->mRules.begin();it!=grammar->mRules.end();++it){
		if (mRules.find((*it).first)!=mRules.end()){
			cerr<<"Rule '"<<(*it).first<<"' is being redefined while including grammar '"<<grammar->mName<<"' into '"<<mName<<"'"<<endl;
		}
		mRules[(*it).first]=(*it).second;
	}
}

bool Grammar::isComplete()const{
	bool ret=true;
	for(auto it=mRules.begin(); it!=mRules.end(); ++it){
		shared_ptr<RecognizerPointer> pointer=dynamic_pointer_cast<RecognizerPointer>((*it).second);
		if (pointer && !pointer->getPointed()){
			cerr<<"Rule '"<<(*it).first<<"' is not defined."<<endl;
			ret=false;
		}
	}
	return ret;
}

string Grammar::toLower(const string &str){
	string ret(str);
	transform(ret.begin(),ret.end(), ret.begin(), ::tolower);
	return ret;
}

}
