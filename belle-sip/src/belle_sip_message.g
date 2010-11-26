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
@header { 
#include "belle-sip/belle-sip.h"
#include "belle_sip_internal.h"
}


/*

message  
	:	  request | response ;
request	
	:	  request_line | message_header* CRLF message_body ;

request_line   
	:	  method SP request_uri SP sip_version CRLF ;

request_uri   
	:	  an_sip_uri ;
sip_version   
	:	  'SIP/' DIGIT '.' DIGIT;

message_header  
	:	 (accept
                |  accept_encoding
                |  accept_language
                |  alert_info
                |  allow
                |  authentication_info
                |  authorization
                |  call_id
                |  call_info
                |  contact
                |  content_disposition
                |  content_encoding
                |  content_language
                |  content_length
                |  content_type
                |  cseq
                |  date
                |  error_info
                |  expires
                |  from
                |  in_reply_to
                |  max_forwards
                |  mime_version
                |  min_expires
                |  organization
                |  priority
                |  proxy_authenticate
                |  proxy_authorization
                |  proxy_require
                |  record_route
                |  reply_to
                |  require
                |  retry_after
                |  route
                |  server
                |  subject
                |  supported
                |  timestamp
                |  to
                |  unsupported
                |  user_agent
                |  via
                |  warning
                |  www_authenticate
                |  extension_header) CRLF;

invitem           
	:	'INVITE' ; //INVITE in caps
ackm 	:	'ACK'; //ACK in caps
optionsm:	'OPTION'; //OPTIONS in caps
byem	:	'BYE' ; //BYE in caps
cancelm :	'CANCEL' ; //CANCEL in caps
registerm
	:	'REGISTER' ; //REGISTER in caps
optionm :	'OPTION';

method 	:	           invitem | ackm | optionm | byem | cancelm | registerm |extension_method ;

extension_method  
	:	  token;
	
response          
	:	  status_line message-header* CRLF message_body ;

status_line     
	:	  sip_version SP status_code SP reason_phrase CRLF ;
	
status_code     
	: extension_code;

extension_code  
	:	  DIGIT DIGIT DIGIT;
reason_phrase   
	:	  (reserved | unreserved | escaped | utf8_non_ascii | utf8cont | SP | HTAB)*;

utf8cont
	:
	;

utf8_non_ascii
	:
	;


	
accept         
	:	  'Accept' HCOLON ( accept_range (COMMA accept_range)* )?;
accept_range   
	:	  media_range (SEMI accept_param)*;
*/
//media_range    
//	:	  ( '*/*'
//                  | ( m_type SLASH '*' )
//                  | ( m_type SLASH m_subtype )
//                  ) ( SEMI m_parameter )* ;

//accept_param   
//	:	  ('q' EQUAL qvalue) | generic_param;
/*
qvalue         
	:	  ( '0' ( '.' DIGIT+)? )
                  | ( '1'( '.'DIGIT+)? );
*/
generic_param  returns [belle_sip_param_pair_t* ret]
	:	  token ( EQUAL is_gen=gen_value )? {$ret=belle_sip_param_pair_new($token.text->chars,$is_gen.text?$gen_value.text->chars:NULL);};
gen_value      
	:	  token |  quoted_string;

quoted_string 
options { greedy = false; }
	: DQUOTE .* DQUOTE ;

