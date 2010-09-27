//============================================================================
// Name        : parser-antlr.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include "sip_uri.h"
#include <stdio.h>



int main (int argc, char *argv[]) {

	sip_uri* L_uri = sip_uri_parse("sip:toto@titi.com:5060;transport=tcp");
	printf(sip_uri_to_string(L_uri));

	return 0;
}
