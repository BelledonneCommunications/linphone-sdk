
#ifndef string8_h
#define string8_h

#include "loader.h"


namespace fake_android{

class String8Impl{
public:
	static bool init(Library *lib);
	static String8Impl *get(){
		return sImpl;
	}
	Function2<void,void*,const char*> mCtor;
	Function1<void,void*> mDtor;
	Function0<void> mInitialize;
private:
	String8Impl(Library *lib);
	static String8Impl *sImpl;
};


class String8{
public:
	explicit String8(const char *str="");
	~String8();
	const char *string()const{
		return ((const char**)mThis)[0];
	}
private:
	uint8_t mThis[64]; //This must be the first field of the class because the real android object will play in this area.
	String8Impl *mImpl;
	
};

}//end of namespace

#endif