/*
accept_encoding  
	:	  'Accept-Encoding' HCOLON ( encoding (COMMA encoding)* );
encoding         
	:	  codings (SEMI accept_param)*;
codings          
	:	  content_coding | '*';
content_coding   :  token;

accept_language  
	:	  'Accept-Language' HCOLON
                     ( language (COMMA language)* )?;
language         
	:	  language_range (SEMI accept_param)*;
language_range   
	:	  ( ( alpha alpha alpha alpha alpha alpha alpha alpha ( '-' alpha alpha alpha alpha alpha alpha alpha alpha )* ) | '*' );

alert_info   
	:	  'Alert-Info' HCOLON alert_param (COMMA alert_param)*;
	
alert_param  
	:	  LAQUOT absoluteURI RAQUOT ( SEMI generic_param )*;

absoluteURI    
	:	  token ':' token;

allow  	:	  'Allow' HCOLON (method (COMMA method))? ;

authorization     
	:	  'Authorization' HCOLON credentials;
credentials       
	:	  ('Digest' LWS digest_response)
                     | other_response;
digest_response   
	:	  dig_resp (COMMA dig_resp)*;
dig_resp          
	:	  username | realm | nonce | digest_uri
                      | dresponse | algorithm | cnonce
                      | opaque | message_qop
                      | nonce_count | auth_param;
username          
	:	  'username' EQUAL username_value;
username_value    :  quoted_string;
digest_uri        
	:	  'uri' EQUAL LDQUOT digest_uri_value RDQUOT;
digest_uri_value  :  rquest_uri ;

rquest_uri
	: absoluteURI;
	;
// Equal to request-uri as specified by HTTP/1.1
message_qop       
	:	  'qop' EQUAL qop_value;

qop_value
	: 'auth'|'auth-int' | token;
	;

cnonce            
	:	  'cnonce' EQUAL cnonce_value;
cnonce_value      
	:	  nonce_value;
nonce_count       
	:	  'nc' EQUAL nc_value;
nc_value          
	:	  huit_lhex; ;
dresponse         
	:	  'response' EQUAL request_digest;
request_digest    
	:	  LDQUOT huit_lhex huit_lhex huit_lhex huit_lhex RDQUOT;

huit_lhex
	: hexdigit+;
	;

auth_param        
	:	  auth_param_name EQUAL
                     ( token | quoted_string );
auth_param_name   
	:	  token;
other_response    
	:	  auth_scheme LWS auth_param
                     (COMMA auth_param)*;
auth_scheme       
	:	  token;

authentication_info  :  'Authentication-Info' HCOLON ainfo
                        (COMMA ainfo)*;
ainfo                
	:	  nextnonce | message_qop
                         | response_auth | cnonce
                         | nonce_count;
nextnonce            
	:	  'nextnonce' EQUAL nonce_value;
nonce_value         :  quoted_string;
response_auth        
	:	  'rspauth' EQUAL response_digest;
response_digest      
	:	  LDQUOT hexdigit* RDQUOT;

call_id  :  ( 'Call-ID' | 'i' ) HCOLON callid;
callid   
	:	  word ( '@' word )?;

call_info   
	:	  'Call-Info' HCOLON info (COMMA info)*;
info        
	:	  LAQUOT absoluteURI RAQUOT ( SEMI info_param)*;
info_param  
	:	  ( 'purpose' EQUAL ( 'icon' | 'info'
               | 'card' | token ) ) | generic_param;
*/

header_contact      returns [belle_sip_header_contact_t* ret]   
scope { belle_sip_header_contact_t* current; }
@init { $header_contact::current = belle_sip_header_contact_new(); }
	:	  ('Contact' /*| 'm'*/ ) hcolom
                  ( STAR | (contact_param (COMMA contact_param)*)) {$ret = $header_contact::current;};
contact_param  
	:	  (name_addr | addr_spec) (SEMI contact_params)*;
name_addr      
	:	  ( display_name )? sp_laquot_sp addr_spec sp_raquot_sp;
addr_spec      :  uri {belle_sip_header_address_set_uri((belle_sip_header_address_t*) $header_contact::current
                                                        ,belle_sip_uri_ref($uri.ret));
                      };//| absoluteURI;
display_name   :  token  | quoted_string {belle_sip_header_address_set_displayname((belle_sip_header_address_t*) $header_contact::current
                                                        ,$display_name.text->chars);};

contact_params     
	:	  /*c_p_q | c_p_expires
                      |*/ contact_extension;
/*c_p_q              
	:	  'q' EQUAL qvalue;
c_p_expires        
	:	  'expires' EQUAL delta_seconds;*/
contact_extension  
	:	  generic_param;
/*
delta_seconds      
	:	  DIGIT+;*/
