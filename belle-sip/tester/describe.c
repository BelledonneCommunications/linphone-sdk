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

#include <stdio.h>

#include "belle-sip/belle-sip.h"

int main(int argc, char *argv[]){
	char *str;
	if (argc!=2){
		fprintf(stderr,"Usage:\n%s <type name>\n",argv[0]);
		return -1;
	}
	str=belle_sip_object_describe_type_from_name(argv[1]);
	if (str){
		printf("%s\n",str);
		belle_sip_free(str);
	}else{
		fprintf(stderr,"Unknown type %s\n",argv[1]);
		return -1;
	}
	return 0;
}
