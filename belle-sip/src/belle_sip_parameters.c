/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "belle-sip/belle-sip.h"
#include "belle-sip/parameters.h"
#include "belle_sip_internal.h"
#include "belle-sip/headers.h"

void belle_sip_parameters_destroy(belle_sip_parameters_t* params) {
	if (params->param_list) belle_sip_list_free (params->param_list);
	if (params->paramnames_list) belle_sip_list_free (params->paramnames_list);
	belle_sip_header_destroy(BELLE_SIP_HEADER(params));
}
void belle_sip_parameters_init(belle_sip_parameters_t *obj) {
	belle_sip_object_init_type(obj,belle_sip_parameters_t);
	belle_sip_header_init((belle_sip_header_t*)obj);
}

belle_sip_parameters_t* belle_sip_parameters_new() {
	belle_sip_parameters_t* l_object = (belle_sip_parameters_t*)belle_sip_object_new(belle_sip_parameters_t,(belle_sip_object_destroy_t)belle_sip_parameters_destroy);
	belle_sip_header_init((belle_sip_header_t*)l_object);
	return l_object;
}

const char*	belle_sip_parameters_get_parameter(belle_sip_parameters_t* params,const char* name) {
	belle_sip_list_t *  lResult = belle_sip_list_find_custom(params->param_list, (belle_sip_compare_func)belle_sip_param_pair_comp_func, name);
	if (lResult) {
		return ((belle_sip_param_pair_t*)(lResult->data))->value;
	}
	else {
		return NULL;
	}
}
unsigned int belle_sip_parameters_is_parameter(belle_sip_parameters_t* params,const char* name) {
	return belle_sip_list_find_custom(params->param_list, (belle_sip_compare_func)belle_sip_param_pair_comp_func, name) != NULL;
}
void	belle_sip_parameters_set_parameter(belle_sip_parameters_t* params,const char* name,const char* value) {
	/*1 check if present*/
	belle_sip_list_t *  lResult = belle_sip_list_find_custom(params->paramnames_list, (belle_sip_compare_func)strcmp, name);
	/* first remove from header names list*/
	if (lResult) {
		belle_sip_list_remove_link(params->paramnames_list,lResult);
	}
	/* next from header list*/
	lResult = belle_sip_list_find_custom(params->param_list, (belle_sip_compare_func)belle_sip_param_pair_comp_func, name);
	if (lResult) {
		belle_sip_param_pair_destroy(lResult->data);
		belle_sip_list_remove_link(params->param_list,lResult);
	}
	/* 2 insert*/
	belle_sip_param_pair_t* lNewpair = belle_sip_param_pair_new(name,value);
	params->param_list=belle_sip_list_append(params->param_list,lNewpair);
	params->paramnames_list=belle_sip_list_append(params->paramnames_list,lNewpair->name);
}

const belle_sip_list_t*	belle_sip_parameters_get_parameter_names(belle_sip_parameters_t* params) {
	return params->paramnames_list;
}
void	belle_sip_parameters_remove_parameter(belle_sip_parameters_t* params,const char* name) {
	/*1 check if present*/
	belle_sip_list_t *  lResult = belle_sip_list_find_custom(params->paramnames_list, (belle_sip_compare_func)strcmp, name);
	/* first remove from header names list*/
	if (lResult) {
		belle_sip_list_remove_link(params->paramnames_list,lResult);
	} else {
		belle_sip_warning("cannot remove param \%s because not present",name);
	}
}