/*
content_disposition   
	:	  'Content-Disposition' HCOLON
                         disp_type ( SEMI disp_param )*;
disp_type             :  'render' | 'session' | 'icon' | 'alert'
                         | disp_extension_token;

disp_param            
	:	  handling_param | generic_param;
handling_param        
	:	  'handling' EQUAL
                         ( 'optional' | 'required'
                         | other_handling );
other_handling        
	:	  token;
disp_extension_token  
	:	  token;

content_encoding  
	:	  ( 'Content-Encoding' | 'e' ) HCOLON
                     content_coding (COMMA content_coding)*;

content_language  
	:	  'Content-Language' HCOLON
                     language_tag (COMMA language_tag)*;
language_tag      
	:	  primary_tag ( '-' subtag )*;
primary_tag       
	:	  huit_alpha;
subtag            
	:	  huit_alpha;

huit_alpha
	: alpha+;
	;

content_length  
	:	  ( 'Content-Length' | 'l' ) HCOLON DIGIT+;
content_type     
	:	  ( 'Content-Type' | 'c' ) HCOLON media_type;
media_type       
	:	  m_type SLASH m_subtype (SEMI m_parameter);
m_type           
	:	  discrete_type | composite_type;
discrete_type    
	:	  'text' | 'image' | 'audio' | 'video'
                    | 'application' | extension_token;
composite_type   
	:	  'message' | 'multipart' | extension_token;
extension_token  
	:	  ietf_token | x_token;
ietf_token       
	:	  token;
x_token          
	:	  'x-' token;
m_subtype        :  extension_token | iana_token;
iana_token       
	:	  token;
m_parameter      
	:	  m_attribute EQUAL m_value;
m_attribute      
	:	  token;
m_value          
	:	  token | quoted_string;

cseq  	:	  'CSeq' HCOLON DIGIT+ LWS method;

date          
	:	  'Date' HCOLON sip_date;
sip_date      
	:	token;//  rfc1123-date
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
	:	  'Error-Info' HCOLON error_uri (COMMA error_uri)*;

error_uri   
	:	  LAQUOT absoluteURI RAQUOT ( SEMI generic_param )*;

expires     
	:	  'Expires' HCOLON delta_seconds;

from        
	:	  ( 'From' | 'f' ) HCOLON from_spec;
from_spec   
	:	  ( name_addr | addr_spec )
               ( SEMI from_param )*;
from_param  
	:	  tag_param | generic_param;
tag_param   
	:	  'tag' EQUAL token;


in_reply_to  
	:	  'In-Reply-To' HCOLON callid (COMMA callid);

max_forwards  :  'Max-Forwards' HCOLON DIGIT+;

mime_version  
	:	  'MIME-Version' HCOLON DIGIT+ '.' DIGIT+;

min_expires  
	:	  'Min-Expires' HCOLON delta_seconds;

organization  
	:	  'Organization' HCOLON (text_utf8_trim)?;

text_utf8_trim
	:
	;

priority        
	:	  'Priority' HCOLON priority_value;
priority_value  
	:	  'emergency' | 'urgent' | 'normal'
                   | 'non-urgent' | other_priority;
other_priority  
	:	  token;

proxy_authenticate  
	:	  'Proxy-Authenticate' HCOLON challenge;
challenge           
	:	  ('Digest' LWS digest_cln (COMMA digest_cln)*)
                       | other_challenge;
other_challenge     
	:	  auth_scheme LWS auth_param
                       (COMMA auth_param)*;
digest_cln          
	:	  realm | domain | nonce
                        | opaque | stale | algorithm
                        | qop_options | auth_param;
realm               
	:	  'realm' EQUAL realm_value;
realm_value         
	:	  quoted_string;
domain              
	:	  'domain' EQUAL LDQUOT uri
                       ( SP+ uri )* RDQUOT;
uri                 
	:	  absoluteURI | '/'.;
nonce               
	:	  'nonce' EQUAL nonce_value;

opaque              
	:	  'opaque' EQUAL quoted_string;
stale               
	:	  'stale' EQUAL ( 'true' | 'false' );
algorithm           
	:	  'algorithm' EQUAL ( 'MD5' | 'MD5-sess'
                       | token );
qop_options         
	:	  'qop' EQUAL LDQUOT qop_value
                       (',' qop_value)* RDQUOT:
qop_value           
	:	  'auth' | 'auth-int' | token;

proxy_authorization  
	:	  'Proxy-Authorization' HCOLON credentials;

proxy_require  
	:	  'Proxy-Require' HCOLON option_tag
                  (COMMA option_tag)*;
option_tag     
	:	  token;

record_route  
	:	  'Record-Route' HCOLON rec_route (COMMA rec_route)*;
rec_route     
	:	  name_addr ( SEMI rr_param );
rr_param      
	:	  generic_param;

reply_to      
	:	  'Reply-To' HCOLON rplyto_spec;
rplyto_spec   
	:	  ( name_addr | addr_spec )
                 ( SEMI rplyto_param )*;
rplyto_param  
	:	  generic_param;
require       
	:	  'Require' HCOLON option_tag (COMMA option_tag)*;

retry_after  
	:	  'Retry-After' HCOLON delta_seconds
                 comment? ( SEMI retry_param )*;

comment	: '(' . ')';
	;

retry_param  
	:	  ('duration' EQUAL delta_seconds)
                | generic_param;

route        
	:	  'Route' HCOLON route_param (COMMA route_param)*;
route_param  
	:	  name_addr ( SEMI rr_param )*;

server           
	:	  'Server' HCOLON server_val (LWS server_val)*;
server_val       
	:	  product | comment;
product          
	:	  token (SLASH product_version)?;
product_version  
	:	  token;

subject  
	:	  ( 'Subject' | 's' ) HCOLON (text_utf_huit)?;

text_utf_huit
	:
	;

supported  
	:	  ( 'Supported' | 'k' ) HCOLON
              (option_tag (COMMA option_tag)*)?;

timestamp  
	:	  'Timestamp' HCOLON (DIGIT)+
               ( '.' (DIGIT)* )? ( LWS delay )?;
delay      
	:	  (DIGIT)* ( '.' (DIGIT)* )?;

to        
	:	  ( 'To' | 't' ) HCOLON ( name_addr
             | addr_spec ) ( SEMI to_param )*;
             
to_param  
	:	  tag_param | generic_param;

unsupported  
	:	  'Unsupported' HCOLON option_tag (COMMA option_tag)*;
user_agent  
	:	  'User-Agent' HCOLON server_val (LWS server_val)*;

via               
	:	  ( 'Via' | 'v' ) HCOLON via_parm (COMMA via_parm)*;
via_parm          
	:	  sent_protocol LWS sent_by ( SEMI via_params )*;
via_params        
	:	  via_ttl | via_maddr
                     | via_received | via_branch
                     | via_extension;
via_ttl           
	:	  'ttl' EQUAL ttl;
via_maddr         
	:	  'maddr' EQUAL host;
via_received      
	:	  'received' EQUAL (ipv4address | ipv6address);
via_branch        
	:	  'branch' EQUAL token;
via_extension     
	:	  generic_param;
sent_protocol     
	:	  protocol_name SLASH protocol_version
                     SLASH transport;
protocol_name     
	:	  'SIP' | token;
protocol_version  
	:	  token;
transport         
	:	  'UDP' | 'TCP' | 'TLS' | 'SCTP'
                     | other_transport;

other_transport
	: token;
	;

sent_by           
	:	  host ( COLON port )?
ttl               
	:	  DIGIT+ ; 0 to 255

warning        
	:	  'Warning' HCOLON warning_value (COMMA warning_value)*;
warning_value  
	:	  warn_code SP warn_agent SP warn_text;
warn_code      
	:	  DIGIT DIGIT DIGIT;
warn_agent     
	:	  hostport | pseudonym;
                  //  the name or pseudonym of the server adding
                  //  the Warning header, for use in debugging
warn_text      
	:	  quoted_string;
pseudonym      
	:	  token;

www_authenticate  
	:	  'WWW-Authenticate' HCOLON challenge;

extension_header  
	:	  header_name HCOLON header_value;
header_name       
	:	  token;
header_value      
	:	  .;
message_body  
	:	  OCTET*;
*/
uri  returns [belle_sip_uri_t* ret]    
scope { belle_sip_uri_t* current; }
@init { $uri::current = belle_sip_uri_new(); }
   :  sip_schema ((userinfo hostport) | hostport ) uri_parameters? headers? {$ret = $uri::current;}; 
