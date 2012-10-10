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

