/*
    belle-sip - SIP (RFC3261) library.
    Copyright (C) 2013  Belledonne Communications SARL, Grenoble, France

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
parser grammar http;


options {	
	language = C;
	
} 

@parser::header {
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

#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wunused"
}

@parser::includes { 
#include "belle-sip/belle-sip.h"
#include "belle_sip_internal.h"

}

@rulecatch 
{
    if (HASEXCEPTION())
    {

    // This is ugly.  We set the exception type to ANTLR3_RECOGNITION_EXCEPTION so we can always
    // catch them.
    //PREPORTERROR();
    EXCEPTION->type = ANTLR3_RECOGNITION_EXCEPTION;
    }
}

/*
  absoluteURI   = scheme ":" ( hier_part | opaque_part )
  hier_part     = ( net_path | abs_path ) [ "?" query ]
  net_path      = "//" authority [ abs_path ]
  abs_path      = "/"  path_segments
*/

 scheme: alpha ( alpha | DIGIT | PLUS | DASH | DOT )*;

generic_uri  returns [belle_generic_uri_t* ret=NULL]    
scope { belle_generic_uri_t* current; }
@init { $generic_uri::current = $ret = belle_generic_uri_new(); }
   :  scheme {belle_generic_uri_set_scheme($generic_uri::current,(const char*)$scheme.text->chars);} COLON hier_part[$generic_uri::current] ;
catch [ANTLR3_MISMATCHED_TOKEN_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($generic_uri::current);
   $ret=NULL;
}   
   

hier_part[belle_generic_uri_t* uri] returns [belle_generic_uri_t* ret=NULL]  
:  	(
	(SLASH SLASH)=>( SLASH SLASH authority[uri] (SLASH path_segments[uri])?) 
	| 
	(SLASH)=>SLASH  path_segments[uri] ) 
	(QMARK query 
	          {
					  char* unescaped_query;
					  unescaped_query=belle_sip_to_unescaped_string((const char *)$query.text->chars);
					  belle_generic_uri_set_query(uri,(const char*)unescaped_query);
					  belle_sip_free(unescaped_query);
					  }) ?;

path_segments[belle_generic_uri_t* uri]
  : (segment ( SLASH segment )*)
  {
  char* unescaped_path;
  unescaped_path=belle_sip_to_unescaped_string((const char *)$path_segments.text->chars);
  belle_generic_uri_set_path(uri,(const char*)unescaped_path);
  belle_sip_free(unescaped_path);
  };
segment: pchar* ( SEMI param )*;
param: pchar*;
pchar:  unreserved | escaped | COLON | AT | AND | EQUAL | PLUS | DOLLARD | COMMA;

query: uric+;
uric: reserved | unreserved | escaped;
                        
authority[belle_generic_uri_t* uri] 
:   	(
	(authority_userinfo[NULL])=>(authority_userinfo[uri] authority_hostport[uri]) 
	| authority_hostport[uri] ); 


authority_userinfo[belle_generic_uri_t* uri] 
scope { belle_generic_uri_t* current; }
@init {$authority_userinfo::current=uri;}
       :	authority_user ( COLON authority_password )? AT ;
authority_user            :	  ( unreserved  | escaped | user_unreserved )+ {
                                                                  char* unescaped_username;
                                                                  unescaped_username=belle_sip_to_unescaped_string((const char *)$text->chars);
                                                                  belle_generic_uri_set_user($authority_userinfo::current,unescaped_username);
                                                                  belle_sip_free(unescaped_username);
                                                                  };

authority_password        :	  ( unreserved | escaped | AND | EQUAL | PLUS | DOLLARD | COMMA )* {
                                                                              char* unescaped_userpasswd;
                                                                              unescaped_userpasswd=belle_sip_to_unescaped_string((const char *)$text->chars);
                                                                              belle_generic_uri_set_user_password($authority_userinfo::current,unescaped_userpasswd);
                                                                              belle_sip_free(unescaped_userpasswd);
                                                                              };
authority_hostport[belle_generic_uri_t* uri] 
scope { belle_generic_uri_t* current; }
@init {$authority_hostport::current=uri;}
        :	  host ( COLON port {belle_generic_uri_set_port($authority_hostport::current,$port.ret);})? {belle_generic_uri_set_host($authority_hostport::current,$host.ret);};
