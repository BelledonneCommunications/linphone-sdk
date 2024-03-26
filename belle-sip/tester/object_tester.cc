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

#include "bctoolbox/exception.hh"
#include "bctoolbox/tester.h"
#include "belle-sip/belle-sip.h"
#include "belle-sip/object++.hh"
#include "belle_sip_tester.h"

using namespace bellesip;

static void on_object_destroyed(void *userpointer, belle_sip_object_t *obj_being_destroyed) {
	int *value = static_cast<int *>(userpointer);
	*value = TRUE;
}

static void basic_test(void) {
	int object_destroyed = FALSE;
	Object *obj = new Object();

	belle_sip_object_t *c_obj = obj->getCObject();
	BC_ASSERT_PTR_NOT_NULL(c_obj);
	if (c_obj) {
		belle_sip_object_weak_ref(c_obj, on_object_destroyed, &object_destroyed);
	}
	/*we put a weak ref to this object in order to know when it is destroyed*/
	obj->ref();
	Object *clone = obj->clone();

	obj->unref();
	BC_ASSERT_FALSE(object_destroyed);
	obj->unref(); /*this unref will destroy the object*/
	BC_ASSERT_TRUE(object_destroyed);
	object_destroyed = false;

	c_obj = clone->getCObject();
	BC_ASSERT_PTR_NOT_NULL(c_obj);
	if (c_obj) {
		belle_sip_object_weak_ref(c_obj, on_object_destroyed, &object_destroyed);
	}
	clone->unref();
	BC_ASSERT_TRUE(object_destroyed);
};

typedef struct _LinphoneEvent LinphoneEvent;

typedef enum _LinphoneEventState { LinphoneEventIdle, LinphoneEventSubscribed } LinphoneEventState;

namespace Linphone {

class Event : public HybridObject<LinphoneEvent, Event> {
public:
	enum State { Idle, Subscribed };
	Event() : mState(Idle) {
	}
	void sendSubscribe(const std::string &dest) {
		mState = Subscribed;
	}
	State getState() const {
		return mState;
	}
	void doSomething() {
		throw BctbxException("Unimplemented");
	}
	void setState(State state) {
		mState = state;
	}
	Event *clone() const {
		return new Event(*this);
	}

	Event(const Event &orig) : HybridObject<LinphoneEvent, Event>(orig) {
		mState = orig.mState;
	}
	Event(Event &&other) : HybridObject<LinphoneEvent, Event>(std::move(other)) {
		mState = other.mState;
	}
	//~Event() = default; //we shouls have the destructor private but in order to test the delete exception
	// we'll make it public.
private:
	State mState;
};

} // namespace Linphone

extern "C" {

using namespace Linphone;

LinphoneEvent *bellesiptest_event_new(void) {
	return (new Event())->toC();
}

void bellesiptest_event_send_subscribe(LinphoneEvent *obj, const char *dest) {
	Event::toCpp(obj)->sendSubscribe(dest);
}

LinphoneEventState bellesiptest_event_get_state(const LinphoneEvent *obj) {
	return (LinphoneEventState)Event::toCpp(obj)->getState(); /*enum conversion should be performed better*/
}

void bellesiptest_event_ref(LinphoneEvent *obj) {
	Event::toCpp(obj)->ref();
}

void bellesiptest_event_unref(LinphoneEvent *obj) {
	Event::toCpp(obj)->unref();
}

} // end of extern "C"

static void dual_object(void) {
	int object_destroyed = 0;
	/*in this test we use the C Api */
	LinphoneEvent *ev = bellesiptest_event_new();
	BC_ASSERT_TRUE(bellesiptest_event_get_state(ev) == LinphoneEventIdle);
	bellesiptest_event_send_subscribe(ev, "sip:1234@sip.bellesiptest.org");
	BC_ASSERT_TRUE(bellesiptest_event_get_state(ev) == LinphoneEventSubscribed);

	belle_sip_object_t *c_obj = Linphone::Event::toCpp(ev)->getCObject();
	BC_ASSERT_PTR_NOT_NULL(c_obj);
	if (c_obj) {
		belle_sip_object_weak_ref(c_obj, on_object_destroyed, &object_destroyed);
	}
	bellesiptest_event_unref(ev);
	BC_ASSERT_TRUE(object_destroyed);
}

static void dual_object_clone(void) {
	int object_destroyed = 0;
	std::shared_ptr<Event> ev = Event::create();
	std::shared_ptr<Event> cloned_ev = ev->clone()->toSharedPtr();

	belle_sip_object_t *c_obj = cloned_ev->getCObject();
	BC_ASSERT_PTR_NOT_NULL(c_obj);
	if (c_obj) {
		belle_sip_object_weak_ref(c_obj, on_object_destroyed, &object_destroyed);
	}
	ev.reset();
	BC_ASSERT_FALSE(object_destroyed); // the reset() shall not cause the cloned object to be destroyed of course.
	cloned_ev.reset();
	BC_ASSERT_TRUE(object_destroyed); // Now the object should be destroyed.
}

