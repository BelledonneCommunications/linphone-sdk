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

#ifndef PARAMETERS_H_
#define PARAMETERS_H_

#include "belle-sip/utils.h"
#include "belle-sip/list.h"

BELLE_SIP_BEGIN_DECLS

/***
 *  parameters
 *
 */

belle_sip_parameters_t* belle_sip_parameters_new(void);
/*
 * remove all parameters */
BELLESIP_EXPORT void belle_sip_parameters_clean(belle_sip_parameters_t* params);

/*BELLESIP_EXPORT void belle_sip_parameters_destroy(belle_sip_parameters_t* params);*/

BELLESIP_EXPORT const char*	belle_sip_parameters_get_parameter(const belle_sip_parameters_t* obj,const char* name);
/*
 * same as #belle_sip_parameters_get_parameter but name is case insensitive */
BELLESIP_EXPORT const char*	belle_sip_parameters_get_case_parameter(const belle_sip_parameters_t* params,const char* name);

/**
 * returns 0 if not found
 */
BELLESIP_EXPORT unsigned int belle_sip_parameters_has_parameter(const belle_sip_parameters_t* obj,const char* name);

BELLESIP_EXPORT void	belle_sip_parameters_set_parameter(belle_sip_parameters_t* obj,const char* name,const char* value);

/**
 * Assign a full set of parameters to the belle_sip_parameters_t object.
 * Parameters are given as string of key=value pairs separated with semicolons, where value is optional.
 * @example belle_sip_parameters_set(parameters,"param1=value1;param2;param3=value3");
**/
BELLESIP_EXPORT void belle_sip_parameters_set(belle_sip_parameters_t *parameters, const char* params);

BELLESIP_EXPORT const belle_sip_list_t *	belle_sip_parameters_get_parameter_names(const belle_sip_parameters_t* obj);

BELLESIP_EXPORT const belle_sip_list_t *	belle_sip_parameters_get_parameters(const belle_sip_parameters_t* obj);

BELLESIP_EXPORT void	belle_sip_parameters_remove_parameter(belle_sip_parameters_t* obj,const char* name);

BELLESIP_EXPORT belle_sip_error_code belle_sip_parameters_marshal(const belle_sip_parameters_t* obj, char* buff, size_t buff_size, size_t *offset);

BELLESIP_EXPORT void belle_sip_parameters_copy_parameters_from(belle_sip_parameters_t *params, const belle_sip_parameters_t *orig);

#define BELLE_SIP_PARAMETERS(obj) BELLE_SIP_CAST(obj,belle_sip_parameters_t)

BELLE_SIP_END_DECLS

#endif /*PARAMETERS_H_*/

