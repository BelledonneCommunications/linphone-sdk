/*
 * Copyright (c) 2010-2020 Belledonne Communications SARL.
 *
 * This file is part of bctoolbox.
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

#ifndef BCTBX_PARAM_STRING_H_
#define BCTBX_PARAM_STRING_H_

#include "bctoolbox/port.h"

// This file provides string manipulation utility when handling a parameter string of the from
// "param1;param2=120;param3=true"



/**
 * Parses a fmtp string such as "profile=0;level=10", finds the value matching
 * parameter param_name, and writes it into result.
 * If a parameter name is found multiple times, only the value of the last occurence is returned.
 * @param paramString the fmtp line (format parameters)
 * @param param_name the parameter to search for
 * @param result the value given for the parameter (if found)
 * @param result_len the size allocated to hold the result string
 * @return TRUE if the parameter was found, else FALSE.
**/
BCTBX_PUBLIC bool_t bctbx_param_string_get_value(const char *paramString, const char *param_name, char *result, size_t result_len);

/**
 * Parses a fmtp string such as "profile=0;level=10". If the value is "true" or "false", returns the corresponding boolean
 * @param paramString the fmtp line (format parameters)
 * @param param_name the parameter to search for
 * @return FALSE if parameter was not found, else TRUE if the parameter value was "true", FALSE if it was "false"
**/
BCTBX_PUBLIC bool_t bctbx_param_string_get_bool_value(const char *paramString, const char *param_name);
#endif /*BCTBX_PARAM_STRING_H_*/




