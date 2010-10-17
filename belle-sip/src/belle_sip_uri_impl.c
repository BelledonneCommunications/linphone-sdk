/*
 * SipUri.cpp
 *
 *  Created on: 18 sept. 2010
 *      Author: jehanmonnier
 */

#include "belle_sip_uri.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "belle_sip_uriParser.h"
#include "belle_sip_uriLexer.h"
#include "belle_sip_utils.h"

#define GET_SET_STRING(object_type,attribute) \
	const char* object_type##_get_##attribute (object_type* obj) {\
		return obj->attribute;\
	}\
	void object_type##_set_##attribute (object_type* obj,const char* value) {\
		if (obj->attribute != NULL) free((void*)obj->attribute);\
		obj->attribute=malloc(strlen(value)+1);\
		strcpy((char*)(obj->attribute),value);\
	}

#define SIP_URI_GET_SET_STRING(attribute) GET_SET_STRING(belle_sip_uri,attribute)

#define GET_SET_INT(object_type,attribute,type) \
	type  object_type##_get_##attribute (object_type* obj) {\
		return obj->attribute;\
	}\
	void object_type##_set_##attribute (object_type* obj,type  value) {\
		obj->attribute=value;\
	}

#define SIP_URI_GET_SET_UINT(attribute) GET_SET_INT(belle_sip_uri,attribute,unsigned int)
#define SIP_URI_GET_SET_INT(attribute) GET_SET_INT(belle_sip_uri,attribute,int)

#define GET_SET_BOOL(object_type,attribute,getter) \
	unsigned int object_type##_##getter##_##attribute (object_type* obj) {\
		return obj->attribute;\
	}\
	void object_type##_set_##attribute (object_type* obj,unsigned int value) {\
		obj->attribute=value;\
	}


#define SIP_URI_GET_SET_BOOL(attribute) GET_SET_BOOL(belle_sip_uri,attribute,is)
#define SIP_URI_HAS_SET_BOOL(attribute) GET_SET_BOOL(belle_sip_uri,attribute,has)




struct _belle_sip_uri {
	unsigned int secure;
	const char* user;
	const char* host;
	unsigned int port;

	const char* transport_param;
	const char* user_param;
	const char* method_param;
	unsigned int lr_param;
	const char* maddr_param;
	int 		ttl_param;

	belle_sip_list* header_list;
	belle_sip_list* headernames_list;
};
void belle_sip_uri_delete(belle_sip_uri* uri) {
	if (uri->user) free (uri->user);
	if (uri->host) free (uri->host);

	if (uri->transport_param) free (uri->transport_param);
	if (uri->user_param) free (uri->user_param);
	if (uri->method_param) free (uri->method_param);
	if (uri->maddr_param) free (uri->maddr_param);

	if (uri->header_list) belle_sip_list_free (uri->header_list);
	if (uri->headernames_list) belle_sip_list_free (uri->headernames_list);
	free(uri);
}

typedef struct _header_pair {
	const char* name;
	const char* value;

} header_pair;
static const header_pair* header_pair_new(const char* name,const char* value) {
	header_pair* lPair = (header_pair*)malloc( sizeof(header_pair));
	lPair->name=strdup(name);
	lPair->value=strdup(value);
	return lPair;
}
static void header_pair_delete(header_pair*  pair) {
	free(pair->name);
	free(pair->value);
	free (pair);
}
static int header_pair_comp_func(const header_pair *a, const char*b) {
	return strcmp(a->name,b);
}

belle_sip_uri* belle_sip_uri_parse (const char* uri) {
	pANTLR3_INPUT_STREAM           input;
	pbelle_sip_uriLexer               lex;
	pANTLR3_COMMON_TOKEN_STREAM    tokens;
	pbelle_sip_uriParser              parser;
	input  = antlr3NewAsciiStringCopyStream	(
			(pANTLR3_UINT8)uri,
			(ANTLR3_UINT32)strlen(uri),
			NULL);
	lex    = belle_sip_uriLexerNew                (input);
	tokens = antlr3CommonTokenStreamSourceNew  (ANTLR3_SIZE_HINT, TOKENSOURCE(lex));
	parser = belle_sip_uriParserNew               (tokens);

	belle_sip_uri* l_parsed_uri = parser->an_sip_uri(parser);

	// Must manually clean up
	//
	parser ->free(parser);
	tokens ->free(tokens);
	lex    ->free(lex);
	input  ->close(input);
	return l_parsed_uri;
}
belle_sip_uri* belle_sip_uri_new () {
	belle_sip_uri* lUri = (belle_sip_uri*)malloc(sizeof(belle_sip_uri));
	memset(lUri,0,sizeof(belle_sip_uri));
	lUri->ttl_param==-1;
	return lUri;
}



char*	belle_sip_uri_to_string(belle_sip_uri* uri)  {
	return belle_sip_concat(	"sip:"
					,(uri->user?uri->user:"")
					,(uri->user?"@":"")
					,uri->host
					,(uri->transport_param?";transport=":"")
					,(uri->transport_param?uri->transport_param:"")
					,NULL);
}


const char*	belle_sip_uri_get_header(belle_sip_uri* uri,const char* name) {
	belle_sip_list * lResult = belle_sip_list_find_custom(uri->header_list, header_pair_comp_func, name);
	if (lResult) {
		return ((header_pair*)(lResult->data))->value;
	}
	else {
		return NULL;
	}
}
void	belle_sip_uri_set_header(belle_sip_uri* uri,const char* name,const char* value) {
	/*1 check if present*/
	belle_sip_list * lResult = belle_sip_list_find_custom(uri->headernames_list, strcmp, name);
	/* first remove from headersnames list*/
	if (lResult) {
		belle_sip_list_remove_link(uri->headernames_list,lResult);
	}
	/* next from header list*/
	lResult = belle_sip_list_find_custom(uri->header_list, header_pair_comp_func, name);
	if (lResult) {
		header_pair_delete(lResult->data);
		belle_sip_list_remove_link(uri->header_list,lResult);
	}
	/* 2 insert*/
	header_pair* lNewpair = header_pair_new(name,value);
	uri->header_list=belle_sip_list_append(uri->header_list,lNewpair);
	uri->headernames_list=belle_sip_list_append(uri->headernames_list,lNewpair->name);
}

belle_sip_list*	belle_sip_uri_get_header_names(belle_sip_uri* uri) {
	return uri->headernames_list;
}



SIP_URI_GET_SET_BOOL(secure)

SIP_URI_GET_SET_STRING(user)
SIP_URI_GET_SET_STRING(host)
SIP_URI_GET_SET_UINT(port)

SIP_URI_GET_SET_STRING(transport_param)
SIP_URI_GET_SET_STRING(user_param)
SIP_URI_GET_SET_STRING(method_param)
SIP_URI_GET_SET_STRING(maddr_param)
SIP_URI_GET_SET_INT(ttl_param)
SIP_URI_HAS_SET_BOOL(lr_param)
