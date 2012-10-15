
#include <mediastreamer2/mscommon.h>
#include <dlfcn.h>

#include "loader.h"


Library::Library(void *handle){
	mHandle=handle;
}

void *Library::getSymbol(const char *name){
	void *symbol=dlsym(mHandle,name);
	if (symbol==NULL){
		ms_message("Could not find symbol %s", name);
	}
	return symbol;
}

Library *Library::load(const char *path){
	void *handle=dlopen(path,RTLD_NOW|RTLD_GLOBAL);
	if (handle==NULL){
		ms_error("Could not load %s: %s",path,dlerror());
		return NULL;
	}
	return new Library(handle);
}

FunctionBase::FunctionBase(Library *lib, const char *symbol_name){
	load(lib,symbol_name);
}

bool FunctionBase::load(Library *lib, const char *symbol_name){
	mSymbolAddr=lib->getSymbol(symbol_name);
	return mSymbolAddr!=NULL;
}
