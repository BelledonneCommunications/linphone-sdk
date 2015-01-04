


#include "grammarbuilder.hh"
#include <iostream>



using namespace::belr;

int main(int argc, char *argv[]){
	
	if (argc<2){
		cerr<<argv[0]<< " <grammarfile-to-load>"<<endl;
		return -1;
	}
	ABNFGrammarBuilder builder;
	builder.createFromAbnf(argv[1]);
	return 0;
};
