/*
	belle-sip - SIP (RFC3261) library.
	Copyright (C) 2019  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "belle-sip/object.h"

#include <memory>
#include <functional>

namespace bellesip{

class ObjectCAccessors;

class BELLESIP_EXPORT Object{
	friend ObjectCAccessors;
	public:
		Object();
		Object *ref();
		void unref();
		virtual belle_sip_error_code marshal(char* buff, size_t buff_size, size_t *offset);
		std::string toString()const{
			std::string ret(belle_sip_object_to_string(&mObject));
			return ret;
		}
		virtual Object *clone()const;
		belle_sip_object_t *getCObject();
		const belle_sip_object_t *getCObject()const;
		void *getCPtr(){
			return static_cast<void*>(getCObject());
		}
		const void *getCPtr()const{
			return static_cast<const void*>(getCObject());
		}
		static Object *getCppObject(void *);
		static const Object *getCppObject(const void *);

	protected:
		virtual ~Object(); /*the destructor must be kept protected, never public, including for all classes inherting from this*/
		Object(const Object &other);
	private:
		void init();
		belle_sip_object_t mObject;
		static belle_sip_object_t *sClone(belle_sip_object_t *);
		static belle_sip_error_code sMarshal(belle_sip_object_t* obj, char* buff, size_t buff_size, size_t *offset);
};

/**
 * Template class to help define an Object usable in both C and C++
 * The template arguments are:
 * - _CType : the type used to represent this object in C
 * - _CppType : the type used in C++ to implement this object. _CppType is used to be set to the type 
 *   of the class inheriting from HybridObject.
 * Example:
 * typedef struct _CExample CExample;
 * class Example : public HybridObject<CExample, Example>{
 * ...
 * }
 * The C object can be obtained with toC() method, directly casted in the expected type.
 * The C++ object can be obtained from C object with static method toCpp().
 * The destructor must be kept protected so that no one can call delete operator on the object. Instead unref() must be used.
 *
 * Rational for using this template:
 * - You have an existing library in C where all C objects are inheriting from belle_sip_object_t (for refcounting, data_set etc...).
 * - You want to use C++ in your library without making any disruption in the API.
 * If you don't care about belle_sip_object_t inheritance in your C api, don't use this.
 * An usage example is shown in tester/object_tester.cc .
**/
template <typename _CType, typename _CppType>
class BELLESIP_EXPORT HybridObject : public Object{
	public:
		HybridObject(){
		}
		_CType *toC(){
			return static_cast<_CType*>(getCPtr());
		}
		const _CType *toC()const{
			return static_cast<const _CType*>(getCPtr());
		}
		static _CppType *toCpp(_CType *ptr){
			return static_cast<_CppType *>(getCppObject(ptr));
		}
		static const _CppType *toCpp(const _CType *ptr){
			return static_cast<const _CppType *>(getCppObject(ptr));
		}
	protected:
		virtual ~HybridObject() = default;
		HybridObject(const HybridObject<_CType, _CppType> &other) : Object(other){
		}
};

/**
 * Convenience function to create a std::shared_ptr that calls Object::unref() instead of delete expression.
 */
template <typename _T, typename... _Args>
BELLESIP_EXPORT std::shared_ptr<_T> make_shared(_Args&&... __args){
	return std::shared_ptr<_T>(new _T(std::forward<_Args>(__args)...), std::mem_fun(&Object::unref));
}

}//end of namespace

extern "C" {
BELLE_SIP_DECLARE_VPTR(belle_sip_cpp_object_t);
}

