
#include "abnf.hh"
#include "grammarbuilder.hh"

#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


namespace belr{

	
ABNFGrammarBuilder::ABNFGrammarBuilder()
: mParser(make_shared<ABNFGrammar>()){
	mParser.setHandler("rulelist", bind(mem_fn(&ABNFGrammarBuilder::createRuleList),this))
		->setCollector<void*>("rule",bind(mem_fn(&ABNFGrammarBuilder::addRule),this, placeholders::_1, placeholders::_2));
	mParser.setHandler("rule", bind(mem_fn(&ABNFGrammarBuilder::createRule),this));
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

void *ABNFGrammarBuilder::createRule(){
	cout<<"Rule created"<<endl;
	return (void*)0x1;
}

void *ABNFGrammarBuilder::createRuleList(){
	cout<<"RuleList created"<<endl;
	return (void*)0x2;
}

void ABNFGrammarBuilder::addRule(void *list, void *rule){
	cout<<"Rule "<<rule<<" added to rule list "<<list<<endl;
}

}//end of namespace
