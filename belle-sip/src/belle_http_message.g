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
grammar belle_http_message;

 
options {	
	language = C;
	
} 
@lexer::header {
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

#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wunused"

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

/*
  absoluteURI   = scheme ":" ( hier_part | opaque_part )
  hier_part     = ( net_path | abs_path ) [ "?" query ]
  net_path      = "//" authority [ abs_path ]
  abs_path      = "/"  path_segments
*/

 scheme: alpha ( alpha | DIGIT | '+' | '-' | '.' )*;

uri  returns [belle_generic_uri_t* ret=NULL]    
scope { belle_generic_uri_t* current; }
@init { $uri::current = $ret = belle_generic_uri_new(); }
   :  scheme {belle_generic_uri_set_scheme($uri::current,(const char*)$scheme.text->chars);} COLON hier_part[$uri::current] ;
catch [ANTLR3_MISMATCHED_TOKEN_EXCEPTION]
{
   belle_sip_message("[\%s]  reason [\%s]",(const char*)EXCEPTION->name,(const char*)EXCEPTION->message);
   belle_sip_object_unref($uri::current);
   $ret=NULL;
}   
   

hier_part[belle_generic_uri_t* uri] returns [belle_generic_uri_t* ret=NULL]  
:  	(
	(SLASH SLASH)=>( SLASH SLASH authority[uri] (SLASH path_segments[uri])?) 
	| 
	(SLASH)=>SLASH  path_segments[uri] ) 
	('?' query 
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
pchar:  unreserved | escaped | COLON | '@' | '&' | EQUAL | '+' | '$' | COMMA;

query: uric+;
uric: reserved | unreserved | escaped;
                        
authority[belle_generic_uri_t* uri] 
:   	(
	(userinfo[NULL])=>(userinfo[uri] hostport[uri]) 
	| hostport[uri] ); 


userinfo[belle_generic_uri_t* uri] 
scope { belle_generic_uri_t* current; }
@init {$userinfo::current=uri;}
       :	user ( COLON password )? '@' ;
user            :	  ( unreserved  | escaped | user_unreserved )+ {
                                                                  char* unescaped_username;
                                                                  unescaped_username=belle_sip_to_unescaped_string((const char *)$text->chars);
                                                                  belle_generic_uri_set_user($userinfo::current,unescaped_username);
                                                                  belle_sip_free(unescaped_username);
                                                                  };
user_unreserved :  '&' | EQUAL | '+' | '$' | COMMA | SEMI | '?' | SLASH;
password        :	  ( unreserved | escaped |'&' | EQUAL | '+' | '$' | COMMA )* {
                                                                              char* unescaped_userpasswd;
                                                                              unescaped_userpasswd=belle_sip_to_unescaped_string((const char *)$text->chars);
                                                                              belle_generic_uri_set_user_password($userinfo::current,unescaped_userpasswd);
                                                                              belle_sip_free(unescaped_userpasswd);
                                                                              };
hostport[belle_generic_uri_t* uri] 
scope { belle_generic_uri_t* current; }
@init {$hostport::current=uri;}
        :	  host ( COLON port {belle_generic_uri_set_port($hostport::current,$port.ret);})? {belle_generic_uri_set_host($hostport::current,$host.ret);};
host returns [const char* ret]
scope { const char* current; }
@init {$host::current=$ret=NULL;}
            :	  (hostname {$host::current=(const char *)$hostname.text->chars;}
                    | ipv4address {$host::current=(const char *)$ipv4address.text->chars;}
                    | ipv6reference ) {$ret=$host::current;};
hostname        :	  ( domainlabel '.' )* toplabel '.'? ;
	
domainlabel     :	  alphanum | (alphanum ( alphanum | '-' )* alphanum) ;
toplabel        :	  alpha | (alpha (  '-'?  alphanum)+) ;

ipv4address    :  three_digit '.' three_digit '.' three_digit '.' three_digit ;
ipv6reference  :  '[' ipv6address ']'{$host::current=(const char *)$ipv6address.text->chars;};
ipv6address    :  hexpart ( COLON ipv4address )? ;
hexpart        :  hexseq | hexseq '::' ( hexseq )? | '::' ( hexseq )?;
hexseq         :  hex4 ( COLON hex4)*;
hex4           :  hexdigit+;/* hexdigit hexdigit hexdigit ;*/

port	returns [int ret]:	DIGIT+ { $ret=atoi((const char *)$text->chars); };

escaped     :  '%' hexdigit hexdigit;
ttl : three_digit;
three_digit: (DIGIT) => DIGIT 
            | 
            (DIGIT DIGIT) => (DIGIT DIGIT) 
            | 
            (DIGIT DIGIT DIGIT) =>(DIGIT DIGIT DIGIT) ;	
token       
	:	  (alphanum | mark | '%' | PLUS | '`'  )+;

reserved    
	:	  SEMI | SLASH | '?' | COLON | '@' | '&' | EQUAL | PLUS
                     | '$' | COMMA;
                     
unreserved :	  alphanum |mark;  
alphanum :	   alpha | DIGIT ;                      
hexdigit 
	:	HEX_CHAR|DIGIT; 
alpha	: HEX_CHAR | COMMON_CHAR; 
 
word        
	:	  (alphanum | mark   | '%'  
                      | PLUS | '`' |
                     LAQUOT | RAQUOT |
                     COLON | '\\' | DQUOTE | SLASH | '[' | ']' | '?' | '{' | '}' )+;
                 

COMMON_CHAR
	:	'g'..'z' | 'G'..'Z' ;	
mark	:	         '-' | '_' | '.' | '!' | '~' | STAR | '\'' | LPAREN | RPAREN ; 


HEX_CHAR:	'a'..'f' |'A'..'F';
DIGIT	: '0'..'9' ;

CRLF	: '\r\n' { USER1 = (int)((char*)ctx->pLexer->input->currentLine - (char*)ctx->pLexer->input->data); /*GETCHARINDEX()*/;};





hcolon	: ( LWS | HTAB )* COLON  LWS? //SWS;
	       ;//|( SP | HTAB )* COLON LWS+;
HTAB	: '	';




  
 
ldquot  :  LWS? DQUOTE ;
rdquot : DQUOTE LWS?;

DQUOTE	: '"';
// open double quotation mark
//SWS   :   LWS? ; 


LWS  	:	(SP* CRLF)? SP+ ; //linear whitespace





PLUS: '+';


COLON
	:	':'
	;
semi: LWS? SEMI LWS?;

SEMI
	:	';'
	;

comma : LWS? COMMA LWS?;
COMMA
	:	','
	;

sp_laquot_sp
	:	LWS? LAQUOT LWS?;
sp_raquot_sp
	:	LWS? RAQUOT LWS?;
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

equal:
   LWS? EQUAL LWS?;
EQUAL
	:	'='
	;

slash : LWS? SLASH LWS?;
SLASH
	:	'/'
	;

STAR
	:	'*'
	;
fragment 
SP
	:	' '
	;
OCTET	: . ;	
