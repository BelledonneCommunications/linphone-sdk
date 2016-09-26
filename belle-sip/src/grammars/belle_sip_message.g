/*     belle-sip - SIP (RFC3261) library.
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

grammar belle_sip_message;


options { 
  language = C;
} 


@parser::header {
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

#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wunused"
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
}
@lexer::header {
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

#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wunused"

}

@includes { 

#include "belle-sip/defs.h"
#include "belle-sip/types.h"
#include "belle-sip/message.h"
#include "belle-sip/http-message.h"
#include "parserutils.h"
BELLESIP_INTERNAL_EXPORT void belle_sip_header_address_set_quoted_displayname(belle_sip_header_address_t* address,const char* value);
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

message returns [belle_sip_message_t* ret]
scope { size_t message_length; }
@init {$ret=NULL;}
  : message_raw[&($message::message_length)] {$ret=$message_raw.ret;};
  
message_raw [size_t* length]  returns [belle_sip_message_t* ret]
scope { size_t* message_length; }
@init {$message_raw::message_length=length;$ret=NULL;}
	:  common_request {$ret = $common_request.ret;} 
	   | common_response {$ret = $common_response.ret;} ;

request returns [belle_sip_request_t* ret=NULL] 
  :common_request {$ret=BELLE_SIP_REQUEST($common_request.ret);}; 
    
  
common_request	returns [belle_sip_message_t* ret]
scope { belle_sip_message_t* current; }
@init {$ret=$common_request::current=NULL; }
	:	  (  ( ~(CRLF)* sip_version CRLF) =>  request_line  {$ret=$common_request::current=BELLE_SIP_MESSAGE($request_line.ret);}
	    | ( ~(CRLF)* http_version CRLF) =>  http_request_line{$ret=$common_request::current=BELLE_SIP_MESSAGE($http_request_line.ret);}) 
		message_header[BELLE_SIP_MESSAGE($common_request::current)]+ last_crlf=CRLF {*($message_raw::message_length)=$last_crlf->user1;} /*message_body ?*/ ;

request_line  returns [belle_sip_request_t* ret=NULL]
scope { belle_sip_request_t* current; }
@init {$request_line::current = belle_sip_request_new(); $ret=$request_line::current; }  
	:	  method {belle_sip_request_set_method($request_line::current,(const char*)($method.text->chars));} 
	    SP 
	    (	uri {belle_sip_request_set_uri($request_line::current,$uri.ret);} 
	    	| 
	    	generic_uri {belle_sip_request_set_absolute_uri($request_line::current,$generic_uri.ret);} 
	     )
	    SP 
	    sip_version 
	    CRLF ;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($request_line::current);
   $ret=NULL;
} 
sip_version   
	:	 {IS_TOKEN(SIP/)}? generic_version; // 'SIP/' DIGIT '.' DIGIT; 


generic_version: alpha+ SLASH DIGIT DOT DIGIT;

message_header [belle_sip_message_t* message] 

	:	           (/*accept
//                |  accept_encoding
//                |  accept_language
//                |  alert_info
//                |  allow
//                |  authentication_info
//                |  authorization
//                |  header_call_id {belle_sip_message_add_header(message,BELLE_SIP_HEADER($header_call_id.ret));}/*
//                |  call_info
//                |  header_contact {belle_sip_message_add_header(message,BELLE_SIP_HEADER($header_contact.ret));}
//                |  content_disposition
//                |  content_encoding
//                |  content_language*/
//                |  header_content_length  {belle_sip_message_add_header(message,BELLE_SIP_HEADER($header_content_length.ret));}
//                |  header_content_type  {belle_sip_message_add_header(message,BELLE_SIP_HEADER($header_content_type.ret));}
//                |  header_cseq  {belle_sip_message_add_header(message,BELLE_SIP_HEADER($header_cseq.ret));}/*
//                |  date
//                |  error_info
//                |  expires*/
//                |  header_from  {belle_sip_message_add_header(message,BELLE_SIP_HEADER($header_from.ret));}/*
//                |  in_reply_to
//                |  max_forwards
//                |  mime_version
//                |  min_expires
//                |  organization
//                |  priority
//                |  proxy_authenticate
//                |  proxy_authorization
//                |  proxy_require*/
//                |  header_record_route  {belle_sip_message_add_header(message,BELLE_SIP_HEADER($header_record_route.ret));}/*
//                |  reply_to
//                |  require
//                |  retry_after*/
//                |  header_route  {belle_sip_message_add_header(message,BELLE_SIP_HEADER($header_route.ret));}/*
//                |  server
//                |  subject
//                |  supported
//                |  timestamp*/
//                |  header_to  {belle_sip_message_add_header(message,BELLE_SIP_HEADER($header_to.ret));}/*
//                |  header_diversion  {belle_sip_message_add_header(message,BELLE_SIP_HEADER($header_diversion.ret));}/*
//                |  unsupported
//                |  user_agent*/
//                |  header_via  {belle_sip_message_add_header(message,BELLE_SIP_HEADER($header_via.ret));}/*
//                |  warning
//                |  www_authenticate*/
                  header_extension_base[(BELLE_SIP_OBJECT_IS_INSTANCE_OF($message,belle_http_request_t) ||BELLE_SIP_OBJECT_IS_INSTANCE_OF($message,belle_http_response_t)) ] {
                    belle_sip_header_t* lheader = BELLE_SIP_HEADER($header_extension_base.ret);
                    do {
                      if (lheader == NULL) break; /*sanity check*/
                      
                      belle_sip_message_add_header(message,lheader);
                      }
                    while((lheader=belle_sip_header_get_next(lheader)) != NULL); } 
                ) CRLF 
               ;

       
/*
invitem           
	:	'INVITE' ; //INVITE in caps
ackm 	:	'ACK'; //ACK in caps
optionsm:	'OPTION'; //OPTIONS in caps
byem	:	'BYE' ; //BYE in caps
cancelm :	'CANCEL' ; //CANCEL in caps
registerm
	:	'REGISTER' ; //REGISTER in caps
optionm :	'OPTION';
*/
method 	:	          /* invitem | ackm | optionm | byem | cancelm | registerm |*/extension_method ;

extension_method  
	:	  token;

common_response returns [belle_sip_message_t* ret=NULL] 
  :   (   (sip_version  ~(CRLF)* CRLF) =>  status_line  {$ret=BELLE_SIP_MESSAGE($status_line.ret);}
      |   (http_version ~(CRLF)*  CRLF) =>  http_status_line{$ret=BELLE_SIP_MESSAGE($http_status_line.ret);}) 
  (message_header[BELLE_SIP_MESSAGE($ret)]+ last_crlf=CRLF {*($message_raw::message_length)=$last_crlf->user1;} /*message_body*/)?  ;

response  returns [belle_sip_response_t* ret=NULL]
	:	  common_response {$ret=BELLE_SIP_RESPONSE($common_response.ret);} ;

status_line   returns [belle_sip_response_t* ret=NULL]  
scope { belle_sip_response_t* current; }
@init {$ret = belle_sip_response_new(); }
	:	  sip_version 
	   SP status_code {belle_sip_response_set_status_code($ret,atoi((char*)$status_code.text->chars));}
	   SP reason_phrase {belle_sip_response_set_reason_phrase($ret,(char*)$reason_phrase.text->chars);}
	   CRLF ;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref( $ret);
   $ret=NULL;
} 

	
status_code     
	: extension_code;

extension_code  
	:	  DIGIT DIGIT DIGIT;
reason_phrase   
	:	  ~(CRLF)*;

//****************http**********************/ 

/*Request-Line   = Method SP Request-URI SP HTTP-Version CRLF*/ 
http_request returns [belle_http_request_t* ret=NULL] 
  :common_request {$ret=BELLE_HTTP_REQUEST($common_request.ret);}; 
  
http_request_line returns [belle_http_request_t *ret=NULL]
scope { belle_http_request_t* current; }
@init {$http_request_line::current = belle_http_request_new(); $ret=$http_request_line::current; }
  : method {belle_http_request_set_method($http_request_line::current,(const char*)($method.text->chars));} 
    SP 
    (generic_uri)=>generic_uri {belle_http_request_set_uri($http_request_line::current,$generic_uri.ret);}
    SP 
    http_version CRLF;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($http_request_line::current);
   $ret=NULL;
} 

http_version:  {IS_TOKEN(HTTP/)}? generic_version;

http_response  returns [belle_http_response_t* ret=NULL]
  :   common_response {$ret=BELLE_HTTP_RESPONSE($common_response.ret);} ;

http_status_line  returns [belle_http_response_t* ret]   
@init {$ret = belle_http_response_new();  }  
  :   http_version 
     SP status_code {belle_http_response_set_status_code($ret,atoi((char*)$status_code.text->chars));}
     SP reason_phrase {belle_http_response_set_reason_phrase($ret,(char*)$reason_phrase.text->chars);}
     CRLF ;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref( $ret);
   $ret=NULL;
}      


/*
  absoluteURI   = scheme ":" ( hier_part | opaque_part )
  hier_part     = ( net_path | abs_path ) [ "?" query ]
  net_path      = "//" authority [ abs_path ]
  abs_path      = "/"  path_segments
*/

