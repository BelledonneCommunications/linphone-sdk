grammar sipuri;

options {	language = C;}
@header { #include "sip_uri.h"}

an_sip_uri  returns [sip_uri* ret]    
scope { sip_uri* current; }
@init { $an_sip_uri::current = sip_uri_new(); }
   :  'sip:' userinfo? hostport uri_parameters {$ret = $an_sip_uri::current;}; 
userinfo        :	user ( ':' password )? '@' ;
user            :	  ( unreserved  | user_unreserved )+ {sip_uri_set_user($an_sip_uri::current,(const char *)$text->chars);};
user_unreserved :  '&' | '=' | '+' | '$' | ',' | ';' | '?' | '/';
password        :	  ( unreserved  |'&' | '=' | '+' | '$' | ',' )*;
hostport        :	  host ( ':' port )?;
host            :	  hostname {sip_uri_set_host($an_sip_uri::current,(const char *)$text->chars);};
hostname        :	  ( domainlabel '.' )* toplabel '.'? ;
	
domainlabel     :	  alphanum | alphanum ( alphanum | '-' )* alphanum ;
toplabel        :	  ALPHA | ALPHA ( alphanum | '-' )* alphanum;
port	:	DIGIT+ ;


uri_parameters    
	:	  ( ';' uri_parameter)*;
uri_parameter     
	:	  transport_param | user_param 
                     | ttl_param | maddr_param | lr_param | other_param ;
transport_param   
	:	  'transport=' transport_value;
transport_value:  ('udp' | 'tcp' | 'sctp' | 'tls'| other_transport) 
                      {sip_uri_set_transport_param($an_sip_uri::current,(const char *)$text->chars);};
other_transport   
	:	  token ;
user_param        
	:	  'user=' ( 'phone' | 'ip' | other_user); 
other_user        
	:	  token;
  
ttl_param         
	:	  'ttl=' ttl;
maddr_param       
	:	  'maddr=' host;
lr_param          
	:	  'lr';
other_param       :  pname ( '=' pvalue )?;
pname             
	:	  paramchar+;
pvalue            
	:	  paramchar+;
paramchar         
	:	  param_unreserved | unreserved | escaped;
param_unreserved  
	:	  '[' | ']' | '/' | ':' | '&' | '+' | '$';

fragment ttl
	:	DIGIT DIGIT? DIGIT? ; 
fragment escaped     
	:	  '%' HEXDIG HEXDIG;
fragment token       
	:	  (alphanum | MARK_LEX | '%' | '+' | '`'  )+;

fragment unreserved :	  alphanum |MARK_LEX;  
fragment alphanum :	   ALPHA | DIGIT ;                     
fragment MARK_LEX	:	         '-' | '_' | '.' | '!' | '~' | '*' | '\'' ; 
fragment HEXDIG 
	:	'a'..'f' |'A'..'F'|DIGIT;
ALPHA	:	('a'..'z'|'A'..'Z');
DIGIT	: '0'..'9' ;

