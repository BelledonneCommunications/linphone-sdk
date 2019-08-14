/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

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
