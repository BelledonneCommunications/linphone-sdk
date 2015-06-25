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

#include "belle-sip/dict.h"
#include "belle-sip/object.h"
#include "belle-sip/belle-sip.h"
#include "belle_sip_internal.h"

#define BELLE_SIP_DICT(obj) BELLE_SIP_CAST(obj,belle_sip_dict_t)


static void belle_sip_dict_string_destroy( void* data )
{
	belle_sip_free(data);
}

static void* belle_sip_dict_string_clone( const char* str, void* data )
{
	(void)str;
	return belle_sip_strdup((const char*)data);
}


belle_sip_dict_t* belle_sip_dict_create()
{
	return belle_sip_object_new(belle_sip_dict_t);
}

static void belle_sip_dict_destroy( belle_sip_dict_t* obj)
{
}

void belle_sip_dict_set_int(belle_sip_dict_t* obj, const char* key, int value)
{
	char tmp[30];
	snprintf(tmp,sizeof(tmp),"%i",value);
	belle_sip_dict_set_string(obj, key, tmp);
}

int belle_sip_dict_get_int(belle_sip_dict_t* obj, const char* key, int default_value)
{
	const char *str=belle_sip_object_data_get(BELLE_SIP_OBJECT(obj),key);
	if (str!=NULL) {
		int ret=0;
		if (strstr(str,"0x")==str){
			sscanf(str,"%x",&ret);
		}else ret=atoi(str);
		return ret;
	}
	else return default_value;
}

void belle_sip_dict_set_string(belle_sip_dict_t* obj, const char*key, const char*value)
{
	belle_sip_object_data_set( BELLE_SIP_OBJECT(obj), key, (void*)belle_sip_strdup(value), belle_sip_dict_string_destroy );
}

const char* belle_sip_dict_get_string(belle_sip_dict_t* obj, const char* key, const char* default_value)
{
	void* data = belle_sip_object_data_get( BELLE_SIP_OBJECT(obj), key );
	if( data ) return (const char *)data;
	else return default_value;
}

void belle_sip_dict_set_int64(belle_sip_dict_t* obj, const char* key, int64_t value)
{
	char tmp[30];
#if defined (_MSC_VER)
	snprintf(tmp,sizeof(tmp),"%I64d",value);
#else
	snprintf(tmp,sizeof(tmp),"%" PRId64"",value);
#endif
	belle_sip_dict_set_string(obj,key,tmp);
}

int64_t belle_sip_dict_get_int64(belle_sip_dict_t* obj, const char* key, int64_t default_value)
{
	const char *str= belle_sip_object_data_get( BELLE_SIP_OBJECT(obj), key );
	if (str!=NULL) {
#ifdef _WIN32
		return (int64_t)_atoi64(str);
#else
		return atoll(str);
#endif
	}
	else return default_value;
}

int belle_sip_dict_remove(belle_sip_dict_t* obj, const char*key)
{
	return belle_sip_object_data_remove(BELLE_SIP_OBJECT(obj), key);
}

void belle_sip_dict_clone( const belle_sip_dict_t* src, belle_sip_dict_t* dst)
{
	belle_sip_dict_clear(dst);
	belle_sip_dict_merge(src, dst);
}

void belle_sip_dict_merge( const belle_sip_dict_t* src, belle_sip_dict_t* dst)
{
	belle_sip_object_data_merge(BELLE_SIP_OBJECT(src), BELLE_SIP_OBJECT(dst), belle_sip_dict_string_clone);
}

int belle_sip_dict_haskey(const belle_sip_dict_t* obj, const char* key)
{
	return belle_sip_object_data_exists(BELLE_SIP_OBJECT(obj), key);
}


void belle_sip_dict_foreach(const belle_sip_dict_t* obj, void (*apply_func)(const char*, void*, void*), void* userdata)
{
	belle_sip_object_data_foreach(BELLE_SIP_OBJECT(obj),apply_func, userdata);
}



void belle_sip_dict_clear(belle_sip_dict_t* obj)
{
	belle_sip_object_data_clear(BELLE_SIP_OBJECT(obj));
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_dict_t);
BELLE_SIP_INSTANCIATE_VPTR(belle_sip_dict_t, belle_sip_object_t,
						   belle_sip_dict_destroy,
						   NULL,
						   NULL,
						   TRUE);

