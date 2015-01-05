
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

ABNFRule *ABNFRule::create(){
	cout<<"Rule created."<<endl;
	return new ABNFRule();
}
shared_ptr<Recognizer> ABNFRule::buildRecognizer(){
	return NULL;
}

ABNFRuleList *ABNFRuleList::create(){
	cout<<"Rulelist created."<<endl;
	return new ABNFRuleList();
}

void ABNFRuleList::addRule(ABNFRule *rule){
	cout<<"Rule "<<rule<<" added to rulelist "<<this<<endl;
}

shared_ptr<Recognizer> ABNFRuleList::buildRecognizer(){
	return NULL;
}

ABNFGrammarBuilder::ABNFGrammarBuilder()
: mParser(make_shared<ABNFGrammar>()){
	mParser.setHandler("rulelist", make_ptrfn(&ABNFRuleList::create))
		->setCollector("rule", make_memfn(&ABNFRuleList::addRule));
	mParser.setHandler("rule", make_ptrfn(&ABNFRule::create));
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
