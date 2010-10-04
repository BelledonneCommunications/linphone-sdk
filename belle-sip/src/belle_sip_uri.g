grammar belle_sip_uri;

options {	language = C;}
@header { #include "belle_sip_uri.h"}

an_sip_uri  returns [belle_sip_uri* ret]    
scope { belle_sip_uri* current; }
@init { $an_sip_uri::current = belle_sip_uri_new(); }
   :  sip_schema userinfo? hostport uri_parameters headers? {$ret = $an_sip_uri::current;}; 
sip_schema : ('sip' | is_sips='sips') ':' {if ($is_sips) belle_sip_uri_set_secure($an_sip_uri::current,1);};
userinfo        :	user ( ':' password )? '@' ;
user            :	  ( unreserved  | escaped | user_unreserved )+ {belle_sip_uri_set_user($an_sip_uri::current,(const char *)$text->chars);};
user_unreserved :  '&' | '=' | '+' | '$' | ',' | ';' | '?' | '/';
password        :	  ( unreserved  |'&' | '=' | '+' | '$' | ',' )*;
hostport        :	  host ( ':' port )? {belle_sip_uri_set_host($an_sip_uri::current,(const char *)$host.text->chars);};
host            :	  (hostname | ipv4address | ipv6reference) ;
fragment hostname        :	  ( domainlabel '.' )* toplabel '.'? ;
	
fragment domainlabel     :	  alphanum | alphanum ( alphanum | '-' )* alphanum ;
fragment toplabel        :	  ALPHA | ALPHA ( alphanum | '-' )* alphanum;

ipv4address    :  three_digit '.' three_digit '.' three_digit '.' three_digit;
ipv6reference  :  '[' ipv6address ']';
ipv6address    :  hexpart ( ':' ipv4address )?;
fragment hexpart        :  hexseq | hexseq '::' ( hexseq )? | '::' ( hexseq )?;
fragment hexseq         :  hex4 ( ':' hex4)*;
fragment hex4           :  HEXDIG HEXDIG HEXDIG HEXDIG ;

port	:	DIGIT+ {belle_sip_uri_set_port($an_sip_uri::current,atoi((const char *)$text->chars));};


uri_parameters    
	:	  ( ';' uri_parameter)*;
uri_parameter     
	:	  transport_param | user_param 
                     | ttl_param | maddr_param | lr_param | other_param ;
transport_param   
	:	  'transport=' transport_value;
transport_value:  ('udp' | 'tcp' | 'sctp' | 'tls'| other_transport) 
                      {belle_sip_uri_set_transport_param($an_sip_uri::current,(const char *)$text->chars);};
other_transport   
	:	  token ;
user_param        
	:	  'user=' ( 'phone' | 'ip' | other_user); 
other_user        
	:	  token;
  
ttl_param         
	:	  'ttl=' ttl;
maddr_param       
	:	  'maddr=' host {belle_sip_uri_set_maddr_param($an_sip_uri::current,(const char *)$host.text->chars);};
lr_param          
	:	  'lr' {belle_sip_uri_set_lr_param($an_sip_uri::current,1);};
other_param       :  pname ( '=' pvalue )?;
pname             
	:	  paramchar+;
pvalue            
	:	  paramchar+;
paramchar         
	:	  param_unreserved | unreserved | escaped;
param_unreserved  
	:	  '[' | ']' | '/' | ':' | '&' | '+' | '$';

headers         :  '?' header ( '&' header )* ;
header          :  hname '=' hvalue;
hname           :  ( hnv_unreserved | unreserved | escaped )+;
hvalue          :  ( hnv_unreserved | unreserved | escaped )*;


fragment hnv_unreserved  :  '[' | ']' | '|' | '?' | ':' | '+' | '$' ;
fragment escaped     :  '%' HEXDIG HEXDIG;
fragment ttl : three_digit;
fragment three_digit: DIGIT DIGIT? DIGIT? ;	
fragment token       
	:	  (alphanum | MARK_LEX | '%' | '+' | '`'  )+;

fragment unreserved :	  alphanum |MARK_LEX;  
fragment alphanum :	   ALPHA | DIGIT ;                     
fragment MARK_LEX	:	         '-' | '_' | '.' | '!' | '~' | '*' | '\'' ; 
fragment HEXDIG 
	:	'a'..'f' |'A'..'F'|DIGIT;
ALPHA	:	('a'..'z'|'A'..'Z');
DIGIT	: '0'..'9' ;

