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

#ifndef PARAMETERS_H_
#define PARAMETERS_H_

#include "belle-sip/list.h"
/***
 *  parameters
 *
 */

typedef struct _belle_sip_parameters belle_sip_parameters_t;

belle_sip_parameters_t* belle_sip_parameters_new();
/*
 * remove all parameters */
void belle_sip_parameters_clean(belle_sip_parameters_t* params);


BELLESIP_EXPORT const char*	belle_sip_parameters_get_parameter(const belle_sip_parameters_t* obj,const char* name);
/*
 * same as #belle_sip_parameters_get_parameter but name is case insensitive */
const char*	belle_sip_parameters_get_case_parameter(const belle_sip_parameters_t* params,const char* name);

/**
 * returns 0 if not found
 */
BELLESIP_EXPORT unsigned int belle_sip_parameters_is_parameter(const belle_sip_parameters_t* obj,const char* name);

void	belle_sip_parameters_set_parameter(belle_sip_parameters_t* obj,const char* name,const char* value);

const belle_sip_list_t *	belle_sip_parameters_get_parameter_names(const belle_sip_parameters_t* obj);

const belle_sip_list_t *	belle_sip_parameters_get_parameters(const belle_sip_parameters_t* obj);

void	belle_sip_parameters_remove_parameter(belle_sip_parameters_t* obj,const char* name);

int belle_sip_parameters_marshal(const belle_sip_parameters_t* obj, char* buff,unsigned int offset,unsigned int buff_size);

#define BELLE_SIP_PARAMETERS(obj) BELLE_SIP_CAST(obj,belle_sip_parameters_t)

#endif /*PARAMETERS_H_*/
