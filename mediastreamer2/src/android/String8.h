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