/*comma, semicolon, or question
   mark*/

opaque_part_for_from_to_contact_addr_spec
	:  uric_no_slash_for_from_to_contact_addr_spec uric_for_from_to_contact_addr_spec*;
opaque_part
	:  uric_no_slash uric*;

uric_no_slash_for_from_to_contact_addr_spec	
	: unreserved | escaped  | COLON | AT| AND | EQUAL | PLUS | DOLLARD;	
uric_no_slash
	:COMMA | SEMI | QMARK  | uric_no_slash_for_from_to_contact_addr_spec ;
                  
 scheme: alpha ( alphanum | PLUS | DASH | DOT )*;

/*remove hiearachy part because complex to handle the :comma, semicolon, or question
   markexception see rfc3261 section 20.10 Contact*/
generic_uri_for_from_to_contact_addr_spec  returns [belle_generic_uri_t* ret=NULL]    
scope { belle_generic_uri_t* current; }
@init { $generic_uri_for_from_to_contact_addr_spec::current = $ret = belle_generic_uri_new(); }
   : scheme {belle_generic_uri_set_scheme($generic_uri_for_from_to_contact_addr_spec::current,(const char*)$scheme.text->chars);}  
   COLON  opaque_part_for_from_to_contact_addr_spec  {belle_generic_uri_set_opaque_part($generic_uri_for_from_to_contact_addr_spec::current,(const char*)$opaque_part_for_from_to_contact_addr_spec.text->chars) ;} ;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($generic_uri::current);
   $ret=NULL;
}



generic_uri  returns [belle_generic_uri_t* ret=NULL]    
scope { belle_generic_uri_t* current; }
@init { $generic_uri::current = $ret = belle_generic_uri_new(); }
   :  hier_part[$generic_uri::current]
      | (scheme {belle_generic_uri_set_scheme($generic_uri::current,(const char*)$scheme.text->chars);}  
      	COLON  (opaque_part {belle_generic_uri_set_opaque_part($generic_uri::current,(const char*)$opaque_part.text->chars) ;}
      		| hier_part[$generic_uri::current]) ) ;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($generic_uri::current);
   $ret=NULL;
}   
   

hier_part[belle_generic_uri_t* uri] returns [belle_generic_uri_t* ret=NULL]  
:   (
 (SLASH SLASH path_segments[NULL])=>( SLASH SLASH path_segments[uri])
  | 
  (SLASH SLASH authority[NULL] (path_segments[NULL])?)=>( SLASH SLASH authority[uri] (path_segments[uri])?) 
  | 
  ( path_segments[uri]) ) 
  (QMARK query 
            {
            char* unescaped_query;
            unescaped_query=belle_sip_to_unescaped_string((const char *)$query.text->chars);
            belle_generic_uri_set_query(uri,(const char*)unescaped_query);
            belle_sip_free(unescaped_query);
            }) ?;

path_segments[belle_generic_uri_t* uri]
  : SLASH (segment ( SLASH segment )*)
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

uric_for_from_to_contact_addr_spec: reserved_for_from_to_contact_addr_spec | unreserved | escaped;
                        
authority[belle_generic_uri_t* uri] 
:     
  ((authority_userinfo[NULL]) =>authority_userinfo[uri] )? authority_hostport[uri] 
  /*|  authority_hostport[uri]*/ ; 
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
}  

authority_userinfo[belle_generic_uri_t* uri] 
scope { belle_generic_uri_t* current; }
@init {$authority_userinfo::current=uri;}
       :  authority_user ( COLON authority_password )?  AT ;
authority_user            :   ( unreserved  | escaped | user_unreserved )+ {
                                                                  char* unescaped_username;
                                                                  unescaped_username=belle_sip_to_unescaped_string((const char *)$text->chars);
                                                                  belle_generic_uri_set_user($authority_userinfo::current,unescaped_username);
                                                                  belle_sip_free(unescaped_username);
                                                                  };

authority_password        :   ( unreserved | escaped | AND | EQUAL | PLUS | DOLLARD | COMMA )* {
                                                                              char* unescaped_userpasswd;
                                                                              const char* source = (const char*)$text->chars;
                                                                              if( source != NULL ){
                                                                                unescaped_userpasswd=belle_sip_to_unescaped_string((const char *)source);
                                                                                belle_generic_uri_set_user_password($authority_userinfo::current,unescaped_userpasswd);
                                                                                belle_sip_free(unescaped_userpasswd);
                                                                              }
                                                                              };
authority_hostport[belle_generic_uri_t* uri] 
scope { belle_generic_uri_t* current; }
@init {$authority_hostport::current=uri;}
        :   host ( COLON port {belle_generic_uri_set_port($authority_hostport::current,$port.ret);})? {belle_generic_uri_set_host($authority_hostport::current,$host.ret);};

//****************http end**********************/ 

//****************SIP**********************/	

generic_param [belle_sip_parameters_t* object]  returns [belle_sip_param_pair_t* ret=NULL]
scope{int is_value; char* gen_value_string;} 
@init { $generic_param::is_value=0; $generic_param::gen_value_string=NULL;}
  :    token (  equal gen_value {$generic_param::gen_value_string=(char*)($gen_value.text->chars);} )? {
                                                     if (object == NULL) {
                                                       $ret=belle_sip_param_pair_new((const char*)($token.text->chars)
                                                                                   ,$generic_param::gen_value_string);
                                                     } else {
                                                       belle_sip_parameters_set_parameter(object
                                                                                         ,(const char*)($token.text->chars)
                                                                                         ,$generic_param::gen_value_string);
                                                       $ret=NULL;
                                                     }
                                                     };
gen_value      
  :   token |  quoted_string;

quoted_string 
options { greedy = false; }
  : DQUOTE (~(DQUOTE | BSLASH) | (BSLASH .))*  DQUOTE ;

/*
accept_encoding  
  :   'Accept-Encoding' HCOLON ( encoding (COMMA encoding)* );
encoding         
  :   codings (SEMI accept_param)*;
codings          
  :   content_coding | '*';
content_coding   :  token;

accept_language  
  :   'Accept-Language' HCOLON
                     ( language (COMMA language)* )?;
language         
  :   language_range (SEMI accept_param)*;
language_range   
  :   ( ( alpha alpha alpha alpha alpha alpha alpha alpha ( '-' alpha alpha alpha alpha alpha alpha alpha alpha )* ) | '*' );

alert_info   
  :   'Alert-Info' HCOLON alert_param (COMMA alert_param)*;
  
alert_param  
  :   LAQUOT absoluteURI RAQUOT ( SEMI generic_param )*;

absoluteURI    
  :   token ':' token;
*/
header_allow returns [belle_sip_header_allow_t* ret]     
scope { belle_sip_header_allow_t* current; }
@init {$header_allow::current = belle_sip_header_allow_new(); $ret=$header_allow::current; }
      
:    {IS_TOKEN(Allow)}? token /*'Allow'*/ hcolon methods {belle_sip_header_allow_set_method($header_allow::current,(const char*)($methods.text->chars));} ;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_allow::current);
   $ret=NULL;
}
methods : /*LWS?*/ method (comma method)*;
authorization_token: {IS_TOKEN(Authorization)}? token;
digest_token: {IS_TOKEN(Digest)}? token;


header_authorization  returns [belle_sip_header_authorization_t* ret]     
scope { belle_sip_header_authorization_t* current; }
@init {$header_authorization::current = belle_sip_header_authorization_new(); $ret=$header_authorization::current; }
  :   authorization_token /*'Authorization'*/ hcolon credentials[$header_authorization::current];
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_authorization::current);
   $ret=NULL;
}

credentials  [belle_sip_header_authorization_t* header_authorization_base]     
  :   (digest_token /*'Digest'*/ {belle_sip_header_authorization_set_scheme(header_authorization_base,"Digest");} 
     lws digest_response[header_authorization_base])
                     | other_response[header_authorization_base];
digest_response  [belle_sip_header_authorization_t* header_authorization_base]  
  :   dig_resp[header_authorization_base] (comma dig_resp[header_authorization_base])*;
dig_resp  [belle_sip_header_authorization_t* header_authorization_base]         
  :   username { belle_sip_header_authorization_set_username(header_authorization_base,$username.ret);
                 belle_sip_free($username.ret);
               }
  | realm { belle_sip_header_authorization_set_realm(header_authorization_base,(char*)$realm.ret);
                 belle_sip_free($realm.ret);
           } 
  | nonce { belle_sip_header_authorization_set_nonce(header_authorization_base,(char*)$nonce.ret);
                 belle_sip_free($nonce.ret);
           }
  | digest_uri[header_authorization_base]
  | dresponse  { belle_sip_header_authorization_set_response(header_authorization_base,(char*)$dresponse.ret);
                 belle_sip_free($dresponse.ret);
               }
  | algorithm  {
                belle_sip_header_authorization_set_algorithm(header_authorization_base,(char*)$algorithm.ret);
               } 
  | cnonce{
            belle_sip_header_authorization_set_cnonce(header_authorization_base,(char*)$cnonce.ret);
            belle_sip_free($cnonce.ret);
           }
  | opaque {
            belle_sip_header_authorization_set_opaque(header_authorization_base,(char*)$opaque.ret);
            belle_sip_free($opaque.ret);
           }
  | message_qop{
            belle_sip_header_authorization_set_qop(header_authorization_base,$message_qop.ret);
           }
  | nonce_count{
            belle_sip_header_authorization_set_nonce_count(header_authorization_base,atoi((char*)$nonce_count.ret));
           } 
  | auth_param[header_authorization_base]
   ;
