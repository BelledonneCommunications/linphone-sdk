


#include "abnf.hh"
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
	shared_ptr<Recognizer> parser;
	
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
	ABNFGrammar abnf_grammar;
	if (!abnf_grammar.isComplete()){
		cerr<<"ABNF Grammar not complete, aborting."<<endl;
		return -1;
	}
	parser=abnf_grammar.getRule("rulelist");
	cout<<"Finished ABNF recognizer construction, starting parsing"<<endl;
	string sgrammar(grammar);
	parser->feed(sgrammar,0);
	cout<<"parsing done"<<endl;
	delete []grammar;
	return 0;
};
