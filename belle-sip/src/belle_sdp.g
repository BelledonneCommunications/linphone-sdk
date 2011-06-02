/*
    belle-sdp - SDP (RFC4566) library.
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
grammar belle_sdp;


options {	
	language = C;
	output=AST;
	
} 
@header { 
#include "belle-sip/belle-sdp.h"
#include "belle_sip_internal.h"
}

session_description:    proto_version
                         origin_field
                         session_name_field
                         (info CR LF)?
                         uri_field?
                         (email CR LF)*
                         phone_field*
                         (connection CR LF)?
                         (bandwidth CR LF)*
                         time_field
                         (repeat_time CR LF)?
                         (zone_adjustments CR LF)?
                         (key_field CR LF)?
                         (attribute CR LF)*
                         media_descriptions;

proto_version:       {IS_TOKEN(v)}?alpha_num EQUAL DIGIT+ CR LF;
                       //  ;this memo describes version 0

origin_field:        {IS_TOKEN(o)}?alpha_num EQUAL username SPACE
                         sess_id SPACE sess_version SPACE
                         nettype SPACE addrtype SPACE
                         addr CR LF;

session_name_field:  {IS_TOKEN(s)}? alpha_num EQUAL text CR LF;

info returns [belle_sdp_info_t* ret]     
scope { belle_sdp_info_t* current; }
@init {$info::current = belle_sdp_info_new(); $ret=$info::current; }
:   {IS_TOKEN(i)}? alpha_num EQUAL info_value {belle_sdp_info_set_value($info::current,(const char*) $info_value.text->chars);} ;

info_value            options { greedy = false; }:        ~(CR|LF)*;

uri_field:           {IS_TOKEN(u)}?alpha_num EQUAL uri CR LF;

email returns [belle_sdp_email_t* ret]     
scope { belle_sdp_email_t* current; }
@init {$email::current = belle_sdp_email_new(); $ret=$email::current; }
  :        {IS_TOKEN(e)}?alpha_num EQUAL email_address {belle_sdp_email_set_value($email::current,(const char*)$email_address.text->chars);};

phone_field:        {IS_TOKEN(p)}?alpha_num EQUAL phone_number CR LF;

connection returns [belle_sdp_connection_t* ret]     
scope { belle_sdp_connection_t* current; }
@init {$connection::current = belle_sdp_connection_new(); $ret=$connection::current; }
:    'c=' nettype { belle_sdp_connection_set_network_type($connection::current,(const char*)$nettype.text->chars);} 
                  SPACE addrtype{ belle_sdp_connection_set_address_type($connection::current,(const char*)$addrtype.text->chars);} 
                  SPACE connection_address {belle_sdp_connection_set_address($connection::current,(const char*)$connection_address.text->chars);}
                  ;
                         //;a connection field must be present
                         //;in every media description or at the
                         //;session-level
                         
bandwidth returns [belle_sdp_bandwidth_t* ret]     
scope { belle_sdp_bandwidth_t* current; }
@init {$bandwidth::current = belle_sdp_bandwidth_new(); $ret=$bandwidth::current; }
  :    {IS_TOKEN(b)}?alpha_num EQUAL bwtype {belle_sdp_bandwidth_set_type($bandwidth::current,(const char*)$bwtype.text->chars); } 
      COLON bandwidth_value {belle_sdp_bandwidth_set_value($bandwidth::current,atoi((const char*)$bandwidth_value.text->chars));};

time_field:          {IS_TOKEN(t)}?alpha_num EQUAL start_time SPACE stop_time;

repeat_time:       {IS_TOKEN(r)}?alpha_num EQUAL repeat_interval (SPACE typed_time)+;

zone_adjustments:    sdp_time SPACE '-'? typed_time
                         (SPACE sdp_time SPACE '-'? typed_time)*;

key_field:           {IS_TOKEN(k)}?alpha_num EQUAL key_value ;
key_value options { greedy = false; }:        ~(CR|LF)*;
//key_type:            {IS_TOKEN(prompt)}? alpha_num*  /*'prompt'*/ |
//                     {IS_TOKEN(clear)}? alpha_num* /*'clear'*/  COLON key_data |
//                     {IS_TOKEN(base64)}? alpha_num* /*'base64*/ COLON key_data |
//                     {IS_TOKEN(base64)}? alpha_num* /*'uri*/ COLON uri;
//
//key_data:            email_safe;


attribute returns [belle_sdp_attribute_t* ret]     
scope { belle_sdp_attribute_t* current; }
@init {$attribute::current = belle_sdp_attribute_new(); $ret=$attribute::current; }: {IS_TOKEN(a)}?alpha_num EQUAL attribute_value;

media_descriptions: media_description*; 
media_description:   media CR LF
                     (info CR LF)?
                     (connection CR LF)?
                     (bandwidth CR LF)*
                     key_field ?
                     (attribute CR LF)*;
                       