username_token: {IS_TOKEN(username)}? token;
username returns [char* ret=NULL]          
  :   username_token /*'username'*/ equal username_value {
                      $ret = _belle_sip_str_dup_and_unquote_string((char*)$username_value.text->chars);
                       };

username_value    :  quoted_string;

uri_token: {IS_TOKEN(uri)}? token;
digest_uri [belle_sip_header_authorization_t* header_authorization_base]        
  :   uri_token /*'uri'*/ equal DQUOTE uri DQUOTE  
  {belle_sip_header_authorization_set_uri(header_authorization_base,$uri.ret);
   };
/*
digest_uri_value  :  rquest_uri ;
rquest_uri
  : uri;
*/  
// Equal to request-uri as specified by HTTP/1.1
message_qop  returns [const char* ret=NULL]     
  :   {IS_TOKEN(qop)}? token/*'qop'*/ equal  qop_value {$ret = (const char*)$qop_value.text->chars;};

qop_value
  :  token;

cnonce returns [char* ret=NULL]            
  :   {IS_TOKEN(cnonce)}? token /*'cnonce'*/ equal cnonce_value {
                                              $ret = _belle_sip_str_dup_and_unquote_string((char*)$cnonce_value.text->chars);
                                              };
cnonce_value      
  :   nonce_value;
nonce_count returns [const char* ret=NULL]       
  :   {IS_TOKEN(nc)}? token /*'nc'*/ equal nc_value {$ret=(char*)$nc_value.text->chars;};
nc_value          
  :   huit_lhex; 
dresponse  returns [char* ret=NULL]       
  :   {IS_TOKEN(response)}? token /*'response'*/ equal request_digest{
                      $ret = _belle_sip_str_dup_and_unquote_string((char*)$request_digest.text->chars);
                     };
request_digest    
  :   quoted_string ;/*sp_laquot_sp huit_lhex huit_lhex huit_lhex huit_lhex sp_raquot_sp;
*/
huit_lhex
  : hexdigit+;

auth_param [belle_sip_header_authorization_t* header_authorization_base]        
  :   auth_param_name equal
                     auth_param_value {belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(header_authorization_base)
                                                                         ,(char*)$auth_param_name.text->chars
                                                                         ,(char*)$auth_param_value.text->chars);
                                       }
          ;
auth_param_value : token | quoted_string ;          
auth_param_name   
  :   token;
other_response [belle_sip_header_authorization_t* header_authorization_base]    
  :   auth_scheme {belle_sip_header_authorization_set_scheme(header_authorization_base,(const char*)$auth_scheme.text->chars);} 
     lws auth_param[header_authorization_base]
                     (comma auth_param[header_authorization_base])*;

auth_scheme       
  :   token;
/*
authentication_info  :  'Authentication-Info' HCOLON ainfo
                        (COMMA ainfo)*;
ainfo                
  :   nextnonce | message_qop
                         | response_auth | cnonce
                         | nonce_count;
nextnonce            
  :   'nextnonce' EQUAL nonce_value;
  */
nonce_value         :  quoted_string;
/*
response_auth        
  :   'rspauth' EQUAL response_digest;
response_digest      
  :   LDQUOT hexdigit* RDQUOT;
*/
/*callid header*/
call_id_token: {IS_HEADER_NAMED(Call-ID,i)}? token;

header_call_id  returns [belle_sip_header_call_id_t* ret=NULL]     
scope { belle_sip_header_call_id_t* current; }
@init {$header_call_id::current = belle_sip_header_call_id_new(); $ret=$header_call_id::current; }
  :  call_id_token /*( 'Call-ID' | 'i' )*/ hcolon call_id{belle_sip_header_call_id_set_call_id($header_call_id::current,(const char*) $call_id.text->chars); };
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_call_id::current);
   $ret=NULL;
}  
call_id   
  :   word ( AT word )? ;
/*
call_info   
  :   'Call-Info' HCOLON info (COMMA info)*;
info        
  :   LAQUOT absoluteURI RAQUOT ( SEMI info_param)*;
info_param  
  :   ( 'purpose' EQUAL ( 'icon' | 'info'
               | 'card' | token ) ) | generic_param;
*/
/* contact header */
contact_token: {IS_HEADER_NAMED(Contact,m)}? token;

header_contact      returns [belle_sip_header_contact_t* ret]   
scope { belle_sip_header_contact_t* current; belle_sip_header_contact_t* first; }
@init { $header_contact::current =NULL; $header_contact::first =NULL; $ret=NULL; }
  :   (contact_token /*'Contact'*/ /*| 'm'*/ ) sp_tab_colon
                  (  (lws? STAR) { $header_contact::current = belle_sip_header_contact_new();
                            belle_sip_header_contact_set_wildcard($header_contact::current,1);}
                  | (contact_param ( COMMA contact_param)*)) {$ret = $header_contact::first; };
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   $ret = $header_contact::first;
   if ($ret) belle_sip_object_unref($ret);
   $ret=NULL;
} 
               
contact_param 
scope { belle_sip_header_contact_t* prev;}
@init { if ($header_contact::current == NULL) {
            $header_contact::current = belle_sip_header_contact_new();
             $header_contact::first = $header_contact::current;
             $contact_param::prev=NULL; 
         } else {
            $contact_param::prev=$header_contact::current;
            belle_sip_header_set_next(BELLE_SIP_HEADER($header_contact::current),(belle_sip_header_t*)belle_sip_header_contact_new());
            $header_contact::current = (belle_sip_header_contact_t*)belle_sip_header_get_next(BELLE_SIP_HEADER($header_contact::current));
         } 
      }
  :   ( name_addr[BELLE_SIP_HEADER_ADDRESS($header_contact::current)] 
     | paramless_addr_spec[BELLE_SIP_HEADER_ADDRESS($header_contact::current)]) (SEMI lws? contact_params lws?)*;


header_address returns [belle_sip_header_address_t* ret]   
@init { $ret=NULL; }
  : header_address_base[belle_sip_header_address_new()] {$ret=$header_address_base.ret;}; 

header_address_base[belle_sip_header_address_t* obj]      returns [belle_sip_header_address_t* ret]   
@init { $ret=obj; }
     :  ( name_addr_with_generic_uri[BELLE_SIP_HEADER_ADDRESS($ret)]
| addr_spec_with_generic_uri[BELLE_SIP_HEADER_ADDRESS($ret)]) { if (!belle_sip_header_address_get_uri($ret)
																	&&
																	!belle_sip_header_address_get_absolute_uri(($ret))) {
																											belle_sip_object_unref($ret);
																											$ret=NULL;
																											}
																};
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
  belle_sip_object_unref($ret);
  $ret=NULL;
}      
name_addr[belle_sip_header_address_t* object]      
  :      (lws? display_name[object])? sp_laquot  addr_spec[object] raquot_sp;

name_addr_with_generic_uri[belle_sip_header_address_t* object]      
  :      (lws? display_name[object])? sp_laquot  addr_spec_with_generic_uri[object] raquot_sp;


addr_spec[belle_sip_header_address_t* object]      
  :  lws? uri {belle_sip_header_address_set_uri(object,$uri.ret);} lws?;//| absoluteURI;

addr_spec_with_generic_uri[belle_sip_header_address_t* object]      
  :  lws? (	uri {belle_sip_header_address_set_uri(object,$uri.ret);}  
  	| 
  	generic_uri { if (   strcasecmp(belle_generic_uri_get_scheme($generic_uri.ret),"sip") != 0
					 &&  strcasecmp(belle_generic_uri_get_scheme($generic_uri.ret),"sips") != 0 ) {
						 belle_sip_header_address_set_absolute_uri(object,$generic_uri.ret);
					 } else {
						 belle_sip_message("Cannot parse a sip/sips uri as a generic uri");
						 belle_sip_object_unref($generic_uri.ret);
					 }}
  	) lws?;
  
paramless_addr_spec[belle_sip_header_address_t* object]      
  :  lws? paramless_uri {belle_sip_header_address_set_uri(object,$paramless_uri.ret);} lws? ;//| absoluteURI;
  
paramless_addr_spec_with_generic_uri[belle_sip_header_address_t* object]      
  :  lws? (	paramless_uri {belle_sip_header_address_set_uri(object,$paramless_uri.ret);} 
  	| 
  	generic_uri_for_from_to_contact_addr_spec{belle_sip_header_address_set_absolute_uri(object,$generic_uri_for_from_to_contact_addr_spec.ret);}
  	) lws? ;
    
display_name_tokens 
	:token (lws token)* ;
