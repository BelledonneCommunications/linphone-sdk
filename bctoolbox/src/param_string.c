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


#include "bctoolbox/port.h"
#include "bctoolbox/param_string.h"

static const char *find_param_occurence_of(const char *fmtp, const char *param){
	const char *pos=fmtp;
	int param_len = (int)strlen(param);
	do{
		pos=strstr(pos,param);
		if (pos){
			/*check that the occurence found is not a subword of a parameter name*/
			if (pos==fmtp){
				if (pos[param_len] == '=') break; /* found it */
			}else if ((pos[-1]==';' || pos[-1]==' ') && pos[param_len] == '='){
				break; /* found it */
			}
			pos+=strlen(param);
		}
	}while (pos!=NULL);
	return pos;
}

static const char *find_last_param_occurence_of(const char *fmtp, const char *param){
	const char *pos=fmtp;
	const char *lastpos=NULL;
	do{
		pos=find_param_occurence_of(pos,param);
		if (pos) {
			lastpos=pos;
			pos+=strlen(param);
		}
	}while(pos!=NULL);
	return lastpos;
}

bool_t bctbx_param_string_get_value(const char *paramString, const char *param_name, char *result, size_t result_len){
	const char *pos=find_last_param_occurence_of(paramString,param_name);
	memset(result, '\0', result_len);
	if (pos){
		const char *equal=strchr(pos,'=');
		if (equal){
			int copied;
			const char *end=strchr(equal+1,';');
			if (end==NULL) end=paramString+strlen(paramString); /*assuming this is the last param */
			copied=MIN((int)(result_len-1),(int)(end-(equal+1)));
			strncpy(result,equal+1,copied);
			result[copied]='\0';
			return TRUE;
		}
	}
	return FALSE;
}


bool_t bctbx_param_string_get_bool_value(const char *paramString, const char *param_name)
{
	size_t result_len = 5;
	char *result = bctbx_malloc(result_len);
	// True if param is found, false if not
	bool_t res = bctbx_param_string_get_value(paramString, param_name, result, result_len);
	res = res && strcmp(result, "true")==0;
	free(result);
	return res;
}

