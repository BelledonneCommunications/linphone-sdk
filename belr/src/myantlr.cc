

#include "myantlr.hh"
#include <algorithm>

namespace myantlr{

Recognizer::Recognizer(){
	mMatchCount=0;
}

void Recognizer::reset(){
	_reset();
	mMatchCount=0;
}

ssize_t Recognizer::feed(const string &input, ssize_t pos){
	mMatchCount=_feed(input, pos);
	return mMatchCount;
}

CharRecognizer::CharRecognizer(char to_recognize) : mToRecognize(to_recognize){
}

void CharRecognizer::_reset(){
}

ssize_t CharRecognizer::_feed(const string &input, ssize_t pos){
	return input[pos]==mToRecognize ? 1 : 0;
}

Selector::Selector(){
}

void Selector::addRecognizer(const shared_ptr<Recognizer> &r){
	mElements.push_back(r);
}

void Selector::_reset(){
	for_each(mElements.begin(),mElements.end(),bind(&Recognizer::reset,placeholders::_1));
}

ssize_t Selector::_feed(const string &input, ssize_t pos){
	ssize_t matched=0;
	for (auto it=mElements.begin(); it!=mElements.end(); ++it){
		matched=(*it)->feed(input, pos);
		if (matched>0) break;
	}
	return matched;
}


}