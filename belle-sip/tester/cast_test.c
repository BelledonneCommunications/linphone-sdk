/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

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

#include "belle-sip/belle-sip.h"
#include "belle_sip_tester.h"

static void cast_test(void){
	belle_sip_stack_t *stack=belle_sip_stack_new(NULL);
	belle_sip_listening_point_t *lp=belle_sip_stack_create_listening_point(stack,"0.0.0.0",7060,"UDP");
	belle_sip_provider_t *provider;
	belle_sip_request_t *req=belle_sip_request_new();
	belle_sip_response_t *resp=belle_sip_response_new();
	belle_sip_message_t *msg;
	int tmp;

	BC_ASSERT_PTR_NOT_NULL(stack);
	BC_ASSERT_PTR_NOT_NULL(lp);
	provider=belle_sip_stack_create_provider(stack,lp);
	BC_ASSERT_PTR_NOT_NULL(provider);
	BC_ASSERT_PTR_NOT_NULL(req);
	BC_ASSERT_PTR_NOT_NULL(resp);

	belle_sip_message("Casting belle_sip_request_t to belle_sip_message_t");
	msg=BELLE_SIP_MESSAGE(req);
	BC_ASSERT_PTR_NOT_NULL(msg);
	belle_sip_message("Ok.");
	belle_sip_message("Casting belle_sip_response_t to belle_sip_message_t");
	msg=BELLE_SIP_MESSAGE(resp);
	BC_ASSERT_PTR_NOT_NULL(msg);
	belle_sip_message("Ok.");
	tmp=BELLE_SIP_IS_INSTANCE_OF(req,belle_sip_response_t);
	belle_sip_message("Casting belle_sip_request_t to belle_sip_response_t: %s",tmp ? "yes" : "no");
	BC_ASSERT_EQUAL(tmp,0,int,"%d");
	belle_sip_object_unref(req);
	belle_sip_object_unref(resp);
	belle_sip_object_unref(provider);
	belle_sip_object_unref(stack);
}


test_t cast_tests[] = {
	TEST_NO_TAG("Casting requests and responses", cast_test)
};

test_suite_t cast_test_suite = {"Object inheritance", NULL, NULL, NULL, NULL,
								sizeof(cast_tests) / sizeof(cast_tests[0]), cast_tests};