static void dual_object_shared_ptr(void) {
	int object_destroyed = 0;
	std::shared_ptr<Event> ev = Event::create();
	belle_sip_object_t *c_obj = ev->getCObject();
	BC_ASSERT_PTR_NOT_NULL(c_obj);
	if (c_obj) {
		belle_sip_object_weak_ref(c_obj, on_object_destroyed, &object_destroyed);
	}
	// Here we mix manual reference from C and shared_ptr.
	belle_sip_object_ref(c_obj);
	ev.reset();
	BC_ASSERT_FALSE(object_destroyed); // the reset() shall not cause the object to be destroyed.
	ev = Event::toCpp((LinphoneEvent *)c_obj)->getSharedFromThis(); // we get again a shared_ptr.
	belle_sip_object_unref(c_obj);
	BC_ASSERT_FALSE(object_destroyed); // the unref() shall not cause the object to be destroyed, since the shared_ptr
	                                   // has a reference to it.
	// Get a manual reference again:
	belle_sip_object_ref(c_obj);
	// Drop the shared_ptr:
	ev.reset();
	BC_ASSERT_FALSE(object_destroyed);
	// Get again a shared_ptr, this will test the re-instanciation of the internal weak_ptr (as
	// std::enable_shared_from_this)
	ev = Event::getSharedFromThis((LinphoneEvent *)c_obj);
	// Drop the C reference:
	belle_sip_object_unref(c_obj);
	BC_ASSERT_FALSE(object_destroyed);

	ev.reset();
	BC_ASSERT_TRUE(object_destroyed); // Now the object should be destroyed.
}

static void dual_object_shared_from_this(void) {
	int object_destroyed = 0;
	std::shared_ptr<Event> ev = Event::create();
	std::shared_ptr<Event> otherptr;
	belle_sip_object_t *c_obj = ev->getCObject();
	BC_ASSERT_PTR_NOT_NULL(c_obj);
	if (c_obj) {
		belle_sip_object_weak_ref(c_obj, on_object_destroyed, &object_destroyed);
	}
	otherptr = ev->getSharedFromThis();
	ev.reset();
	BC_ASSERT_FALSE(object_destroyed);
	otherptr.reset();
	BC_ASSERT_TRUE(object_destroyed);
}

static void dual_object_shared_from_this_from_c(void) {
	int object_destroyed = 0;
	LinphoneEvent *event = Event::createCObject();
	std::shared_ptr<Event> otherptr;
	BC_ASSERT_PTR_NOT_NULL(event);
	if (event) {
		belle_sip_object_weak_ref(event, on_object_destroyed, &object_destroyed);
	}
	otherptr = Event::getSharedFromThis(event);
	otherptr.reset(); // the reset() of the shared_ptr shall not cause the object to be destroyed, because we still have
	                  // the C ref obtained from createCObject().
	BC_ASSERT_FALSE(object_destroyed);
	belle_sip_object_unref(event);
	BC_ASSERT_TRUE(object_destroyed);
}

static void dual_object_in_weak_ptr(void) {
	int object_destroyed = 0;
	std::shared_ptr<Event> ev = Event::create();
	belle_sip_object_t *c_obj = ev->getCObject();
	BC_ASSERT_PTR_NOT_NULL(c_obj);
	if (c_obj) {
		belle_sip_object_weak_ref(c_obj, on_object_destroyed, &object_destroyed);
	}
	// Here we mix manual reference from C and shared_ptr.
	belle_sip_object_ref(c_obj);
	ev.reset();
	BC_ASSERT_FALSE(object_destroyed); // the reset() shall not cause the object to be destroyed.
	ev = Event::toCpp((LinphoneEvent *)c_obj)->getSharedFromThis(); // we get again a shared_ptr.
	std::weak_ptr<Event> ev_weak =
	    Event::toCpp((LinphoneEvent *)c_obj)->getSharedFromThis(); /* get another shared_ptr to make the weak one */
	BC_ASSERT_TRUE(ev_weak.lock() != nullptr);
	belle_sip_object_unref(c_obj);
	BC_ASSERT_FALSE(object_destroyed); // the unref() shall not cause the object to be destroyed, since the shared_ptr
	                                   // has a reference to it.
	ev.reset();
	BC_ASSERT_TRUE(object_destroyed); // Now the object should be destroyed.
	/* the weak_ptr shall be reset too. */
	BC_ASSERT_TRUE(ev_weak.lock() == nullptr);
}

static void dual_object_move(void) {
	Event ev;

	ev.setState(Event::Subscribed);

	std::shared_ptr<Event> evptr = Event::create(std::move(ev));
	BC_ASSERT_TRUE(evptr->getState() == Event::Subscribed);
	evptr.reset();
}

static void main_loop_cpp_do_later(void) {
	int test = 0;
	belle_sip_main_loop_t *ml = belle_sip_main_loop_new();

	belle_sip_main_loop_cpp_do_later(ml, [&test]() { test = 44; });
	BC_ASSERT_TRUE(test == 0);
	belle_sip_main_loop_sleep(ml, 10);
	BC_ASSERT_TRUE(test == 44);
	belle_sip_object_unref(ml);
}

static test_t object_tests[] = {
    TEST_NO_TAG("Basic test", basic_test),
    TEST_NO_TAG("Hybrid C/C++ object", dual_object),
    TEST_NO_TAG("Hybrid C/C++ object clone", dual_object_clone),
    TEST_NO_TAG("Hybrid C/C++ object with shared_ptr", dual_object_shared_ptr),
    TEST_NO_TAG("Hybrid C/C++ object with sharedFromThis()", dual_object_shared_from_this),
    TEST_NO_TAG("Hybrid C/C++ object with sharedFromThis() from C object", dual_object_shared_from_this_from_c),
    TEST_NO_TAG("Hybrid C/C++ object with in a weak_ptr", dual_object_in_weak_ptr),
    TEST_NO_TAG("Hybrid C/C++ object with move", dual_object_move),
    TEST_NO_TAG("Mainloop's do_later in c++", main_loop_cpp_do_later)};

test_suite_t object_test_suite = {"Object",
                                  NULL,
                                  NULL,
                                  belle_sip_tester_before_each,
                                  belle_sip_tester_after_each,
                                  sizeof(object_tests) / sizeof(object_tests[0]),
                                  object_tests,
                                  0,
                                  0};