display_name[belle_sip_header_address_t* object]  
  :  display_name_tokens {belle_sip_header_address_set_displayname(object,(const char*)($display_name_tokens.text->chars));}
     | quoted_string 
     	{
     	char* unescaped_char = belle_sip_string_to_backslash_less_unescaped_string((const char*)($quoted_string.text->chars));
     	belle_sip_header_address_set_quoted_displayname(object,(const char*)unescaped_char);
     	belle_sip_free(unescaped_char);
     	}
     ;

contact_params     
  :   /*c_p_q | c_p_expires
                      |*/ contact_extension;
/*c_p_q              
  :   'q' EQUAL qvalue;
c_p_expires        
  :   'expires' EQUAL delta_seconds;*/
contact_extension  
  :   generic_param [BELLE_SIP_PARAMETERS($header_contact::current)];

delta_seconds      
  :   DIGIT+;
/*

content_disposition   
  :   'Content-Disposition' HCOLON
                         disp_type ( SEMI disp_param )*;
disp_type             :  'render' | 'session' | 'icon' | 'alert'
                         | disp_extension_token;

disp_param            
  :   handling_param | generic_param;
handling_param        
  :   'handling' EQUAL
                         ( 'optional' | 'required'
                         | other_handling );
other_handling        
  :   token;
disp_extension_token  
  :   token;

content_encoding  
  :   ( 'Content-Encoding' | 'e' ) HCOLON
                     content_coding (COMMA content_coding)*;

content_language  
  :   'Content-Language' HCOLON
                     language_tag (COMMA language_tag)*;
language_tag      
  :   primary_tag ( '-' subtag )*;
primary_tag       
  :   huit_alpha;
subtag            
  :   huit_alpha;

huit_alpha
  : alpha+;
  ;
*/
/*content_length_token :  {strcmp("Content-Length",(const char*)(INPUT->toStringTT(INPUT,LT(1),LT(14)))->chars) == 0}? token;*/
content_length_token :  {IS_HEADER_NAMED(Content-Length,l)}? token;
header_content_length  returns [belle_sip_header_content_length_t* ret]     
scope { belle_sip_header_content_length_t* current; }
@init {$header_content_length::current = belle_sip_header_content_length_new(); $ret=$header_content_length::current; }
  :  content_length_token /*( 'Content-Length' | 'l' )*/ 
     hcolon 
     content_length {belle_sip_header_content_length_set_content_length($header_content_length::current,atoi((const char*)$content_length.text->chars));};
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_content_length::current);
   $ret=NULL;
}
content_length:DIGIT+;
 
content_type_token :  {IS_HEADER_NAMED(Content-Type,c)}? token;
header_content_type  returns [belle_sip_header_content_type_t* ret=NULL]   
scope { belle_sip_header_content_type_t* current;}
@init { $header_content_type::current = belle_sip_header_content_type_new();$ret=$header_content_type::current; }
  :  content_type_token/* ( 'Content-Type' | 'c' )*/ hcolon media_type;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
  belle_sip_object_unref($header_content_type::current);
   $ret=NULL;
}   
media_type       
  :  m_type {belle_sip_header_content_type_set_type($header_content_type::current,(const char*)$m_type.text->chars);} 
     slash 
     m_subtype {belle_sip_header_content_type_set_subtype($header_content_type::current,(const char*)$m_subtype.text->chars);} 
     (semi type_param)? (semi generic_param [BELLE_SIP_PARAMETERS($header_content_type::current)])*;
m_type           
  : token;

type_param: {IS_TOKEN(type)}? token equal type_param_value;
type_param_value : m_type slash m_subtype {belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS($header_content_type::current)
																								,"type"
																								,(const char*)$type_param_value.text->chars);};

/*  discrete_type | composite_type;
discrete_type    
  :   'text' | 'image' | 'audio' | 'video'
                    | 'application' | extension_token;
composite_type   
  :   'message' | 'multipart' | extension_token;
extension_token  
  :   ietf_token | x_token;
ietf_token       
  :   token;
x_token          
  :   'x-' token;*/
m_subtype        : token ;/* extension_token | iana_token;
iana_token       
  :   token;
m_parameter      
  :   m_attribute EQUAL m_value;
m_attribute      
  :   token;
m_value          
  :   token | quoted_string;
*/

cseq_token :  {IS_TOKEN(CSeq)}? token;
header_cseq  returns [belle_sip_header_cseq_t* ret]   
scope { belle_sip_header_cseq_t* current; }
@init { $header_cseq::current = belle_sip_header_cseq_new();$ret = $header_cseq::current; }
  : cseq_token
    hcolon 
    seq_number {belle_sip_header_cseq_set_seq_number($header_cseq::current,atoi((const char*)$seq_number.text->chars));} 
    lws 
    method {belle_sip_header_cseq_set_method($header_cseq::current,(const char*)$method.text->chars);} ;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_cseq::current);
   $ret=NULL;
}     
seq_number:DIGIT+;

/*Date header*/
date_token: {IS_TOKEN(Date)}? token;

header_date  returns [belle_sip_header_date_t* ret]     
scope { belle_sip_header_date_t* current; }
@init {$header_date::current = belle_sip_header_date_new(); $ret=$header_date::current; }
  :  date_token /*( 'Date' )*/ hcolon sip_date{belle_sip_header_date_set_date($header_date::current,(const char*) $sip_date.text->chars); };
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_date::current);
   $ret=NULL;
}

date
  : sip_date;

sip_date
  :  /*wkday*/alpha alpha alpha COMMA SP /*date1*/DIGIT DIGIT SP alpha alpha alpha SP DIGIT DIGIT DIGIT DIGIT SP /*time*/DIGIT DIGIT  COLON DIGIT DIGIT  COLON DIGIT DIGIT SP /*GMTT*/alpha alpha alpha;




/*
//  rfc1123-date
//rfc1123-date  =  wkday "," SP date1 SP time SP "GMT"
//date1         =  2DIGIT SP month SP 4DIGIT
//                 ; day month year (e.g., 02 Jun 1982)
//time          =  2DIGIT ":" 2DIGIT ":" 2DIGIT
//                 ; 00:00:00 - 23:59:59
//wkday         =  "Mon" / "Tue" / "Wed"
//                 / "Thu" / "Fri" / "Sat" / "Sun"
//month         =  "Jan" / "Feb" / "Mar" / "Apr"
//                 / "May" / "Jun" / "Jul" / "Aug"
//                 / "Sep" / "Oct" / "Nov" / "Dec"

error_info  
  :   'Error-Info' HCOLON error_uri (COMMA error_uri)*;

error_uri   
  :   LAQUOT absoluteURI RAQUOT ( SEMI generic_param )*;
*/
header_expires returns [belle_sip_header_expires_t* ret]   
scope { belle_sip_header_expires_t* current; }
@init { $header_expires::current = belle_sip_header_expires_new();$ret = $header_expires::current; }     
  :   {IS_TOKEN(Expires)}? token /*'Expires'*/ hcolon delta_seconds {belle_sip_header_expires_set_expires($header_expires::current,atoi((const char *)$delta_seconds.text->chars));};
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_expires::current);
   $ret=NULL;
}   

from_token:  {IS_HEADER_NAMED(From,f)}? token;
header_from  returns [belle_sip_header_from_t* ret]   
scope { belle_sip_header_from_t* current; }
@init { $header_from::current = belle_sip_header_from_new();$ret = $header_from::current; }
        
  :   from_token/* ( 'From' | 'f' )*/ sp_tab_colon from_spec ;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_from::current);
   $ret=NULL;
} 
  
from_spec   
  :   ( name_addr_with_generic_uri[BELLE_SIP_HEADER_ADDRESS($header_from::current)] | paramless_addr_spec_with_generic_uri[BELLE_SIP_HEADER_ADDRESS($header_from::current)] )
               ( SEMI lws? from_param lws?)*;
from_param  
  :   /*tag_param |*/ generic_param [BELLE_SIP_PARAMETERS($header_from::current)];
                       
/*
tag_param   
  :   'tag' EQUAL token;
*/
/*
in_reply_to  
  :   'In-Reply-To' HCOLON callid (COMMA callid);
*/
header_max_forwards  returns [belle_sip_header_max_forwards_t* ret]   
scope { belle_sip_header_max_forwards_t* current; }
@init { $header_max_forwards::current = belle_sip_header_max_forwards_new();$ret = $header_max_forwards::current; } 
  :   {IS_TOKEN(Max-Forwards)}? token /*'Max-Forwards'*/ hcolon 
    max_forwards {belle_sip_header_max_forwards_set_max_forwards($header_max_forwards::current,atoi((const char*)$max_forwards.text->chars));};
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_max_forwards::current);
   $ret=NULL;
} 
max_forwards:DIGIT+;
/*
mime_version  
  :   'MIME-Version' HCOLON DIGIT+ '.' DIGIT+;

min_expires  
  :   'Min-Expires' HCOLON delta_seconds;

organization  
  :   'Organization' HCOLON (text_utf8_trim)?;

text_utf8_trim
  :
  ;

priority        
  :   'Priority' HCOLON priority_value;
priority_value  
  :   'emergency' | 'urgent' | 'normal'
                   | 'non-urgent' | other_priority;
other_priority  
  :   token;
*/
header_proxy_authenticate   returns [belle_sip_header_proxy_authenticate_t* ret]   
scope { belle_sip_header_proxy_authenticate_t* current; }
@init { $header_proxy_authenticate::current = belle_sip_header_proxy_authenticate_new();$ret = $header_proxy_authenticate::current; } 
  :   {IS_TOKEN(Proxy-Authenticate)}? token /*'Proxy-Authenticate'*/ 
  hcolon challenge[BELLE_SIP_HEADER_WWW_AUTHENTICATE($header_proxy_authenticate::current)];
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_proxy_authenticate::current);
   $ret=NULL;
} 