media returns [belle_sdp_media_t* ret]     
scope { belle_sdp_media_t* current; }
@init {$media::current = belle_sdp_media_new(); $ret=$media::current; }
:         {IS_TOKEN(m)}?alpha_num EQUAL 
          media_value {belle_sdp_media_set_value($media::current,(const char*)$media_value.text->chars);} 
          SPACE port {belle_sdp_media_set_media_port($media::current,atoi((const char*)$port.text->chars));} 
          (SLASH integer{belle_sdp_media_set_port_count($media::current,atoi((const char*)$integer.text->chars));})? 
          SPACE proto {belle_sdp_media_set_media_protocol($media::current,(const char*)$proto.text->chars);}
          (SPACE fmt)?;

media_value:               alpha_num+;
                     //    ;typically "audio", "video", "application"
                     //    ;or "data"

fmt:                 DIGIT+;
                     //;typically an RTP payload type for audio
                     //;and video media

proto              options { greedy = false; }:        ~(SPACE|CR|LF)*;
                     //;typically "RTP/AVP" or "udp" for IP4

port:                DIGIT+;
                     //    ;should in the range "1024" to "65535" inclusive
                     //    ;for UDP based media

attribute_value:           (att_field {belle_sdp_attribute_set_name($attribute::current,(const char*)$att_field.text->chars);} 
                            COLON att_value {belle_sdp_attribute_set_value($attribute::current,(const char*)$att_value.text->chars);}) 
                            | att_field {belle_sdp_attribute_set_name($attribute::current,(const char*)$att_field.text->chars);};

att_field:           alpha_num+;

att_value            options { greedy = false; }:        ~(CR|LF)*;

sess_id:             DIGIT*;
                        // ;should be unique for this originating username/host

sess_version:        DIGIT+;
                         //;0 is a new session

connection_address:  multicast_address
                         | addr;

multicast_address:   (decimal_uchar decimal_uchar decimal_uchar DOT) decimal_uchar '/' ttl  ( '/' integer )?;
                         //;multicast addresses may be in the range
                         //;224.0.0.0 to 239.255.255.255

ttl:                 decimal_uchar;

start_time:           DIGIT+  ;

stop_time:           DIGIT+  ;

sdp_time:                 DIGIT+;
                     //    ;sufficient for 2 more centuries

repeat_interval:     typed_time;

typed_time:          DIGIT* fixed_len_time_unit?;

fixed_len_time_unit: {IS_TOKEN(d)}? alpha_num
                      | {IS_TOKEN(h)}? alpha_num
                      | {IS_TOKEN(m)}? alpha_num
                      | {IS_TOKEN(s)}? alpha_num;

bwtype:              alpha_num+;

bandwidth_value:           DIGIT+;

username:            email_safe;
                         //;pretty wide definition, but doesn't include SPACE

email_address       options { greedy = false; }:        ~(CR|LF)* ;  //| email '(' email_safe ')' |
                         //email_safe '<' email '>';


uri:          text        ;//defined in RFC1630

phone_number:         phone;/*(phone '(') => (phone '(') email_safe ')'
                      | (phone) => phone 
                      | email_safe LQUOTE phone RQUOTE;*/

phone:               text;//'+' DIGIT*POS_DIGIT (SPACE | '-' | DIGIT)*;
                         //;there must be a SPACE or hyphen between the
                         //;international code and the rest of the number.

nettype:             alpha_num+;//'IN';
                        // ;list to be extended

addrtype:            alpha_num+; //'IP4' | 'IP6';
                         //;list to be extended

addr:                (fqdn)=>fqdn | unicast_address ;
  
fqdn:                (alpha_num |'-'|DOT)*;
                     //    ;fully qualified domain name as specified in RFC1035

unicast_address:     ipv4_address ;//| ipv6_address

ipv4_address :         b1 DOT decimal_uchar DOT decimal_uchar DOT b4;
b1:                  decimal_uchar;
                      //;less than "224"; not "0" or "127"
b4:                  decimal_uchar;
                      //;not "0"

//ip6_address :         ;//to be defined

text :                byte_string;
                      //default is to interpret this as IS0-10646 UTF8
                      //ISO 8859-1 requires a "a=charset:ISO-8859-1"
                      //session-level attribute to be used

byte_string options { greedy = false; }:        (.)* ; 
                         //any byte except NUL, CR or LF

decimal_uchar:        d+=DIGIT+ {$d->count<=3}?  ; 

integer:             POS_DIGIT DIGIT*;

email_safe : byte_string;


alpha_num:       ALPHA | DIGIT;


DIGIT:           ZERO     | POS_DIGIT;
fragment ZERO: '0';
fragment POS_DIGIT :           '1'..'9';
ALPHA:               'a'..'z'|'A'..'Z';
SPACE: ' ';

//CRLF  : CR LF { USER1 = (int)(ctx->pLexer->input->currentLine - ctx->pLexer->input->data);};
LQUOTE: '<';
RQUOTE: '>';
CR:'\r';
LF:'\n';
DOT: '.';
EQUAL: '=';
COLON: ':';
SLASH: '/';
ANY_EXCEPT_CR_LF: ~(CR|LF);