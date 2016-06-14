
#include "belr/abnf.hh"
#include "belr/grammarbuilder.hh"
#include "belr/parser-impl.cc"

#include "bctoolbox/logging.h"

#include <iostream>
#include <fstream>
#include <sstream>


namespace belr{

ABNFBuilder::~ABNFBuilder(){
}

ABNFNumval::ABNFNumval() : mIsRange(false) {
	
}

shared_ptr< ABNFNumval > ABNFNumval::create(){
	return make_shared<ABNFNumval>();
}

shared_ptr< Recognizer > ABNFNumval::buildRecognizer(const shared_ptr< Grammar >& grammar){
	if (mIsRange){
		return Utils::char_range(mValues[0],mValues[1]);
	}else{
		auto seq=Foundation::sequence();
		for (auto it=mValues.begin();it!=mValues.end();++it){
			seq->addRecognizer(Foundation::charRecognizer(*it,true));
		}
		return seq;
	}
}

void ABNFNumval::parseValues(const string &val, int base){
	size_t dash=val.find('-');
	if (dash!=string::npos){
		mIsRange=true;
		string first=val.substr(1,dash-1);
		string last=val.substr(dash+1,string::npos);
		mValues.push_back(strtol(first.c_str(),NULL,base));
		mValues.push_back(strtol(last.c_str(),NULL,base));
	}else{
		mIsRange=false;
		string tmp=val.substr(1,string::npos);
		const char *s=tmp.c_str();
		char *endptr=NULL;
		do{
			long lv=strtol(s,&endptr,base);
			if (lv == 0 && s == endptr) {
				break;
			}
			if (*endptr=='.') s=endptr+1;
			else s=endptr;
			mValues.push_back(lv);
		}while(*s!='\0');
	}
}

void ABNFNumval::setDecVal(const string& decval){
	parseValues(decval,10);
}

void ABNFNumval::setHexVal(const string& hexval){
	parseValues(hexval,16);
}

void ABNFNumval::setBinVal(const string& binval){
	parseValues(binval,2);
}

shared_ptr< Recognizer > ABNFOption::buildRecognizer(const shared_ptr< Grammar >& grammar){
	return Foundation::loop()->setRecognizer(mAlternation->buildRecognizer(grammar),0,1);
}

ABNFOption::ABNFOption() {
	
}

shared_ptr< ABNFOption > ABNFOption::create(){
	return make_shared<ABNFOption>();
}

void ABNFOption::setAlternation(const shared_ptr< ABNFAlternation >& a){
	mAlternation=a;
}

ABNFGroup::ABNFGroup() {
	
}

shared_ptr< ABNFGroup > ABNFGroup::create(){
	return make_shared<ABNFGroup>();
}

shared_ptr< Recognizer > ABNFGroup::buildRecognizer(const shared_ptr< Grammar >& grammar){
	return mAlternation->buildRecognizer(grammar);
}

void ABNFGroup::setAlternation(const shared_ptr< ABNFAlternation >& a){
	mAlternation=a;
}

shared_ptr< Recognizer > ABNFElement::buildRecognizer(const shared_ptr< Grammar >& grammar){
	if (mElement)
		return mElement->buildRecognizer(grammar);
	if (!mRulename.empty())
		return grammar->getRule(mRulename);
	if (!mCharVal.empty()){
		if (mCharVal.size()==1)
			return Foundation::charRecognizer(mCharVal[0],false);
		else 
			return Utils::literal(mCharVal);
	}
	bctbx_error("[belr] ABNFElement::buildRecognizer is empty, should not happen!");
	abort();
	return NULL;
}

ABNFElement::ABNFElement() {
	
}

shared_ptr< ABNFElement > ABNFElement::create(){
	return make_shared<ABNFElement>();
}

void ABNFElement::setElement(const shared_ptr< ABNFBuilder >& e){
	mElement=e;
}

void ABNFElement::setRulename(const string& rulename){
	mRulename=rulename;
}

void ABNFElement::setCharVal(const string& charval){
	mCharVal=charval.substr(1,charval.size()-2); //in order to remove surrounding quotes
}

void ABNFElement::setProseVal(const string& prose){
	if (!prose.empty()){
		bctbx_error("[belr] prose-val is not supported.");
		abort();
	}
}

ABNFRepetition::ABNFRepetition() : mMin(0), mMax(-1), mCount(-1) {
	
}

shared_ptr< ABNFRepetition > ABNFRepetition::create(){
	return make_shared<ABNFRepetition>();
}

void ABNFRepetition::setCount(int count){
	mCount=count;
}

void ABNFRepetition::setMin(int min){
	mMin=min;
}

void ABNFRepetition::setMax(int max){
	mMax=max;
}

void ABNFRepetition::setRepeat(const string& r){
	mRepeat=r;
}

void ABNFRepetition::setElement(const shared_ptr< ABNFElement >& e){
	mElement=e;
}

shared_ptr< Recognizer > ABNFRepetition::buildRecognizer(const shared_ptr< Grammar >& grammar){
	if (mRepeat.empty()) return mElement->buildRecognizer(grammar);
	//cout<<"Building repetition recognizer with count="<<mCount<<" min="<<mMin<<" max="<<mMax<<endl;
	if (mCount!=-1){
		return Foundation::loop()->setRecognizer(mElement->buildRecognizer(grammar), mCount, mCount);
	}else{
		return Foundation::loop()->setRecognizer(mElement->buildRecognizer(grammar), mMin, mMax);
	}
}

shared_ptr<ABNFConcatenation> ABNFConcatenation::create(){
	return make_shared<ABNFConcatenation>();
}

shared_ptr<Recognizer> ABNFConcatenation::buildRecognizer(const shared_ptr<Grammar> &grammar){
	if (mRepetitions.size()==0){
		bctbx_error("[belr] No repetitions set !");
		abort();
	}
	if (mRepetitions.size()==1){
		return mRepetitions.front()->buildRecognizer(grammar);
	}else{
		auto seq=Foundation::sequence();
		for (auto it=mRepetitions.begin(); it!=mRepetitions.end(); ++it){
			seq->addRecognizer((*it)->buildRecognizer(grammar));
		}
		return seq;
	}
	return NULL;
}

void ABNFConcatenation::addRepetition(const shared_ptr< ABNFRepetition >& r){
	mRepetitions.push_back(r);
}

shared_ptr<ABNFAlternation> ABNFAlternation::create(){
	return make_shared<ABNFAlternation>();
}

void ABNFAlternation::addConcatenation(const shared_ptr<ABNFConcatenation> &c){
	//cout<<"Concatenation "<<c<<" added to alternation "<<this<<endl;
	mConcatenations.push_back(c);
}

shared_ptr<Recognizer> ABNFAlternation::buildRecognizer(const shared_ptr<Grammar> &grammar){
	if (mConcatenations.size()==1) return mConcatenations.front()->buildRecognizer(grammar);
	return buildRecognizerNoOptim(grammar);
}

shared_ptr< Recognizer > ABNFAlternation::buildRecognizerNoOptim(const shared_ptr< Grammar >& grammar){
	auto sel=Foundation::selector();
	for (auto it=mConcatenations.begin(); it!=mConcatenations.end(); ++it){
		sel->addRecognizer((*it)->buildRecognizer(grammar));
	}
	return sel;
}

ABNFRule::ABNFRule() {
	
}

shared_ptr<ABNFRule> ABNFRule::create(){
	return make_shared<ABNFRule>();
}

void ABNFRule::setName(const string& name){
	if (!mName.empty()) bctbx_error("[belr] Rule %s is renamed !!!!!", name.c_str());
	//cout<<"Rule "<<this<<" is named "<<name<<endl;
	mName=name;
}

void ABNFRule::setAlternation(const shared_ptr<ABNFAlternation> &a){
	//cout<<"Rule "<<this<<" is given alternation "<<a<<endl;
	mAlternation=a;
}

bool ABNFRule::isExtension()const{
	return mDefinedAs.find('/')!=string::npos;
}

shared_ptr<Recognizer> ABNFRule::buildRecognizer(const shared_ptr<Grammar> &grammar){
	return mAlternation->buildRecognizer(grammar);
}

void ABNFRule::setDefinedAs(const string& defined_as){
	mDefinedAs=defined_as;
}


shared_ptr<ABNFRuleList> ABNFRuleList::create(){
	//cout<<"Rulelist created."<<endl;
	return make_shared<ABNFRuleList>();
}

void ABNFRuleList::addRule(const shared_ptr<ABNFRule>& rule){
	//cout<<"Rule "<<rule<<" added to rulelist "<<this<<endl;
	mRules.push_back(rule);
}

shared_ptr<Recognizer> ABNFRuleList::buildRecognizer(const shared_ptr<Grammar> &grammar){
	for (auto it=mRules.begin(); it!=mRules.end(); ++it){
		shared_ptr<ABNFRule> rule=(*it);
		if (rule->isExtension()){
			grammar->extendRule(rule->getName(), rule->buildRecognizer(grammar));
		}else{
			grammar->addRule(rule->getName(), rule->buildRecognizer(grammar));
		}
	}
	return NULL;
}

ABNFGrammarBuilder::ABNFGrammarBuilder()
: mParser(make_shared<ABNFGrammar>()){
	mParser.setHandler("rulelist", make_fn(&ABNFRuleList::create))
		->setCollector("rule", make_sfn(&ABNFRuleList::addRule));
	mParser.setHandler("rule", make_fn(&ABNFRule::create))
		->setCollector("rulename",make_sfn(&ABNFRule::setName))
		->setCollector("defined-as",make_sfn(&ABNFRule::setDefinedAs))
		->setCollector("alternation",make_sfn(&ABNFRule::setAlternation));
	mParser.setHandler("alternation", make_fn(&ABNFAlternation::create))
		->setCollector("concatenation",make_sfn(&ABNFAlternation::addConcatenation));
	mParser.setHandler("concatenation", make_fn(&ABNFConcatenation::create))
		->setCollector("repetition", make_sfn(&ABNFConcatenation::addRepetition));
	mParser.setHandler("repetition", make_fn(&ABNFRepetition::create))
		->setCollector("repeat", make_sfn(&ABNFRepetition::setRepeat))
		->setCollector("repeat-min", make_sfn(&ABNFRepetition::setMin))
		->setCollector("repeat-max", make_sfn(&ABNFRepetition::setMax))
		->setCollector("repeat-count", make_sfn(&ABNFRepetition::setCount))
		->setCollector("element", make_sfn(&ABNFRepetition::setElement));
	mParser.setHandler("element", make_fn(&ABNFElement::create))
		->setCollector("rulename", make_sfn(&ABNFElement::setRulename))
		->setCollector("group", make_sfn(&ABNFElement::setElement))
		->setCollector("option", make_sfn(&ABNFElement::setElement))
		->setCollector("char-val", make_sfn(&ABNFElement::setCharVal))
		->setCollector("num-val", make_sfn(&ABNFElement::setElement))
		->setCollector("prose-val", make_sfn(&ABNFElement::setElement));
	mParser.setHandler("group", make_fn(&ABNFGroup::create))
		->setCollector("alternation", make_sfn(&ABNFGroup::setAlternation));
	mParser.setHandler("option", make_fn(&ABNFOption::create))
		->setCollector("alternation", make_sfn(&ABNFOption::setAlternation));
	mParser.setHandler("num-val", make_fn(&ABNFNumval::create))
		->setCollector("bin-val", make_sfn(&ABNFNumval::setBinVal))
		->setCollector("hex-val", make_sfn(&ABNFNumval::setHexVal))
		->setCollector("dec-val", make_sfn(&ABNFNumval::setDecVal));
}

shared_ptr<Grammar> ABNFGrammarBuilder::createFromAbnf(const string &abnf, const shared_ptr<Grammar> &gram){
	size_t parsed;
	
	shared_ptr<ABNFBuilder> builder = mParser.parseInput("rulelist",abnf,&parsed);
	if (parsed<(size_t)abnf.size()){
		bctbx_error("[belr] Only %llu bytes parsed over a total of %llu.", (unsigned long long)parsed, (unsigned long long) abnf.size());
		return NULL;
	}
	shared_ptr<Grammar> retGram;
	if (gram==NULL) retGram=make_shared<Grammar>(abnf);
	else retGram=gram;
	builder->buildRecognizer(retGram);
	bctbx_message("[belr] Succesfully created grammar with %i rules.", retGram->getNumRules());
	if (retGram->isComplete()){
		bctbx_message("[belr] Grammar is complete.");
		retGram->optimize();
		bctbx_message("[belr] Grammar has been optimized.");
	}else{
		bctbx_warning("[belr] Grammar is not complete.");
	}
	return gram;
}

shared_ptr<Grammar> ABNFGrammarBuilder::createFromAbnfFile(const string &path, const shared_ptr<Grammar> &gram){
	ifstream istr(path);
	if (!istr.is_open()){
		bctbx_error("[belr] Could not open %s", path.c_str());
		return NULL;
	}
	stringstream sstr;
	sstr<<istr.rdbuf();
	return createFromAbnf(sstr.str(), gram);
}

}//end of namespace