challenge [belle_sip_header_www_authenticate_t* www_authenticate]           
  :   ({IS_TOKEN(Digest)}? token /*'Digest'*/ {belle_sip_header_www_authenticate_set_scheme(www_authenticate,"Digest");} 
     lws digest_cln[www_authenticate] (comma digest_cln[www_authenticate])*)
                       | other_challenge [www_authenticate];
other_challenge [belle_sip_header_www_authenticate_t* www_authenticate]    
  :   auth_scheme {belle_sip_header_www_authenticate_set_scheme(www_authenticate,(char*)$auth_scheme.text->chars);} 
     lws auth_param[(belle_sip_header_authorization_t*)www_authenticate]
                       (comma auth_param[(belle_sip_header_authorization_t*)www_authenticate])*;
digest_cln [belle_sip_header_www_authenticate_t* www_authenticate]         
  : 
   realm {belle_sip_header_www_authenticate_set_realm(www_authenticate,(char*)$realm.ret);
           belle_sip_free($realm.ret);} 
  | nonce {belle_sip_header_www_authenticate_set_nonce(www_authenticate,(char*)$nonce.ret);
           belle_sip_free($nonce.ret);}
  | algorithm {belle_sip_header_www_authenticate_set_algorithm(www_authenticate,$algorithm.ret);} 
  | opaque  {belle_sip_header_www_authenticate_set_opaque(www_authenticate,$opaque.ret);
             belle_sip_free($opaque.ret);}
  | qop_opts {belle_sip_header_www_authenticate_set_qop(www_authenticate,$qop_opts.ret);
              /*belle_sip_free($qop_opts.ret);*/}
  | domain {belle_sip_header_www_authenticate_set_domain(www_authenticate,$domain.ret);
             belle_sip_free($domain.ret);} 
  | stale { if (strcmp("true",$stale.ret)==0) {
               belle_sip_header_www_authenticate_set_stale(www_authenticate,1);
             }
          }
     | auth_param[(belle_sip_header_authorization_t*)www_authenticate];
/* the cast above is very BAD, but auth_param works on fields common to the two structures*/

realm returns [char* ret=NULL]              
  :   {IS_TOKEN(realm)}? token /*'realm'*/ equal realm_value {
                      $ret = _belle_sip_str_dup_and_unquote_string((char*)$realm_value.text->chars);
                       };
realm_value         
  :   quoted_string ;

domain returns [char* ret=NULL]              
  :   {IS_TOKEN(domain)}? token /*'domain'*/ equal quoted_string {
                      $ret = _belle_sip_str_dup_and_unquote_string((char*)$quoted_string.text->chars);};
  /* LDQUOT uri
                       ( SP+ uri )* RDQUOT;
uri                 
  :   absoluteURI | '/'.;
*/
nonce  returns [char* ret=NULL]             
  :   {IS_TOKEN(nonce)}? token /*'nonce'*/ equal nonce_value{
                      $ret = _belle_sip_str_dup_and_unquote_string((char*)$nonce_value.text->chars);
                       };
opaque returns [char* ret=NULL]             
  :   {IS_TOKEN(opaque)}? token /*'opaque'*/ equal quoted_string{
                      $ret = _belle_sip_str_dup_and_unquote_string((char*)$quoted_string.text->chars);
                       };

stale  returns [const char* ret=NULL]             
  :   {IS_TOKEN(stale)}? token /*'stale'*/ equal stale_value {$ret=(char*)$stale_value.text->chars;} /* ( 'true' | 'false' )*/;

stale_value:token;

algorithm returns [const char* ret=NULL]           
  :   {IS_TOKEN(algorithm)}? token /*'algorithm'*/ equal /* ( 'MD5' | 'MD5-sess'
                       |*/ alg_value=token {$ret=(char*)$alg_value.text->chars;}/*)*/
  ;

qop_opts returns [belle_sip_list_t* ret=NULL]        
scope { belle_sip_list_t* list; }
@init{$qop_opts::list=NULL;}
  :   {IS_TOKEN(qop)}? token /*'qop'*/ equal 
  /*ldquot*/DQUOTE 
  qop_opts_value 
  (COMMA qop_opts_value)* 
  /*rdquot*/DQUOTE {$ret=$qop_opts::list;} ;
  
qop_opts_value  
: lws? token lws? {$qop_opts::list=belle_sip_list_append($qop_opts::list,belle_sip_strdup((const char*)$token.text->chars));};

header_proxy_authorization  returns [belle_sip_header_proxy_authorization_t* ret=NULL]
scope { belle_sip_header_proxy_authorization_t* current; }
@init { $header_proxy_authorization::current = belle_sip_header_proxy_authorization_new();$ret = $header_proxy_authorization::current; }
  :   {IS_TOKEN(Proxy-Authorization)}? token /*'Proxy-Authorization'*/ hcolon credentials[(belle_sip_header_authorization_t*)$header_proxy_authorization::current];
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_proxy_authorization::current);
   $ret=NULL;
}

/*
proxy_require  
  :   'Proxy-Require' HCOLON option_tag
                  (COMMA option_tag)*;
option_tag     
  :   token;
*/
/*FIXME service-route = recorde-route = route, too many copy/past*/
service_route_token:  {IS_TOKEN(Service-Route)}? token;
header_service_route  returns [belle_sip_header_service_route_t* ret=NULL]   
scope { belle_sip_header_service_route_t* current; belle_sip_header_service_route_t* first;}
@init { $header_service_route::current = NULL;$header_service_route::first =NULL;}
  :   service_route_token /*'Service-Route'*/ sp_tab_colon srv_route (COMMA srv_route)* {$ret = $header_service_route::first;};
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   $ret = $header_service_route::first;
   if ($ret) belle_sip_object_unref($ret);
   $ret=NULL;
}
srv_route
scope { belle_sip_header_service_route_t* prev;}
@init { if ($header_service_route::current == NULL) {
            $header_service_route::first = $header_service_route::current = belle_sip_header_service_route_new();
            $srv_route::prev=NULL;
         } else {
            belle_sip_header_t* header = BELLE_SIP_HEADER($header_service_route::current); 
            $srv_route::prev=$header_service_route::current;
            belle_sip_header_set_next(header,(belle_sip_header_t*)($header_service_route::current = belle_sip_header_service_route_new()));
         } 
      }     
   :   name_addr[BELLE_SIP_HEADER_ADDRESS($header_service_route::current)] ( SEMI lws? sr_param lws?)*;

  
sr_param      
  :   generic_param[BELLE_SIP_PARAMETERS($header_service_route::current)];
  
record_route_token:  {IS_TOKEN(Record-Route)}? token;
header_record_route  returns [belle_sip_header_record_route_t* ret=NULL]   
scope { belle_sip_header_record_route_t* current; belle_sip_header_record_route_t* first;}
@init { $header_record_route::current = NULL;$header_record_route::first =NULL;}
  :   record_route_token /*'Record-Route'*/ sp_tab_colon rec_route ( COMMA rec_route)* {$ret = $header_record_route::first;};
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   $ret = $header_record_route::first;
   if ($ret) belle_sip_object_unref($ret);
   $ret=NULL;
}
rec_route
scope { belle_sip_header_record_route_t* prev;}
@init { if ($header_record_route::current == NULL) {
            $header_record_route::first = $header_record_route::current = belle_sip_header_record_route_new();
            $rec_route::prev=NULL;
         } else {
            belle_sip_header_t* header = BELLE_SIP_HEADER($header_record_route::current); 
            $rec_route::prev=$header_record_route::current;
            belle_sip_header_set_next(header,(belle_sip_header_t*)($header_record_route::current = belle_sip_header_record_route_new()));
         } 
      }     
  :   name_addr[BELLE_SIP_HEADER_ADDRESS($header_record_route::current)] ( SEMI lws? rr_param lws? )*;
  
rr_param      
  :   generic_param[BELLE_SIP_PARAMETERS($header_record_route::current)];
/*
reply_to      
  :   'Reply-To' HCOLON rplyto_spec;
rplyto_spec   
  :   ( name_addr | addr_spec )
                 ( SEMI rplyto_param )*;
rplyto_param  
  :   generic_param;
require       
  :   'Require' HCOLON option_tag (COMMA option_tag)*;

retry_after  
  :   'Retry-After' HCOLON delta_seconds
                 comment? ( SEMI retry_param )*;
*/
comment : LPAREN . RPAREN;
  
