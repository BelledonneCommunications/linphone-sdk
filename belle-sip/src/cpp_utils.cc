/*
	belle-sip - SIP (RFC3261) library.
	Copyright (C) 2010-2018  Belledonne Communications SARL

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


#ifndef BELLE_SIP_USE_STL
#define BELLE_SIP_USE_STL 1
#endif


#if BELLE_SIP_USE_STL
#include <functional>

typedef std::function<int (unsigned int)> belle_sip_source_cpp_func_t;

static int belle_sip_source_cpp_func(belle_sip_source_cpp_func_t* user_data, unsigned int events)
{
	int result = (*user_data)(events);
	return result;
}
 
static void belle_sip_source_on_remove(belle_sip_source_t* source)
{
	delete static_cast<belle_sip_source_cpp_func_t *>(belle_sip_source_get_user_data(source));
	belle_sip_source_set_user_data(source,NULL);
}

belle_sip_source_t * belle_sip_main_loop_create_cpp_timeout(belle_sip_main_loop_t *ml
								, belle_sip_source_cpp_func_t *func
								, unsigned int timeout_value_ms
								, const char* timer_name)
{
	belle_sip_source_t* source = belle_sip_main_loop_create_timeout(  ml
								, (belle_sip_source_func_t)belle_sip_source_cpp_func
								, func
								, timeout_value_ms
								, timer_name);
	belle_sip_source_set_remove_cb(source,belle_sip_source_on_remove);
	return source;
}

static void do_later(void *ud){
	std::function<void (void)> *func = static_cast<std::function<void (void)> *>(ud);
	(*func)();
	delete func;
}

void belle_sip_main_loop_cpp_do_later(belle_sip_main_loop_t *ml, const std::function<void (void)> &func){
	belle_sip_main_loop_do_later(ml, do_later, new std::function<void (void)>(func));
}


#endif
