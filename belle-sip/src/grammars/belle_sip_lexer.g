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
lexer grammar belle_sip_lexer;



 
options {	
	language = C;
	
} 
@lexer::header {
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
@lexer::includes { 
#include "belle-sip/belle-sip.h"
#include "belle_sip_internal.h"
}

                 

COMMON_CHAR
	:	'g'..'z' | 'G'..'Z' ;	

HEX_CHAR:	'a'..'f' |'A'..'F';
DIGIT	: '0'..'9' ;
AT: '@';
AND: '&';
DOLLARD: '$';
QMARK: '?';
EMARK: '!';
DASH: '-';
CRLF	: '\r\n' { USER1 = (int)((char*)ctx->pLexer->input->currentLine - (char*)ctx->pLexer->input->data); /*GETCHARINDEX()*/;};
HTAB	: '	';
OR : '|';
PERCENT: '%';
DQUOTE	: '"';
SQUOTE  : '\'';
BQUOTE: '`';
BSLASH: '\\';
LBRACE: '{';
RBRACE: '}';
USCORE: '_';
TILDE: '~';
DOT: '.';

LWS  	:	(SP* CRLF)? SP+ ; //linear whitespace
PLUS: '+';
COLON
	:	':'
	;
SEMI
	:	';'
	;
COMMA
	:	','
	;
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
RSBRAQUET
  : ']'
  ;

LSBRAQUET
  : '['
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
fragment 
SP
	:	' '
	;
OCTET	: . ;	