/*
retry_param  
  :   ('duration' EQUAL delta_seconds)
                | generic_param;
*/
route_token:  {IS_TOKEN(Route)}? token;
header_route  returns [belle_sip_header_route_t* ret=NULL]   
scope { belle_sip_header_route_t* current;belle_sip_header_route_t* first; }
@init { $header_route::current = NULL; $header_route::first =NULL;}
  :   route_token /*'Route'*/ sp_tab_colon route_param ( COMMA route_param)*{$ret = $header_route::first;};
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   $ret = $header_route::first;
   if ($ret) belle_sip_object_unref($ret);
   $ret=NULL;
}
route_param 
scope { belle_sip_header_route_t* prev;}
@init { if ($header_route::current == NULL) {
            $header_route::first = $header_route::current = belle_sip_header_route_new();
             $route_param::prev=NULL;
         } else {
            belle_sip_header_t* header = BELLE_SIP_HEADER($header_route::current); 
            $route_param::prev=$header_route::current;
            belle_sip_header_set_next(header,(belle_sip_header_t*)($header_route::current = belle_sip_header_route_new()));
         } 
      }      
  :   name_addr[BELLE_SIP_HEADER_ADDRESS($header_route::current)] ( SEMI lws? r_param lws?)*;

r_param      
  :   generic_param[BELLE_SIP_PARAMETERS($header_route::current)];
/*
server           
  :   'Server' HCOLON server_val (lws server_val)*;
*/

/*
subject  
  :   ( 'Subject' | 's' ) HCOLON (text_utf_huit)?;

text_utf_huit
  :
  ;

supported  
  :   ( 'Supported' | 'k' ) HCOLON
              (option_tag (COMMA option_tag)*)?;

timestamp  
  :   'Timestamp' HCOLON (DIGIT)+
               ( '.' (DIGIT)* )? ( lws delay )?;
delay      
  :   (DIGIT)* ( '.' (DIGIT)* )?;
*/
to_token:  {IS_HEADER_NAMED(To,t)}? token;
header_to  returns [belle_sip_header_to_t* ret=NULL]   
scope { belle_sip_header_to_t* current; }
@init { $header_to::current = belle_sip_header_to_new(); $ret = $header_to::current;}
        
  :   to_token /*'To' ( 'To' | 't' )*/ sp_tab_colon to_spec;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_to::current);
   $ret=NULL;
}  
to_spec   
  :   ( name_addr_with_generic_uri[BELLE_SIP_HEADER_ADDRESS($header_to::current)] | paramless_addr_spec_with_generic_uri[BELLE_SIP_HEADER_ADDRESS($header_to::current)] )
               ( SEMI lws? to_param lws?)*;
to_param  
  :   /*tag_param |*/ generic_param [BELLE_SIP_PARAMETERS($header_to::current)];

diversion_token: {IS_HEADER_NAMED(Diversion,d)}? token;
header_diversion returns [belle_sip_header_diversion_t* ret=NULL]
scope { belle_sip_header_diversion_t* current; }
@init { $header_diversion::current = belle_sip_header_diversion_new(); $ret = $header_diversion::current;}

  :   diversion_token /*'Diversion' ( 'Diversion' | 'd' )*/ sp_tab_colon diversion_spec;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_diversion::current);
   $ret=NULL;
}
diversion_spec
  :   ( name_addr_with_generic_uri[BELLE_SIP_HEADER_ADDRESS($header_diversion::current)] | paramless_addr_spec_with_generic_uri[BELLE_SIP_HEADER_ADDRESS($header_diversion::current)] )
               ( SEMI lws? diversion_param lws?)*;
diversion_param
  :   /*tag_param |*/ generic_param [BELLE_SIP_PARAMETERS($header_diversion::current)];

refer_to_token:  {IS_TOKEN(Refer-To)}? token;
header_refer_to  returns [belle_sip_header_refer_to_t* ret=NULL]   
  :   refer_to_token /*'Refer-To'*/ 
      sp_tab_colon 
      refer_to_spec[BELLE_SIP_HEADER_ADDRESS(belle_sip_header_refer_to_new())] {$ret = BELLE_SIP_HEADER_REFER_TO($refer_to_spec.ret);};

referred_by_token:  {IS_TOKEN(Referred-By)}? token;
header_referred_by  returns [belle_sip_header_referred_by_t* ret=NULL]   
  :   referred_by_token /*'Referred-By'*/ 
      sp_tab_colon 
      refer_to_spec[BELLE_SIP_HEADER_ADDRESS(belle_sip_header_referred_by_new())] {$ret = BELLE_SIP_HEADER_REFERRED_BY($refer_to_spec.ret);};
  
refer_to_spec [belle_sip_header_address_t* address]    returns [belle_sip_header_address_t* ret]
@init {$ret=address;}
  :   (( name_addr[address] | paramless_addr_spec[address])  ( SEMI lws? generic_param [BELLE_SIP_PARAMETERS(address)] lws? )* );
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref(address);
   $ret=NULL;
} 

    
/*
unsupported  
  :   'Unsupported' HCOLON option_tag (COMMA option_tag)*;
*/
header_user_agent  returns [belle_sip_header_user_agent_t* ret]   
scope { belle_sip_header_user_agent_t* current; }
@init { $header_user_agent::current = belle_sip_header_user_agent_new();$ret = $header_user_agent::current;}
  :   {IS_TOKEN(User-Agent)}? token /*'User-Agent'*/ hcolon server_val (lws server_val)*;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_user_agent::current);
   $ret=NULL;
} 

server_val    : word {belle_sip_header_user_agent_add_product($header_user_agent::current,(const char*)$word.text->chars); };
/*serval_item    
  :   product | comment  ;
product          
  :   token (SLASH product_version)?;
product_version  
  :   token;
  */
via_token:  {IS_HEADER_NAMED(Via,v)}? token;
header_via  returns [belle_sip_header_via_t* ret]   
scope { belle_sip_header_via_t* current; belle_sip_header_via_t* first; }
@init { $header_via::current = NULL;$header_via::first =NULL;$ret = NULL;}
        
  :   via_token/* ( 'via' | 'v' )*/ hcolon via_parm (comma via_parm)* {$ret = $header_via::first;} ;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   $ret = $header_via::first;
   if ($ret) belle_sip_object_unref($ret);
   $ret=NULL;
}
via_parm
scope { belle_sip_header_via_t* prev;}
@init { if ($header_via::current == NULL) {
            $header_via::first = $header_via::current = belle_sip_header_via_new();
            $via_parm::prev=NULL;
         } else {
      belle_sip_header_t* header;
            $via_parm::prev=$header_via::current;
            header = BELLE_SIP_HEADER($header_via::current); 
            belle_sip_header_set_next(header,(belle_sip_header_t*)($header_via::current = belle_sip_header_via_new()));
         } 
      }          
  :   sent_protocol  lws sent_by ( semi via_params )*;
  
via_params        
  :   /*via_ttl | via_maddr
                     |  via_branch
                     | via_extension */ via_received[$header_via::current] | generic_param [BELLE_SIP_PARAMETERS($header_via::current)];
/*via_ttl           
  :   'ttl' EQUAL ttl;
via_maddr         
  :   'maddr' EQUAL host;*/
via_received [belle_sip_header_via_t* object] 
  : {IS_TOKEN(received)}? token EQUAL via_address {belle_sip_header_via_set_received(object,(const char*)$via_address.text->chars);};

via_address: ipv4address | ipv6address;
/*
via_branch        
  :   'branch' EQUAL token;
via_extension     
  :   generic_param;*/
sent_protocol     
  :   (protocol_name slash protocol_version) {belle_sip_header_via_set_protocol($header_via::current,(const char*)$text->chars);}
                     slash transport {belle_sip_header_via_set_transport($header_via::current,(const char*)$transport.text->chars);} ;
protocol_name     
  :   /*'SIP' |*/ token;
protocol_version  
  :   token;
transport         
  :  /* 'UDP' | 'TCP' | 'TLS' | 'SCTP'
                     | */ other_transport;
other_transport
  : token;

sent_by           
  :   host {belle_sip_header_via_set_host($header_via::current,$host.ret);}
     ( COLON port {belle_sip_header_via_set_port($header_via::current,$port.ret);} )? ;


/*
warning        
  :   'Warning' HCOLON warning_value (COMMA warning_value)*;
warning_value  
  :   warn_code SP warn_agent SP warn_text;
warn_code      
  :   DIGIT DIGIT DIGIT;
warn_agent     
  :   hostport | pseudonym;
                  //  the name or pseudonym of the server adding
                  //  the Warning header, for use in debugging
warn_text      
  :   quoted_string;
pseudonym      
  :   token;
*/
header_www_authenticate returns [belle_sip_header_www_authenticate_t* ret]   
scope { belle_sip_header_www_authenticate_t* current; }
@init { $header_www_authenticate::current = belle_sip_header_www_authenticate_new();$ret = $header_www_authenticate::current; } 
  :   {IS_TOKEN(WWW-Authenticate)}? token /*'WWW-Authenticate'*/ hcolon challenge[$header_www_authenticate::current];
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_www_authenticate::current);
   $ret=NULL;
}
state_value: token ;

