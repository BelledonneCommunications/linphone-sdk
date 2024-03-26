/*
 * Copyright (c) 2012-2019 Belledonne Communications SARL.
 *
 * This file is part of belle-sip.
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

#include "belle-sip/object++.hh"
#include "belle_sip_internal.h"

#include <mutex>
#include <unordered_set>

#ifdef __GNUC__
#include <cxxabi.h>
#endif

namespace bellesip {

/*
 * Leak detector structure. The initialization and deinitialisation are not thread-safe.
 * Test programs must ensure that there is only one thread at first use
 * and one thread at last use (when calling belle_sip_object_flush_active_objects())
 */
class ObjectLeakDetector {
public:
	static ObjectLeakDetector &get() {
		if (!sInstance) sInstance.reset(new ObjectLeakDetector());
		return *sInstance;
	}
	void add(belle_sip_object_t *obj) {
		std::lock_guard<std::recursive_mutex> lk(mMutex);
		mObjects.insert(obj);
	}
	void remove(belle_sip_object_t *obj) {
		std::lock_guard<std::recursive_mutex> lk(mMutex);
		mObjects.erase(obj);
	}
	size_t count() {
		std::lock_guard<std::recursive_mutex> lk(mMutex);
		return mObjects.size();
	}
	void flushActiveObjects() {
		// do not free objects so that they are still detected as leaked by valgrind and such
		mMutex.lock();
		mObjects.clear();
		mMutex.unlock();
		sInstance.reset();
	}
	void dumpActiveObjects() {
		std::lock_guard<std::recursive_mutex> lk(mMutex);
		belle_sip_warning("List of leaked objects:");
		for (auto &obj : mObjects) {
			char *content = belle_sip_object_to_string(obj);
			belle_sip_warning("%s(%p) ref=%i, content [%10s...]", belle_sip_object_vptr_get_type_name(obj), obj,
			                  obj->ref, content);
			belle_sip_free(content);
		}
	}

private:
	std::recursive_mutex mMutex;
	std::unordered_set<belle_sip_object_t *> mObjects;
	static std::unique_ptr<ObjectLeakDetector> sInstance;
};

std::unique_ptr<ObjectLeakDetector> ObjectLeakDetector::sInstance;

class ObjectCAccessors {
public:
	static belle_sip_error_code sMarshal(belle_sip_object_t *obj, char *buff, size_t buff_size, size_t *offset) {
		return Object::getCppObject(obj)->marshal(buff, buff_size, offset);
	}
	static void doDelete(belle_sip_object_t *obj) {
		delete Object::getCppObject(obj);
	}
	static const void *getCppAddress(const belle_sip_object_t *obj) {
		return Object::getCppObject(obj);
	}
	static const char *getTypeName(const belle_sip_object_t *obj) {
		static thread_local std::string readableTypeName;
		const Object *cppObject = Object::getCppObject(obj);
#ifdef __GNUC__
		int status = 0;
		char *tmp = abi::__cxa_demangle(typeid(*cppObject).name(), 0, 0, &status);
		if (status != 0) {
#endif
			readableTypeName = typeid(*cppObject).name();
#ifdef __GNUC__
		} else {
			readableTypeName = tmp;
			free(tmp);
		}
#endif
		return readableTypeName.c_str();
	}
};

void Object::init() {
	static belle_sip_object_vptr_t *vptr = nullptr;
	if (!vptr) {
		vptr = belle_sip_cpp_object_t_vptr_get();
		vptr->cpp_offset = (int)((intptr_t)&mObject - (intptr_t)this);
	}
	_belle_sip_object_init(&mObject, vptr);
}

Object::Object() {
	init();
}

Object::Object(const Object &other) {
	init();
	mObject.vptr->get_parent()->clone(&mObject, &other.mObject); /*belle_sip_object_t own's clone method*/
}

Object::Object(Object &&other) {
	init();
	mObject.name = other.mObject.name;
	mObject.data_store = other.mObject.data_store;
	other.mObject.name = nullptr;
	other.mObject.data_store = nullptr;
	/* TODO: Warn if weak_refs are non null. It doesn't make sense to move an object that has weak_refs */
}

Object::~Object() {
#if 0
	if (mObject.ref != -1){
		/*note: throwing an exception here does not work*/
		belle_sip_fatal("bellesip::Object [%p] has been destroyed directly with delete operator. This is prohibited, "
		                "use unref() instead.",
		                this);
	}
#endif
	belle_sip_object_uninit(&mObject);
	belle_sip_debug("Object destroyed [%p]", &mObject);
}

Object *Object::ref() {
	belle_sip_object_ref(&mObject);
	return this;
}

const Object *Object::ref() const {
	belle_sip_object_ref(&mObject);
	return this;
}

void Object::unref() {
	belle_sip_object_unref(&mObject);
}

void Object::constUnref() const {
	belle_sip_object_unref(&mObject);
}

std::string Object::toString() const {
	return std::string();
}

belle_sip_error_code Object::marshal(char *buff, size_t buff_size, size_t *offset) {
	std::string tmp = toString();
	if (tmp.size() >= buff_size) {
		return BELLE_SIP_BUFFER_OVERFLOW;
	}
	strncpy(buff, tmp.c_str(), buff_size);
	*offset += tmp.size();
	return BELLE_SIP_OK;
}

Object *Object::clone() const {
	return new Object(*this);
}

belle_sip_object_t *Object::getCObject() {
	return &mObject;
}

const belle_sip_object_t *Object::getCObject() const {
	return &mObject;
}

Object *Object::getCppObject(void *ptr) {
	belle_sip_cpp_object_t *obj = BELLE_SIP_CAST(ptr, belle_sip_cpp_object_t);
	intptr_t cppaddr = (intptr_t)obj - (intptr_t)obj->vptr->cpp_offset;
	return reinterpret_cast<Object *>(cppaddr);
}

const Object *Object::getCppObject(const void *ptr) {
	return Object::getCppObject((void *)ptr);
}

} // namespace bellesip

void belle_sip_cpp_object_delete(belle_sip_object_t *obj) {
	bellesip::ObjectCAccessors::doDelete(obj);
}

const char *belle_sip_cpp_object_get_type_name(const belle_sip_object_t *obj) {
	return bellesip::ObjectCAccessors::getTypeName(obj);
}

const void *belle_sip_cpp_object_get_address(const belle_sip_object_t *obj) {
	return bellesip::ObjectCAccessors::getCppAddress(obj);
}

void belle_sip_object_add_to_leak_detector(belle_sip_object_t *obj) {
	bellesip::ObjectLeakDetector::get().add(obj);
}

void belle_sip_object_remove_from_leak_detector(belle_sip_object_t *obj) {
	bellesip::ObjectLeakDetector::get().remove(obj);
}

int belle_sip_object_get_object_count(void) {
	return (int)bellesip::ObjectLeakDetector::get().count();
}

void belle_sip_object_flush_active_objects(void) {
	bellesip::ObjectLeakDetector::get().flushActiveObjects();
}

void belle_sip_object_dump_active_objects(void) {
	bellesip::ObjectLeakDetector::get().dumpActiveObjects();
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_cpp_object_t);
BELLE_SIP_INSTANCIATE_VPTR3(belle_sip_cpp_object_t,
                            belle_sip_object_t,
                            NULL,
                            NULL,
                            bellesip::ObjectCAccessors::sMarshal,
                            NULL,
                            NULL,
                            FALSE,
                            TRUE);