sip_schema : ('sip' | is_sips='sips') COLON {if ($is_sips) belle_sip_uri_set_secure($uri::current,1);};
userinfo        :	user ( COLON password )? '@' ;
user            :	  ( unreserved  | escaped | user_unreserved )+ {belle_sip_uri_set_user($uri::current,(const char *)$text->chars);};
user_unreserved :  '&' | EQUAL | '+' | '$' | COMMA | SEMI | '?' | SLASH;
password        :	  ( unreserved  |'&' | EQUAL | '+' | '$' | COMMA )*;
hostport        :	  host ( COLON port )? {belle_sip_uri_set_host($uri::current,(const char *)$host.text->chars);};
host            :	  (hostname | ipv4address | ipv6reference) ;
hostname        :	  ( domainlabel '.' )* toplabel '.'? ;
	
domainlabel     :	  alphanum | (alphanum ( alphanum | '-' )* alphanum) ;
toplabel        :	  alpha | (alpha ( alphanum | '-' )* alphanum) ;

ipv4address    :  three_digit '.' three_digit '.' three_digit '.' three_digit;
ipv6reference  :  '[' ipv6address ']';
ipv6address    :  hexpart ( COLON ipv4address )?;
hexpart        :  hexseq | hexseq '::' ( hexseq )? | '::' ( hexseq )?;
hexseq         :  hex4 ( COLON hex4)*;
hex4           :  hexdigit hexdigit hexdigit hexdigit ;