header_subscription_state  returns [belle_sip_header_subscription_state_t* ret] 
scope { belle_sip_header_subscription_state_t* current; }
@init { $header_subscription_state::current = belle_sip_header_subscription_state_new();$ret = $header_subscription_state::current; } 
 : {IS_TOKEN(Subscription-State)}? token /*"Subscription-State"*/ 
 hcolon state_value {belle_sip_header_subscription_state_set_state($header_subscription_state::current,(const char*)$state_value.text->chars);} 
 (semi  generic_param [BELLE_SIP_PARAMETERS($header_subscription_state::current)])* ;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_subscription_state::current);
   $ret=NULL;
}

header_event  returns [belle_sip_header_event_t* ret] 
scope { belle_sip_header_event_t* current; }
@init { $header_event::current = belle_sip_header_event_new();$ret = $header_event::current; } 
 : {IS_TOKEN(Event)}? token /*"Event"*/ 
 hcolon event_package {belle_sip_header_event_set_package_name($header_event::current,(const char*)$event_package.text->chars);} 
 (semi  generic_param [BELLE_SIP_PARAMETERS($header_event::current)])* ;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_event::current);
   $ret=NULL;
}
event_package
	:	 token;

/*
Replaces         = "Replaces" HCOLON replaces-values
                          *(COMMA replaces-values)

      replaces-values  = callid *( SEMI replaces-param )
      callid           = token [ "@" token ]
      replaces-param   = to-tag | from-tag | extension-param
      to-tag           = "to-tag" EQUAL ( UUID | "*" )
      from-tag         = "from-tag" EQUAL UUID
      extension-param  = token [ EQUAL ( token | quoted-string ) ]
*/    
header_replaces  returns [belle_sip_header_replaces_t* ret] 
scope { belle_sip_header_replaces_t* current; }
@init { $header_replaces::current = belle_sip_header_replaces_new();$ret = $header_replaces::current; } 
 : {IS_TOKEN(Replaces)}? token /*"Replaces"*/ 
 hcolon call_id {belle_sip_header_replaces_set_call_id($header_replaces::current,(const char*)$call_id.text->chars);} 
  (semi  generic_param [BELLE_SIP_PARAMETERS($header_replaces::current)])* ;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($header_replaces::current);
   $ret=NULL;
}

//**********************************Privacy*******************************//


header_p_preferred_identity returns [belle_sip_header_p_preferred_identity_t* ret]   
@init { $ret=NULL; }
  :  {IS_TOKEN(P-Preferred-Identity)}? token /*"P-Preferred-Identity"*/ 
 sp_tab_colon header_address_base[(belle_sip_header_address_t*)belle_sip_header_p_preferred_identity_new()] {$ret=(belle_sip_header_p_preferred_identity_t*)$header_address_base.ret;}; 
  
header_privacy  returns [belle_sip_header_privacy_t* ret]   
scope { belle_sip_header_privacy_t* current; }
@init { $header_privacy::current = belle_sip_header_privacy_new();$ret = $header_privacy::current;}
  :   {IS_TOKEN(Privacy)}? token /*'Privacy'*/ hcolon privacy_val (semi privacy_val)*;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($ret);
   $ret=NULL;
} 
privacy_val: token {belle_sip_header_privacy_add_privacy($header_privacy::current,(const char*)$token.text->chars);};

//**********************************Supported*******************************//
header_supported  returns [belle_sip_header_supported_t* ret]
scope { belle_sip_header_supported_t* current; }
@init { $header_supported::current = belle_sip_header_supported_new();$ret = $header_supported::current;}
:   {IS_TOKEN(Supported)}? token /*'Supported'*/ hcolon supported_val (comma supported_val)*;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
	belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
	belle_sip_object_unref($ret);
	$ret=NULL;
}
supported_val: token {belle_sip_header_supported_add_supported($header_supported::current,(const char*)$token.text->chars);};

//**********************************Content-Disposition*******************************//

content_disposition_value: token ;

header_content_disposition  returns [belle_sip_header_content_disposition_t* ret]
scope { belle_sip_header_content_disposition_t* current; }
@init { $header_content_disposition::current = belle_sip_header_content_disposition_new();$ret = $header_content_disposition::current; }
: {IS_TOKEN(Content-Disposition)}? token /*"Content-Disposition"*/
hcolon content_disposition_value {belle_sip_header_content_disposition_set_content_disposition($header_content_disposition::current,(const char*)$content_disposition_value.text->chars);}
(semi  generic_param [BELLE_SIP_PARAMETERS($header_content_disposition::current)])* ;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
	belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
	belle_sip_object_unref($header_content_disposition::current);
	$ret=NULL;
}
//**********************************Accept*******************************//


accept_token :  {IS_TOKEN(Accept)}? token;
header_accept  returns [belle_sip_header_accept_t* ret=NULL]
scope { belle_sip_header_accept_t* current; belle_sip_header_accept_t* first; }
@init { $header_accept::current = NULL;$header_accept::first =NULL;$ret = NULL;}
:  accept_token/* ( 'Accept')*/ hcolon accept_param (comma accept_param)* {$ret = $header_accept::first;} ;
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
	belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
	$ret = $header_accept::first;
	if ($ret) belle_sip_object_unref($ret);
	$ret=NULL;
}

accept_param
scope { belle_sip_header_accept_t* prev;}
@init { if ($header_accept::current == NULL) {
	$header_accept::first = $header_accept::current = belle_sip_header_accept_new();
	$accept_param::prev=NULL;
} else {
	belle_sip_header_t* header;
	$accept_param::prev=$header_accept::current;
	header = BELLE_SIP_HEADER($header_accept::current);
	belle_sip_header_set_next(header,(belle_sip_header_t*)($header_accept::current = belle_sip_header_accept_new()));
}
}
:  accept_main_media_type {belle_sip_header_accept_set_type($header_accept::current,(const char*)$accept_main_media_type.text->chars);}
slash
accept_sub_media_type {belle_sip_header_accept_set_subtype($header_accept::current,(const char*)$accept_sub_media_type.text->chars);}
(semi  generic_param [BELLE_SIP_PARAMETERS($header_accept::current)])*;

accept_main_media_type: token;
accept_sub_media_type: token;


//*********************************************//
header  returns [belle_sip_header_t* ret=NULL]
	 : header_extension_base[FALSE] {$ret=$header_extension_base.ret;};
	
//********************************************************************************************//
header_extension_base[ANTLR3_BOOLEAN is_http]  returns [belle_sip_header_t* ret]
scope {int as_value;}
@init {$header_extension_base::as_value=0;$ret=NULL;}  
  :    (header_name 
       hcolon /*sp_tab_colon*/ /*because LWS can be in both colon or header_value*/ 
       (header_value[(const char*)$header_name.text->chars,$is_http ]{$header_extension_base::as_value=1;$ret=$header_value.ret;})?)  {
       	if (!$ret && !$header_extension_base::as_value) { /*to handle value parsing error*/
       	/*special case: header without value*/
       		$ret=belle_sip_header_create((const char*)$header_name.text->chars,NULL);
       	}
       };
                   
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   if ($ret) belle_sip_object_unref($ret);
   $ret=NULL;
}                   
header_name       
  :   token;

header_value[const char* name, ANTLR3_BOOLEAN is_http] returns [belle_sip_header_t* ret] 
options { greedy = false; }      
 @init {$ret=NULL;}  
  :  (~(SP|CRLF) ((CRLF SP) | ~CRLF)* )  {
                    if ($is_http) {
                    	$ret=belle_http_header_create($name,(const char*)$header_value.text->chars);
                    }else {
                    	$ret=belle_sip_header_create($name,(const char*)$header_value.text->chars);
                    }
                   } ; 
  
message_body
options { greedy = false; }  
  :   OCTET+;

paramless_uri  returns [belle_sip_uri_t* ret=NULL]    
scope { belle_sip_uri_t* current; }
@init { $paramless_uri::current = belle_sip_uri_new(); }
   :  sip_schema[$paramless_uri::current] ( (userinfo[$paramless_uri::current]) =>(userinfo[$paramless_uri::current] hostport[$paramless_uri::current]) | hostport[$paramless_uri::current])
   headers[$paramless_uri::current]? {$ret = $paramless_uri::current;}; 
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($paramless_uri::current);
   $ret=NULL;
}

uri  returns [belle_sip_uri_t* ret=NULL]    
scope { belle_sip_uri_t* current; }
@init { $uri::current = belle_sip_uri_new(); }
   :  sip_schema[$uri::current] ( ((userinfo[NULL])=>userinfo[$uri::current] hostport[$uri::current]) | hostport[$uri::current] ) 
   uri_parameters[$uri::current]? 
   headers[$uri::current]? {$ret = $uri::current;}; 
catch [ANTLR3_RECOGNITION_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($uri::current);
   $ret=NULL;
}

