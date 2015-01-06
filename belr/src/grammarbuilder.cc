
#include "abnf.hh"
#include "grammarbuilder.hh"
#include "parser-impl.cc"

#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


namespace belr{

ABNFBuilder::~ABNFBuilder(){
}

shared_ptr<ABNFConcatenation> ABNFConcatenation::create(){
	return make_shared<ABNFConcatenation>();
}

shared_ptr<Recognizer> ABNFConcatenation::buildRecognizer(const shared_ptr<Grammar> &grammar){
	return NULL;
}

shared_ptr<ABNFAlternation> ABNFAlternation::create(){
	return make_shared<ABNFAlternation>();
}

void ABNFAlternation::addConcatenation(const shared_ptr<ABNFConcatenation> &c){
	cout<<"Concatenation "<<c<<"added to alternation "<<this<<endl;
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


shared_ptr<ABNFRule> ABNFRule::create(){
	cout<<"Rule created."<<endl;
	return make_shared<ABNFRule>();
}

void ABNFRule::setName(const string& name){
	if (!mName.empty())
		cerr<<"Rule "<<this<<" is renamed !!!!!"<<endl;
	cout<<"Rule "<<this<<" is named "<<name<<endl;
	mName=name;
}

void ABNFRule::setAlternation(const shared_ptr<ABNFAlternation> &a){
	cout<<"Rule "<<this<<" is given alternation "<<a<<endl;
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
	cout<<"Rulelist created."<<endl;
	return make_shared<ABNFRuleList>();
}

void ABNFRuleList::addRule(const shared_ptr<ABNFRule>& rule){
	cout<<"Rule "<<rule<<" added to rulelist "<<this<<endl;
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
}

shared_ptr<Grammar> ABNFGrammarBuilder::createFromAbnf(const string &path){
	struct stat sb;
	char *grammar;
	size_t parsed;
	
	if (stat(path.c_str(),&sb)==-1){
		cerr<<"Could not stat "<<path<<endl;
		return NULL;
	}
	int fd=open(path.c_str(),O_RDONLY);
	grammar=new char[sb.st_size+1];
	grammar[sb.st_size]='\0';
	if (read(fd,grammar,sb.st_size)!=sb.st_size){
		cerr<<"Could not read "<<path<<endl;
		close(fd);
		return NULL;
	}
	string sgrammar(grammar);
	delete []grammar;
	mParser.parseInput("rulelist",sgrammar,&parsed);
	if (parsed<(size_t)sb.st_size){
		cerr<<"Only "<<parsed<<" bytes parsed over a total of "<< sb.st_size <<endl;
	}
	return NULL;
}

}//end of namespace
