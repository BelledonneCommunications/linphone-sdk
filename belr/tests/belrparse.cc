


#include "grammarbuilder.hh"
#include "abnf.hh"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>

using namespace::belr;

int main(int argc, char *argv[]){
	
	if (argc<2){
		cerr<<argv[0]<< " <grammarfile-to-load> [<input file>] [rule1] [rule2]..."<<endl;
		return -1;
	}
	ABNFGrammarBuilder builder;
	shared_ptr<Grammar> grammar=make_shared<Grammar>(argv[1]);
	grammar->include(make_shared<CoreRules>());
	builder.createFromAbnf(argv[1],grammar);
	
	if (argc>3){
		ifstream istr(argv[2]);
		if (!istr.is_open()){
			cerr<<"Could not open "<<argv[2]<<endl;
			return -1;
		}
		stringstream str;
		str<<istr.rdbuf();
		DebugParser parser(grammar);
		list<string> rules;
		for(int i=3; i<argc; ++i){
			rules.push_back(argv[i]);
		}
		parser.setObservedRules(rules);
		size_t parsed;
		auto t_start = std::chrono::high_resolution_clock::now();
		auto ret=parser.parseInput(argv[3],str.str(),&parsed);
		auto t_end = std::chrono::high_resolution_clock::now();
		if (parsed<str.str().size()){
			cerr<<"Parsing ended prematuraly at pos "<<parsed<<endl;
		}else{
			cout<<"Parsing done in "<<std::chrono::duration<double, std::milli>(t_end-t_start).count()<<" milliseconds"<<endl;
		}
		if (ret){
			ret->tostream(0,cout);
		}else{
			cerr<<"Parsing failed."<<endl;
			return -1;
		}
	}
	return 0;
};
