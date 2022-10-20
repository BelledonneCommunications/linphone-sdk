/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "String8.h"

namespace fake_android{

String8Impl::String8Impl(Library *lib) : 
	mCtor(lib,"_ZN7android7String8C1EPKc"),
	mDtor(lib,"_ZN7android7String8D1Ev"),
	mInitialize(lib,"_ZN7android18initialize_string8Ev"){
}

bool String8Impl::init(Library *lib){
	String8Impl *impl=new String8Impl(lib);
	if (!impl->mCtor.isFound()) goto end;
	if (!impl->mDtor.isFound()) goto end;
	if (!impl->mInitialize.isFound()) goto end;
	
	impl->mInitialize.invoke();
	sImpl=impl;
	return true;
	
	end:
	delete impl;
	return false;
}

String8Impl * String8Impl::sImpl=0;

String8::String8(const char* cstr){
	mImpl=String8Impl::get();
	mImpl->mCtor.invoke(mThis,cstr);
}

String8::~String8(){
	mImpl->mDtor.invoke(mThis);
}


}//end of namespace

