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

#ifndef DICT_H
#define DICT_H

#include <sys/types.h>

#include "object.h"


typedef struct belle_sip_dict belle_sip_dict_t;
#define BELLE_SIP_DICT(obj) BELLE_SIP_CAST(obj,belle_sip_dict_t)

/**
 * @brief belle_sip_dict_create
 * @return an instance of a belle_sip_dict_t object.
 * @note The object is not owned by default.
 * @note all belle_sip_dict_set_* functions will overwrite existing values.
 */
belle_sip_dict_t* belle_sip_dict_create();

/**
 * @brief belle_sip_dict_set_int stores an integer into the dictionary
 * @param obj the dictionary instance
 * @param key the name of the integer to store
 * @param value value to store
 */
void belle_sip_dict_set_int(belle_sip_dict_t* obj, const char* key, int value);

/**
 * @brief belle_sip_dict_get_int retrieves an integer from the dictionary
 * @param obj the dictionary instance
 * @param key name of the integer to retrieve
 * @param default_value value to return if the key is not found
 * @return the searched integer if the key exists, default_value if not found
 */
int belle_sip_dict_get_int(belle_sip_dict_t* obj, const char* key, int default_value);

/**
 * @brief belle_sip_dict_set_string stores a string into the dictionary
 * @param obj the dictionary instance
 * @param key the name of the string to store
 * @param value value to store
 */
void belle_sip_dict_set_string(belle_sip_dict_t* obj, const char* key, const char*value);
/**
 * @brief belle_sip_dict_get_string retrieves a string from the dictionary
 * @param obj the dictionary instance
 * @param key the name of the string to retrieve
 * @param default_value
 * @return the searched string if the key exists, default_value if not found
 */
const char* belle_sip_dict_get_string(belle_sip_dict_t* obj, const char* key, const char* default_value);

/**
 * @brief belle_sip_dict_set_int64 stores an int64 in the dictionary
 * @param obj the dictionary instance
 * @param key the name of the integer to store
 * @param value value to store
 */
void belle_sip_dict_set_int64(belle_sip_dict_t* obj, const char* key, int64_t value);

/**
 * @brief belle_sip_dict_get_int64 retrieves an int64 from the dictionary
 * @param obj the dictionary instance
 * @param key the name of the integer to retrieve
 * @param default_value value to return if the key is not found
 * @return the searched int64 if the key exists, default_value if not found
 */
int64_t belle_sip_dict_get_int64(belle_sip_dict_t* obj, const char* key, int64_t default_value);

/**
 * @brief belle_sip_dict_remove will erase the value for a key
 * @param obj the dictionary instance
 * @param key the name of the integer to remove
 * @return 0 if the key was found, 1 otherwise
 */
int belle_sip_dict_remove(belle_sip_dict_t* obj, const char* key);

/**
 * @brief belle_sip_dict_clear will clear the object's dictionary.
 * @param obj the dictionary instance
 */
void belle_sip_dict_clear(belle_sip_dict_t* obj);

/**
 * @brief belle_sip_dict_haskey tells if a key exists in the dictionary.
 * @param obj the dictionary instance
 * @param key the key to look for
 * @return 1 if the key exists, 0 otherwise
 * @todo create unit test
 */
int belle_sip_dict_haskey(belle_sip_dict_t* obj, const char* key);

#endif // DICT_H