sip_token:  {IS_TOKEN(sip)}? token;
sips_token:  {IS_TOKEN(sips)}? token;

sip_schema[belle_sip_uri_t* uri]  : (sips_token {belle_sip_uri_set_secure(uri,1);}
            | sip_token) COLON ;
userinfo[belle_sip_uri_t* uri] 
scope { belle_sip_uri_t* current; }
@init {$userinfo::current=uri;}
       :  user ( COLON password )?  AT;
user            :   ( unreserved  | escaped | user_unreserved )+ {
                                                                  char* unescaped_username;
                                                                  unescaped_username=belle_sip_to_unescaped_string((const char *)$text->chars);
                                                                  belle_sip_uri_set_user($userinfo::current,unescaped_username);
                                                                  belle_sip_free(unescaped_username);
                                                                  };
password        :   ( unreserved | escaped |AND | EQUAL | PLUS | DOLLARD | COMMA )* {
                                                                              char* unescaped_userpasswd;
                                                                              const char* source = (const char*)$text->chars;
                                                                              if( source != NULL ){
                                                                                unescaped_userpasswd=belle_sip_to_unescaped_string((const char *)source);
                                                                                belle_sip_uri_set_user_password($userinfo::current,unescaped_userpasswd);
                                                                                belle_sip_free(unescaped_userpasswd);
                                                                              }
                                                                              };
hostport[belle_sip_uri_t* uri] 
scope { belle_sip_uri_t* current; }
@init {$hostport::current=uri;}
        :   host ( COLON port {belle_sip_uri_set_port($hostport::current,$port.ret);})? {belle_sip_uri_set_host($hostport::current,$host.ret);};

uri_parameters[belle_sip_uri_t* uri] 
scope { belle_sip_uri_t* current; }
@init {$uri_parameters::current=uri;}    
  :   ( (semi uri_parameter)|(lws? SEMI) )+; /*allow semi only teven if not in the rfc*/
uri_parameter  //all parameters are considered as other     
  :    other_param ;
other_param       
:  pname {
  char* unescaped_parameters = belle_sip_to_unescaped_string((const char *) $pname.text->chars);
  belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS($uri_parameters::current)
                                      ,unescaped_parameters
                                      ,NULL);
  belle_sip_free(unescaped_parameters);
}
  |
   (pname EQUAL pvalue)  {
   char* unescaped_pname = belle_sip_to_unescaped_string((const char *) $pname.text->chars);
   char* unescaped_pvalue = belle_sip_to_unescaped_string((const char *) $pvalue.text->chars);
   belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS($uri_parameters::current)
                                      ,unescaped_pname
                                      ,unescaped_pvalue);
  belle_sip_free(unescaped_pname);
  belle_sip_free(unescaped_pvalue);
  }
   ;

pname             
  :   paramchar+;
pvalue            
  :   paramchar+;
paramchar         
  :   (param_unreserved)=>param_unreserved | unreserved | escaped; 
param_unreserved  
  :   LSBRAQUET | RSBRAQUET | SLASH | COLON | AND | PLUS | DOLLARD | DOT;

headers[belle_sip_uri_t* uri] 
scope { belle_sip_uri_t* current; int is_hvalue; }
@init {$headers::current=uri; $headers::is_hvalue=0;}
                :  QMARK uri_header ( AND uri_header )* ;
uri_header           
scope {int is_hvalue; }
@init {$uri_header::is_hvalue=0;}
              : hname EQUAL (hvalue{$uri_header::is_hvalue = 1;})? {
                      char* unescaped_hname = belle_sip_to_unescaped_string((const char *)$hname.text->chars);
                      char* unescaped_hvalue = ($uri_header::is_hvalue)?belle_sip_to_unescaped_string((const char *)$hvalue.text->chars):NULL;
                      belle_sip_uri_set_header($headers::current,unescaped_hname,unescaped_hvalue);
                      belle_sip_free(unescaped_hname);
                      if (unescaped_hvalue) belle_sip_free(unescaped_hvalue);
                  };
hname           :  ( hnv_unreserved | unreserved | escaped )+;
hvalue          :  ( hnv_unreserved | unreserved | escaped )+;

/*hnv-unreserved  =  "[" / "]" / "/" / "?" / ":" / "+" / "$"*/
hnv_unreserved  :   LSBRAQUET | RSBRAQUET  | SLASH | QMARK | COLON | PLUS | DOLLARD ;


//****************SIP end**********************/	
	
	
//*************************common tokens*******************************/

user_unreserved :  AND | EQUAL | PLUS | DOLLARD | COMMA | SEMI | QMARK | SLASH;
host returns [const char* ret]
scope { const char* current; }
@init {$host::current=$ret=NULL;}
            :   (hostname {$host::current=(const char *)$hostname.text->chars;}
                    | ipv4address {$host::current=(const char *)$ipv4address.text->chars;}
                    | ipv6reference ) {$ret=$host::current;};
hostname        :   ( domainlabel DOT )* (toplabel)=>toplabel DOT? ;
  
domainlabel     :   alphanum | (alphanum ( alphanum | DASH )* alphanum) ;
toplabel        :   alpha | (alpha (  DASH?  alphanum)+) ;

ipv4address    :  three_digit DOT three_digit DOT three_digit DOT three_digit ;
ipv6reference  :  LSBRAQUET ipv6address RSBRAQUET {$host::current=(const char *)$ipv6address.text->chars;};
ipv6address    :  hexpart ( COLON ipv4address )? ;
hexpart        :  hexseq | hexseq COLON COLON ( hexseq )? | COLON COLON ( hexseq )?;
hexseq         :  hex4 ( COLON hex4)*;
hex4           :  hexdigit+;/* hexdigit hexdigit hexdigit ;*/

port  returns [int ret]
@init {$ret=-1;}
	:  DIGIT+ { $ret=atoi((const char *)$text->chars); };

escaped     :  PERCENT hexdigit hexdigit;
ttl : three_digit;
three_digit: (DIGIT) => DIGIT 
            | 
            (DIGIT DIGIT) => (DIGIT DIGIT) 
            | 
            (DIGIT DIGIT DIGIT) =>(DIGIT DIGIT DIGIT) ; 
token       
  :   (alphanum | mark | PERCENT | PLUS | BQUOTE  )+;

reserved_for_from_to_contact_addr_spec: 
	COLON | AT | AND | EQUAL | PLUS | DOLLARD  | SLASH;
	
reserved    
  :   SEMI | COMMA  | QMARK  | reserved_for_from_to_contact_addr_spec ;
                     
unreserved :    alphanum |mark;  
alphanum :     alpha | DIGIT ;                      
hexdigit 
  : HEX_CHAR|DIGIT; 
alpha : HEX_CHAR | COMMON_CHAR; 
 
word        
  :   (alphanum | mark   | PERCENT 
                      | PLUS | BQUOTE |
                     LAQUOT | RAQUOT |
                     COLON | BSLASH | DQUOTE | SLASH | LSBRAQUET | RSBRAQUET | QMARK | LBRACE | RBRACE )+;

mark  :          DASH | USCORE | DOT | EMARK | TILDE | STAR | SQUOTE | LPAREN | RPAREN ; 
sp_tab_colon 
	:( SP | HTAB )* COLON ;	
hcolon  : sp_tab_colon lws? //SWS;
         ;//|( SP | HTAB )* COLON LWS+;
ldquot  :  lws? DQUOTE ;
rdquot : DQUOTE lws?;
semi: lws? SEMI lws?;
comma : lws? COMMA lws?;
sp_laquot
  : lws? LAQUOT ;
raquot_sp
  : RAQUOT lws?;
equal:
   lws? EQUAL lws?;
slash : lws? SLASH lws?;
lws : (SP* CRLF SP+) | SP+ ; //linear whitespace
//*************************************common tokens*****************/	

COMMON_CHAR
  : 'g'..'z' | 'G'..'Z' ; 

HEX_CHAR: 'a'..'f' |'A'..'F';
DIGIT : '0'..'9' ;
AT: '@';
AND: '&';
DOLLARD: '$';
QMARK: '?';
EMARK: '!';
DASH: '-';
CRLF  : '\r\n' { USER1 = (int)((char*)ctx->pLexer->input->currentLine - (char*)ctx->pLexer->input->data); /*GETCHARINDEX()*/;};
HTAB  : '	';
OR : '|';
PERCENT: '%';
DQUOTE  : '"';
SQUOTE  : '\'';
BQUOTE: '`';
BSLASH: '\\';
LBRACE: '{';
RBRACE: '}';
USCORE: '_';
TILDE: '~';
DOT: '.';


PLUS: '+';
COLON
  : ':'
  ;
SEMI
  : ';'
  ;
COMMA
  : ','
  ;
LAQUOT
  : '<'
  ;
RAQUOT
  : '>'
  ;

RPAREN
  : ')'
  ;

LPAREN
  : '('
  ;
RSBRAQUET
  : ']'
  ;

LSBRAQUET
  : '['
  ;
  
EQUAL
  : '='
  ;

SLASH
  : '/'
  ;

STAR
  : '*'
  ; 
SP
  : ' '
  ;
OCTET : . ; 


	