port	:	DIGIT+ {belle_sip_uri_set_port($uri::current,atoi((const char *)$text->chars));};


uri_parameters    
	:	  ( SEMI uri_parameter )+;
uri_parameter  //all parameters are considered as other     
	:	   other_param ;
other_param       :  pname ( EQUAL pvalue )?
  {
    if (strcmp("lr",(const char *)$pname.text->chars) == 0) {
      belle_sip_uri_set_lr_param($uri::current,1);
      } else if (strcmp("transport",(const char*)$pname.text->chars)==0) {
        belle_sip_uri_set_transport_param($uri::current,(const char *)$pvalue.text->chars);
      } else if (strcmp("user",(const char *)$pname.text->chars)==0) {
        belle_sip_uri_set_user_param($uri::current,(const char *)$pvalue.text->chars);
      } else if (strcmp("maddr",(const char *)$pname.text->chars)==0) {
        belle_sip_uri_set_maddr_param($uri::current,(const char *)$pvalue.text->chars);
      } else if (strcmp("ttl",(const char *)$pname.text->chars)==0) {
        belle_sip_uri_set_ttl_param($uri::current,atoi((const char *)$pvalue.text->chars));
      } else if (strcmp("method",(const char *)$pname.text->chars)==0) {
        belle_sip_uri_set_method_param($uri::current,(const char *)$pvalue.text->chars);
      } else {
        belle_sip_warning("unknown uri param \%s",(const char *)$other_param.text->chars);
      }
  };
pname             
	:	  paramchar+;
pvalue            
	:	  paramchar+;
paramchar         
	:	  param_unreserved | unreserved | escaped; 
param_unreserved  
	:	  '[' | ']' | SLASH | COLON | '&' | '+' | '$' | '.';

headers         :  '?' header ( '&' header )* ;
header          :  hname EQUAL hvalue? {belle_sip_uri_set_header($uri::current,(const char *)$hname.text->chars,(const char *)$hvalue.text->chars);};
hname           :  ( hnv_unreserved | unreserved | escaped )+;
hvalue          :  ( hnv_unreserved | unreserved | escaped )+;


hnv_unreserved  :  '[' | ']' | '|' | '?' | COLON | '+' | '$' ;
escaped     :  '%' hexdigit hexdigit;
ttl : three_digit;
three_digit: DIGIT | (DIGIT DIGIT) | (DIGIT DIGIT DIGIT) ;	
token       
	:	  (alphanum | mark | '%' | '+' | '`'  )+;

reserved    
	:	  SEMI | SLASH | '?' | COLON | '@' | '&' | EQUAL | '+'
                     | '$' | COMMA;
                     
unreserved :	  alphanum |mark;  
alphanum :	   alpha | DIGIT ;                      
hexdigit 
	:	HEX_CHAR|DIGIT; 
alpha	: HEX_CHAR | COMMON_CHAR; 
 
word        
	:	  (alphanum | '-' | '.' | '!' | '%' | STAR |
                     '_' | '+' | '`' | '\'' | '~' |
                     LPAREN | RPAREN | LAQUOT | RAQUOT |
                     COLON | '\\' | DQUOTE | SLASH | '[' | ']' | '?' | '{' | '}' )+;
                 

COMMON_CHAR
	:	'g'..'z' | 'G'..'Z' ;	
mark	:	         '-' | '_' | '.' | '!' | '~' | STAR | '\'' ; 


HEX_CHAR:	'a'..'f' |'A'..'F';
DIGIT	: '0'..'9' ;

CRLF	: '\r\n';





hcolom	: ( SP | HTAB )* COLON SP* ; //SWS;

HTAB	: '	';




  
 
  


DQUOTE	: '"';
// open double quotation mark

LWS  	:	  (SP* CRLF) SP* ; //linear whitespace
SWS  	:	  LWS? ;






COLON
	:	':'
	;

SEMI
	:	';'
	;

COMMA
	:	','
	;

sp_laquot_sp
	:	SP* LAQUOT SP*;
sp_raquot_sp
	:	SP* RAQUOT SP*;
LAQUOT
	:	'<'
	;

RAQUOT
	:	'>'
	;

RPAREN
	:	')'
	;

LPAREN
	:	'('
	;

EQUAL
	:	'='
	;

SLASH
	:	'/'
	;

STAR
	:	'*'
	;

SP
	:	' '
	;
OCTET	: . ;	