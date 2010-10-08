grammar belle_sip_uri;

options {	
	language = C;
	
}
@header { #include "belle_sip_uri.h"}

an_sip_uri  returns [belle_sip_uri* ret]    
scope { belle_sip_uri* current; }
@init { $an_sip_uri::current = belle_sip_uri_new(); }
   :  sip_schema ((userinfo hostport) | hostport ) uri_parameters? headers? {$ret = $an_sip_uri::current;}; 
sip_schema : ('sip' | is_sips='sips') ':' {if ($is_sips) belle_sip_uri_set_secure($an_sip_uri::current,1);};
userinfo        :	user ( ':' password )? '@' ;
user            :	  ( unreserved  | escaped | user_unreserved )+ {belle_sip_uri_set_user($an_sip_uri::current,(const char *)$text->chars);};
user_unreserved :  '&' | '=' | '+' | '$' | ',' | ';' | '?' | '/';
password        :	  ( unreserved  |'&' | '=' | '+' | '$' | ',' )*;
hostport        :	  host ( ':' port )? {belle_sip_uri_set_host($an_sip_uri::current,(const char *)$host.text->chars);};
host            :	  (hostname | ipv4address | ipv6reference) ;
hostname        :	  ( domainlabel '.' )* toplabel '.'? ;
	
domainlabel     :	  alphanum | (alphanum ( alphanum | '-' )* alphanum) ;
toplabel        :	  alpha | (alpha ( alphanum | '-' )* alphanum) ;

ipv4address    :  three_digit '.' three_digit '.' three_digit '.' three_digit;
ipv6reference  :  '[' ipv6address ']';
ipv6address    :  hexpart ( ':' ipv4address )?;
hexpart        :  hexseq | hexseq '::' ( hexseq )? | '::' ( hexseq )?;
hexseq         :  hex4 ( ':' hex4)*;
hex4           :  hexdigit hexdigit hexdigit hexdigit ;

port	:	DIGIT+ {belle_sip_uri_set_port($an_sip_uri::current,atoi((const char *)$text->chars));};


uri_parameters    
	:	  ( ';' uri_parameter)+;
uri_parameter     
	:	  {strcmp("transport=",(const char*)(INPUT->toStringTT(INPUT,LT(1),LT(10)))->chars) == 0}? transport_param 
		| {strcmp("user=",(const char*)(INPUT->toStringTT(INPUT,LT(1),LT(5)))->chars) == 0}? user_param 
    | {strcmp("ttl=",(const char*)(INPUT->toStringTT(INPUT,LT(1),LT(4)))->chars) == 0}? ttl_param 
    | {strcmp("maddr=",(const char*)(INPUT->toStringTT(INPUT,LT(1),LT(6)))->chars) == 0}? maddr_param 
    | {strcmp("lr",(const char*)(INPUT->toStringTT(INPUT,LT(1),LT(2)))->chars) == 0}? lr_param 
    | other_param ;
transport_param   
	:	  pname '=' transport_value;
transport_value: token 
                      {belle_sip_uri_set_transport_param($an_sip_uri::current,(const char *)$text->chars);};
user_param        
	:	  other_param; 

  
ttl_param         
	:	  pname '=' ttl;
maddr_param       
	:	  pname '=' host {belle_sip_uri_set_maddr_param($an_sip_uri::current,(const char *)$host.text->chars);};
lr_param          
	:	  alpha alpha {belle_sip_uri_set_lr_param($an_sip_uri::current,1);};
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
header          :  hname '=' hvalue? {belle_sip_uri_set_header($an_sip_uri::current,$hname.text->chars,$hvalue.text->chars);};
hname           :  ( hnv_unreserved | unreserved | escaped )+;
hvalue          :  ( hnv_unreserved | unreserved | escaped )+;


hnv_unreserved  :  '[' | ']' | '|' | '?' | ':' | '+' | '$' ;
escaped     :  '%' hexdigit hexdigit;
ttl : three_digit;
three_digit: DIGIT | (DIGIT DIGIT) | (DIGIT DIGIT DIGIT) ;	
token       
	:	  (alphanum | MARK_LEX | '%' | '+' | '`'  )+;

unreserved :	  alphanum |MARK_LEX;  
alphanum :	   alpha | DIGIT ;                      
hexdigit 
	:	HEX_CHAR|DIGIT; 
alpha	: HEX_CHAR | COMMON_CHAR; 

COMMON_CHAR
	:	'g'..'z' | 'G'..'Z' ;	
MARK_LEX	:	         '-' | '_' | '.' | '!' | '~' | '*' | '\'' ; 


HEX_CHAR:	'a'..'f' |'A'..'F';
DIGIT	: '0'..'9' ;

	
