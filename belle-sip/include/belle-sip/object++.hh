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

namespace bellesip {

class ObjectCAccessors;

class BELLESIP_EXPORT Object{
	friend ObjectCAccessors;
	public:
		Object();
		Object *ref();
		const Object *ref() const;
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
		void constUnref()const;
	private:
		void init();
		mutable belle_sip_cpp_object_t mObject;
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
 *
 * VERY IMPORTANT USAGE RULES:
 * - The destructor MUST be kept protected so that no one can call delete operator on the object. Instead unref() must be used.
 * - make_shared<>() or shared_ptr<>(new ...) MUST NOT be used to instanciate an HybridObject, use create() instead.
 *
 * A shared_ptr<> can be obtained at any time from an HybridObject using getSharedFromThis().
 * The static getSharedFromThis(_Ctype*) method can be used to directly obtain a shared_ptr from the C object.
 *
 * The clone() method must be overriden to return a new _CppType contructed with copy contructor,
 * or return nullptr if the object is defined to be not clonable.
 *
 * Rational for using this template:
 * - You have an existing library in C where all C objects are inheriting from belle_sip_object_t (for refcounting, data_set etc...).
 * - You want to use C++ in your library without making any disruption in the API.
 * IMPORTANT:
 * If you don't care about belle_sip_object_t inheritance in your C api,
 * or if you don't need any C api associated to this object at all, DON'T USE THIS.
 * An usage example is shown in tester/object_tester.cc .
**/
template <typename _CType, typename _CppType>
class HybridObject : public Object {
	public:
		//Create the C++ object returned as a shared_ptr. Reference counting is managed by shared_ptr automatically.
		template <typename... _Args>
		static inline std::shared_ptr<_CppType> create(_Args&&... __args) {
			return (new _CppType(std::forward<_Args>(__args)...))->toSharedPtr();
		}
		//Convenience creator to get the C object object instead. Automatically acquires a ref. Consumers have the responsibility to unref
		template <typename... _Args>
		static inline _CType *createCObject(_Args&&... __args) {
			return (new _CppType(std::forward<_Args>(__args)...))->toC();
		}
		//Obtain the C object from this.
		_CType *toC(){
			return static_cast<_CType*>(getCPtr());
		}
		//Obtain the C object this, when it is const.
		const _CType *toC()const{
			return static_cast<const _CType*>(getCPtr());
		}
		//Obtain the C++ object as a normal pointer.
		static _CppType *toCpp(_CType *ptr){
			return static_cast<_CppType *>(getCppObject(ptr));
		}
		//Obtain the C++ object as a normal pointer, when the c++ object is const.
		static const _CppType *toCpp(const _CType *ptr){
			return static_cast<const _CppType *>(getCppObject(ptr));
		}
		//Obtain a shared_ptr from the C++ object.
		std::shared_ptr<_CppType> getSharedFromThis() {
			return sharedFromThis(false);
		}
		//Obtain a shared_ptr from the C++ object in the const case.
		std::shared_ptr<const _CppType> getSharedFromThis () const {
			return sharedFromThis(false);
		}
		//Convenience method for easy CType -> shared_ptr<CppType> conversion
		static std::shared_ptr<_CppType> getSharedFromThis(_CType *ptr) {
			return toCpp(ptr)->getSharedFromThis();
		}
		//Convenience method for easy CType -> shared_ptr<CppType> conversion
		static std::shared_ptr<const _CppType> getSharedFromThis(const _CType *ptr) {
			return toCpp(const_cast<_CType *>(ptr))->getSharedFromThis();
		}
		// Obtain a shared_ptr that takes ownership of the object.
		// Use this method with caution as it will transfer the ownership of the object to the shared_ptr, unlike getSharedFromThis() that
		// gives you a new reference to the object.
		// This method should be only useful to transfer into a shared_ptr an object that was created as a normal pointer, for example
		// by clone() or createCObject() methods.
		// There should be NO public const variant of this method.
		std::shared_ptr<_CppType> toSharedPtr(){
			return sharedFromThis(true);
		}

