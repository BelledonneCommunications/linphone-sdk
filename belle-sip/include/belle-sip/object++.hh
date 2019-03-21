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

#ifndef belle_sip_object_plusplus_h
#define belle_sip_object_plusplus_h

#include "belle-sip/types.h"
#include "belle-sip/object.h"
#include "belle-sip/utils.h"

#include <memory>
#include <list>
#include <functional>

#ifdef _WIN32
    // Disable C4251 triggered by the std::enabled_shared_from_this inheritance
    #pragma warning(push)
    #pragma warning(disable: 4251)
#endif // ifdef _WIN32

namespace bellesip {

class ObjectCAccessors;

class BELLESIP_EXPORT Object {
	friend ObjectCAccessors;
	public:
		Object();
		Object *ref();
		void unref();
		//Overrides should keep	the size of toString() lower than BELLE_SIP_MAX_TO_STRING_SIZE
	        virtual std::string toString() const;
		virtual Object *clone()const;
		belle_sip_cpp_object_t *getCObject();
		const belle_sip_cpp_object_t *getCObject()const;
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
		belle_sip_cpp_object_t mObject;
		belle_sip_error_code marshal(char* buff, size_t buff_size, size_t *offset);
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
class HybridObject : public Object, public std::enable_shared_from_this<HybridObject<_CType, _CppType> > {
	public:
		//Ref is managed by shared_ptr, unref will be called on last ref.
		template <typename... _Args>
		static inline std::shared_ptr<_CppType> create(_Args&&... __args) {
			return std::shared_ptr<_CppType>(new _CppType(std::forward<_Args>(__args)...), std::mem_fun(&Object::unref));
		}
		//Convenience creator to get a C object. Automatically aquires a ref. Consumers have the responsibility to unref
		template <typename... _Args>
		static inline _CType *createCObject(_Args&&... __args) {
			_CppType *obj = new _CppType(std::forward<_Args>(__args)...);
			obj->ref();
			return obj->toC();
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

		std::shared_ptr<const _CppType> getSharedFromThis() const {
			try {
				return std::dynamic_pointer_cast<const _CppType>(this->shared_from_this());
			} catch (const std::exception &up) {
				belle_sip_error("getSharedFromThis() exception: Object not created with 'make_shared'. Error: [%s].", up.what());
				return nullptr;
			}
			return nullptr;
		}
		std::shared_ptr<_CppType> getSharedFromThis () {
			return std::const_pointer_cast<_CppType>(static_cast<const _CppType *>(this)->getSharedFromThis());
		}

		std::shared_ptr<_CppType> toSharedPtr() {
			return std::shared_ptr<_CppType>(static_cast<_CppType *>(this), std::mem_fun(&Object::unref));
		}

		std::shared_ptr<const _CppType> toSharedPtr() const {
			return std::shared_ptr<const _CppType>(static_cast<const _CppType *>(this), std::mem_fun(&Object::unref));
		}
		//Convenience method for easy CType -> shared_ptr<CppType> conversion
		static std::shared_ptr<_CppType> toSharedPtr(const _CType *ptr) {
			return toCpp(const_cast<_CType *>(ptr))->toSharedPtr();
		}
		//Convenience method for easy bctbx_list(_Ctype) -> std::list<_CppType> conversion
		static std::list<_CppType> getCppListFromCList(const bctbx_list_t *cList) {
			std::list<_CppType> result;
			for (auto it = cList; it; it = bctbx_list_next(it))
				result.push_back(toCpp(static_cast<_CType>(bctbx_list_get_data(it))));
			return result;
		}
		//Convenience method for easy bctbx_list(_Ctype) -> std::list<_CppType> conversion
		//Applies 'func' to get _CppType from _CType. Used in case we do not want to call  `toCpp` on _Ctype
		static std::list<_CppType> getCppListFromCList(const bctbx_list_t *cList, const std::function<_CppType (_CType)> &func) {
			std::list<_CppType> result;
			for (auto it = cList; it; it = bctbx_list_next(it))
			 	result.push_back(func(static_cast<_CType>(bctbx_list_get_data(it))));
			return result;
		}

	protected:
		virtual ~HybridObject() {}
		HybridObject() {}
		HybridObject(const HybridObject<_CType, _CppType> &other) : Object(other), std::enable_shared_from_this<HybridObject<_CType, _CppType>> (other) {}
};


/**
 * Convenience function to create a std::shared_ptr that calls Object::unref() instead of delete expression.
 */
template <typename _T, typename... _Args>
inline std::shared_ptr<_T> make_shared(_Args&&... __args) {
	return std::shared_ptr<_T>(new _T(std::forward<_Args>(__args)...), std::mem_fun(&Object::unref));
}


}//end of namespace

extern "C" {
	BELLE_SIP_DECLARE_VPTR(belle_sip_cpp_object_t);
}

#ifdef _WIN32
    #pragma warning(pop)
#endif // ifdef _WIN32

#endif //belle_sip_object_plusplus_h
