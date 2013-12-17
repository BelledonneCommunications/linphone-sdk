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
parser grammar common;


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



user_unreserved :  AND | EQUAL | PLUS | DOLLARD | COMMA | SEMI | QMARK | SLASH;
host returns [const char* ret]
scope { const char* current; }
@init {$host::current=$ret=NULL;}
            :	  (hostname {$host::current=(const char *)$hostname.text->chars;}
                    | ipv4address {$host::current=(const char *)$ipv4address.text->chars;}
                    | ipv6reference ) {$ret=$host::current;};
hostname        :	  ( domainlabel DOT )* toplabel DOT? ;
	
domainlabel     :	  alphanum | (alphanum ( alphanum | DASH )* alphanum) ;
toplabel        :	  alpha | (alpha (  DASH?  alphanum)+) ;

ipv4address    :  three_digit DOT three_digit DOT three_digit DOT three_digit ;
ipv6reference  :  LSBRAQUET ipv6address RSBRAQUET {$host::current=(const char *)$ipv6address.text->chars;};
ipv6address    :  hexpart ( COLON ipv4address )? ;
hexpart        :  hexseq | hexseq COLON COLON ( hexseq )? | COLON COLON ( hexseq )?;
hexseq         :  hex4 ( COLON hex4)*;
hex4           :  hexdigit+;/* hexdigit hexdigit hexdigit ;*/

port	returns [int ret]:	DIGIT+ { $ret=atoi((const char *)$text->chars); };

escaped     :  PERCENT hexdigit hexdigit;
ttl : three_digit;
three_digit: (DIGIT) => DIGIT 
            | 
            (DIGIT DIGIT) => (DIGIT DIGIT) 
            | 
            (DIGIT DIGIT DIGIT) =>(DIGIT DIGIT DIGIT) ;	
token       
	:	  (alphanum | mark | PERCENT | PLUS | BQUOTE  )+;

reserved    
  :   SEMI | SLASH | QMARK | COLON | AT | AND | EQUAL | PLUS
                     | DOLLARD | COMMA;
                     
unreserved :	  alphanum |mark;  
alphanum :	   alpha | DIGIT ;                      
hexdigit 
	:	HEX_CHAR|DIGIT; 
alpha	: HEX_CHAR | COMMON_CHAR; 
 
word        
  :   (alphanum | mark   | PERCENT 
                      | PLUS | BQUOTE |
                     LAQUOT | RAQUOT |
                     COLON | BSLASH | DQUOTE | SLASH | LSBRAQUET | RSBRAQUET | QMARK | LBRACE | RBRACE )+;

mark  :          DASH | USCORE | DOT | EMARK | TILDE | STAR | SQUOTE | LPAREN | RPAREN ; 
hcolon  : ( LWS | HTAB )* COLON  LWS? //SWS;
         ;//|( SP | HTAB )* COLON LWS+;
ldquot  :  LWS? DQUOTE ;
rdquot : DQUOTE LWS?;
semi: LWS? SEMI LWS?;
comma : LWS? COMMA LWS?;
sp_laquot_sp
  : LWS? LAQUOT LWS?;
sp_raquot_sp
  : LWS? RAQUOT LWS?;
equal:
   LWS? EQUAL LWS?;
slash : LWS? SLASH LWS?;