		//Convenience method for easy bctbx_list(_Ctype) -> std::list<_CppType> conversion
		//It does not take ownership of the hybrid object, but takes a ref.
		static std::list<std::shared_ptr<_CppType>> getCppListFromCList(const bctbx_list_t *cList) {
			std::list<std::shared_ptr<_CppType>> result;
			for (auto it = cList; it != nullptr; it = bctbx_list_next(it))
				result.push_back(toCpp(static_cast<_CType*>(bctbx_list_get_data(it)))->getSharedFromThis() );
			return result;
		}
		//Convenience method for easy bctbx_list(_Ctype) -> std::list<_CppType> conversion
		//Applies 'func' to get _CppType from _CType. Used in case we do not want to call  `toCpp` on _Ctype
		static std::list<std::shared_ptr<_CppType>> getCppListFromCList(const bctbx_list_t *cList, const std::function<std::shared_ptr<_CppType> (_CType *)> &func) {
			std::list<std::shared_ptr<_CppType>> result;
			for (auto it = cList; it != nullptr; it = bctbx_list_next(it))
			 	result.push_back(func(static_cast<_CType*>(bctbx_list_get_data(it))));
			return result;
		}
		//Convenience method for easy std::list<shared_ptr<CppType>> -> bctbx_list(CType) conversion
		//It does not take ownership of the hybrid object, but takes a ref.
		static bctbx_list_t* getCListFromCppList(const std::list<std::shared_ptr<_CppType> > &cppList) {
			bctbx_list_t *result = nullptr;
			for (auto it = cppList.begin(); it != cppList.end(); it++) {
				std::shared_ptr<_CppType> cppPtr = static_cast<std::shared_ptr<_CppType>>(*it);
				cppPtr->ref();
				_CType *cptr = cppPtr->toC();
				result = bctbx_list_append(result, cptr);
			}
			return result;
		}
		//Convenience method for easy std::list<CppType*> -> bctbx_list(CType) conversion
		//It does not take ownership of the hybrid object, but takes a ref.
		static bctbx_list_t* getCListFromCppList(const std::list<_CppType*> &cppList) {
			bctbx_list_t *result = nullptr;
			for (auto it = cppList.begin(); it != cppList.end(); it++) {
				_CppType *cppPtr = static_cast<_CppType*>(*it);
				cppPtr->ref();
				_CType *cptr = cppPtr->toC();
				result = bctbx_list_append(result, cptr);
			}
			return result;
		}

		static const char * nullifyEmptyString(const std::string& cppString){
			if (cppString.empty()) return nullptr;
			else return cppString.c_str();
		}

	protected:
		virtual ~HybridObject() = default;
		HybridObject() {}
		HybridObject(const HybridObject<_CType, _CppType> &other) : Object(other) {}
	private:
		std::shared_ptr<_CppType> sharedFromThis(bool withTransfer)const{
			std::shared_ptr<_CppType> sp;
			if ((sp = mSelf.lock()) == nullptr){
				sp = std::shared_ptr<_CppType>(static_cast<_CppType *>(const_cast<HybridObject<_CType, _CppType> *>(this)),
								  std::mem_fn(&HybridObject<_CType, _CppType>::constUnref));
				mSelf = sp;
				if (!withTransfer){
					sp->ref();
				}
			}else{
				if (withTransfer){
					belle_sip_fatal("This HybridObject already has shared_ptr<> instances pointing to it.");
				}
			}
			return sp;
		}
		mutable std::weak_ptr<_CppType> mSelf;
};

}//end of namespace

extern "C" {
	BELLE_SIP_DECLARE_VPTR(belle_sip_cpp_object_t);
}



#endif //belle_sip_object_plusplus_h
