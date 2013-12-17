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

grammar belle_sip_message;


options { 
  language = C;
  
} 

import   belle_sip_lexer, common, sip, http ;


@includes { 
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

message returns [belle_sip_message_t* ret]
scope { size_t message_length; }
@init {$ret=NULL;}
  : message_raw[&($message::message_length)] {$ret=$message_raw.ret;};
message_raw [size_t* length]  returns [belle_sip_message_t* ret]
scope { size_t* message_length; }
@init {$message_raw::message_length=length;$ret=NULL;}
	:	  request {$ret = BELLE_SIP_MESSAGE($request.ret);} 
	   | response {$ret = BELLE_SIP_MESSAGE($response.ret);} ;
request	returns [belle_sip_request_t* ret]
scope { belle_sip_request_t* current; }
@init {$request::current = belle_sip_request_new(); $ret=$request::current; }
	:	  request_line  message_header[BELLE_SIP_MESSAGE($request::current)]+ last_crlf=CRLF {*($message_raw::message_length)=$last_crlf->user1;} /*message_body ?*/ ;

request_line   
	:	  method {belle_sip_request_set_method($request::current,(const char*)($method.text->chars));} 
	    (SP)=>LWS 
	    uri {belle_sip_request_set_uri($request::current,$uri.ret);}
	    LWS 
	    sip_version 
	    CRLF ;

sip_version   
	:	word;// 'SIP/' DIGIT '.' DIGIT;

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
//                |  unsupported
//                |  user_agent*/
//                |  header_via  {belle_sip_message_add_header(message,BELLE_SIP_HEADER($header_via.ret));}/*
//                |  warning
//                |  www_authenticate*/
                  header_extension[TRUE] {
                    belle_sip_header_t* lheader = BELLE_SIP_HEADER($header_extension.ret);
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

response  returns [belle_sip_response_t* ret]
scope { belle_sip_response_t* current; }
@init {$response::current = belle_sip_response_new(); $ret=$response::current; }         
	:	  status_line (message_header[BELLE_SIP_MESSAGE($response::current)]+ last_crlf=CRLF {*($message_raw::message_length)=$last_crlf->user1;} /*message_body*/)?  ;

status_line     
	:	  sip_version 
	   LWS status_code {belle_sip_response_set_status_code($response::current,atoi((char*)$status_code.text->chars));}
	   LWS reason_phrase {belle_sip_response_set_reason_phrase($response::current,(char*)$reason_phrase.text->chars);}
	   CRLF ;
	
status_code     
	: extension_code;

extension_code  
	:	  DIGIT DIGIT DIGIT;
reason_phrase   
	:	  ~(CRLF)*;
