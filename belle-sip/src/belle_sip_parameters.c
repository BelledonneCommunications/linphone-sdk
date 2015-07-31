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
#include "belle-sip/parameters.h"
#include "belle_sip_internal.h"
#include "belle-sip/headers.h"

void belle_sip_parameters_init(belle_sip_parameters_t *obj){
}

void belle_sip_parameters_clean(belle_sip_parameters_t* params) {
	if (params->param_list) belle_sip_list_free_with_data (params->param_list, (void (*)(void*))belle_sip_param_pair_destroy);
	if (params->paramnames_list) belle_sip_list_free(params->paramnames_list);
	params->paramnames_list=NULL;
	params->param_list=NULL;
}

static void belle_sip_parameters_destroy(belle_sip_parameters_t* params) {
	belle_sip_parameters_clean(params);
}

void belle_sip_parameters_copy_parameters_from(belle_sip_parameters_t *params, const belle_sip_parameters_t *orig){
	belle_sip_list_t* list=orig->param_list;
	for(;list!=NULL;list=list->next){
		belle_sip_param_pair_t* container = (belle_sip_param_pair_t* )(list->data);
		belle_sip_parameters_set_parameter( params,container->name,container->value);
	}
}

static void belle_sip_parameters_clone(belle_sip_parameters_t *params, const belle_sip_parameters_t *orig){
	belle_sip_parameters_copy_parameters_from(params,orig);
}


belle_sip_error_code belle_sip_parameters_marshal(const belle_sip_parameters_t* params, char* buff, size_t buff_size, size_t *offset) {
	belle_sip_list_t* list=params->param_list;
	belle_sip_error_code error=BELLE_SIP_OK;
	for(;list!=NULL;list=list->next){
		belle_sip_param_pair_t* container = (belle_sip_param_pair_t* )(list->data);
		if (container->value) {
			error=belle_sip_snprintf(buff,buff_size,offset,";%s=%s", container->name, container->value);
		} else {
			error=belle_sip_snprintf(buff,buff_size,offset,";%s", container->name);
		}
		if (error!=BELLE_SIP_OK) return error;
	}
	return error;
}
BELLE_SIP_NEW_HEADER(parameters,header,"parameters")
const belle_sip_list_t *belle_sip_parameters_get_parameters(const belle_sip_parameters_t* obj) {
	return obj->param_list;
}

const char* belle_sip_parameters_get_parameter_base(const belle_sip_parameters_t* params,const char* name,belle_sip_compare_func func) {
	belle_sip_list_t *  lResult = belle_sip_list_find_custom(params->param_list, func, name);
	if (lResult) {
		return ((belle_sip_param_pair_t*)(lResult->data))->value;
	}
	else {
		return NULL;
	}
}
const char* belle_sip_parameters_get_parameter(const belle_sip_parameters_t* params,const char* name) {
	return belle_sip_parameters_get_parameter_base(params,name,(belle_sip_compare_func)belle_sip_param_pair_comp_func);
}
const char* belle_sip_parameters_get_case_parameter(const belle_sip_parameters_t* params,const char* name) {
	return belle_sip_parameters_get_parameter_base(params,name,(belle_sip_compare_func)belle_sip_param_pair_case_comp_func);
}

unsigned int belle_sip_parameters_has_parameter(const belle_sip_parameters_t* params,const char* name) {
	return belle_sip_list_find_custom(params->param_list, (belle_sip_compare_func)belle_sip_param_pair_comp_func, name) != NULL;
}

void belle_sip_parameters_set_parameter(belle_sip_parameters_t* params,const char* name,const char* value) {
	/*1 check if present*/
	belle_sip_param_pair_t* lNewpair;
	belle_sip_list_t *  lResult = belle_sip_list_find_custom(params->paramnames_list, (belle_sip_compare_func)strcmp, name);
	/* first remove from header names list*/
	if (lResult) {
		params->paramnames_list=belle_sip_list_delete_link(params->paramnames_list,lResult);
	}
	/* next from header list*/
	lResult = belle_sip_list_find_custom(params->param_list, (belle_sip_compare_func)belle_sip_param_pair_comp_func, name);
	if (lResult) {
		belle_sip_param_pair_destroy(lResult->data);
		params->param_list=belle_sip_list_delete_link(params->param_list,lResult);
	}
	/* 2 insert*/
	lNewpair = belle_sip_param_pair_new(name,value);
	params->param_list=belle_sip_list_append(params->param_list,lNewpair);
	params->paramnames_list=belle_sip_list_append(params->paramnames_list,lNewpair->name);
}

void belle_sip_parameters_set(belle_sip_parameters_t *parameters, const char* params){
	belle_sip_parameters_clean(parameters);
	if (params && *params!='\0'){
		char *tmp=belle_sip_strdup(params);
		char *end_of_param;
		char *current=tmp;
		char *equal;
		char *next;

		do{
			end_of_param=strchr(current,';');
			equal=strchr(current,'=');
			if (!end_of_param) {
				end_of_param=current+strlen(current);
				next=end_of_param;
			}else{
				*end_of_param='\0';
				next=end_of_param+1;
			}
			if (equal && equal<end_of_param){
				*equal='\0';
				belle_sip_parameters_set_parameter(parameters,current,equal+1);
			}else{
				belle_sip_parameters_set_parameter(parameters,current,NULL);
			}
			current=next;
		}while(*current!='\0');
		belle_sip_free(tmp);
	}
}

const belle_sip_list_t*	belle_sip_parameters_get_parameter_names(const belle_sip_parameters_t* params) {
	return params?params->paramnames_list:NULL;
}

void belle_sip_parameters_remove_parameter(belle_sip_parameters_t* params,const char* name) {
	/*1 check if present*/
	belle_sip_list_t *  lResult = belle_sip_list_find_custom(params->paramnames_list, (belle_sip_compare_func)strcmp, name);
	/* first remove from header names list*/
	if (lResult) {
		params->paramnames_list=belle_sip_list_delete_link(params->paramnames_list,lResult);
		/*next remove node*/
		lResult = belle_sip_list_find_custom(params->param_list, (belle_sip_compare_func)belle_sip_param_pair_comp_func, name);
		if (lResult) {
			belle_sip_param_pair_destroy(lResult->data);
			params->param_list=belle_sip_list_delete_link(params->param_list,lResult);
		}
	}
}

