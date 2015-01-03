


#include "abnf.hh"
#include "parser.hh"
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


using namespace::belr;

int main(int argc, char *argv[]){
	const char *grammarfile;
	int fd;
	struct stat sb;
	char *grammar;
	shared_ptr<Parser> parser;
	
	if (argc<2){
		cerr<<argv[0]<< "grammarfile-to-load"<<endl;
		return -1;
	}
	grammarfile=argv[1];
	if (stat(grammarfile,&sb)==-1){
		cerr<<"Could not stat "<<grammarfile<<endl;
		return -1;
	}
	fd=open(grammarfile,O_RDONLY);
	grammar=new char[sb.st_size+1];
	grammar[sb.st_size]='\0';
	if (read(fd,grammar,sb.st_size)!=sb.st_size){
		cerr<<"Could not read "<<grammarfile<<endl;
		return -1;
	}
	cout<<"Building ABNF recognizer"<<endl;
	shared_ptr<ABNFGrammar> abnf_grammar=make_shared<ABNFGrammar>();
	if (!abnf_grammar->isComplete()){
		cerr<<"ABNF Grammar not complete, aborting."<<endl;
		return -1;
	}
	parser=make_shared<Parser>(abnf_grammar);
	cout<<"Finished ABNF recognizer construction, starting parsing"<<endl;
	string sgrammar(grammar);
	size_t parsed;
	parser->parseInput("rulelist",sgrammar,&parsed);
	cout<<"parsing done"<<endl;
	delete []grammar;
	return 0;
};
