#include <stdint.h>

#include "criterion.hpp"

#include "belle-sip/belle-sip.h"

// antlr: 59 µs
// belr: 7 µs
BENCHMARK(SimpleSipUriParsing) {
	static const char *uri = "sip:registrar.biloxi.com";
	belle_sip_uri_parse(uri);
}

// antlr: 59 µs
// belr: 8 µs
BENCHMARK(SimpleSipsUriParsing) {
	static const char *uri = "sips:registrar.biloxi.com";
	belle_sip_uri_parse(uri);
}

// antlr: 12 µs
// belr: 6 µs
BENCHMARK(SimpleSipUriIpv4Parsing) {
	static const char *uri = "sip:192.168.0.1";
	belle_sip_uri_parse(uri);
}

// antlr: 8 µs
// belr: 3 µs
BENCHMARK(SimpleSipUriIpv6Parsing) {
	static const char *uri = "sip:[fe80::1]";
	belle_sip_uri_parse(uri);
}

// antlr: 25 µs
// belr: 10 µs
BENCHMARK(ComplexSipUriParsing) {
	static const char *uri = "sip:alice@atlanta.com:5060;transport=tcp";
	belle_sip_uri_parse(uri);
}

// antlr: 16 µs
// belr: 9 µs
BENCHMARK(ComplexSipUriIpv6Parsing) {
	static const char *uri = "sip:bob@[2a01:e35:1387:1020:6233:4bff:fe0b:5663]:5060;transport=tcp";
	belle_sip_uri_parse(uri);
}

// antlr: 29 µs
// belr: 11 µs
BENCHMARK(SipUriWithPhoneNumberParsing) {
	static const char *uri = "sip:+331231231231@sip.example.org;user=phone";
	belle_sip_uri_parse(uri);
}

// antlr: 24 µs
// belr: 33 µs
BENCHMARK(SipUriWithParametersParsing) {
	static const char *uri =
	    "sip:maddr=@192.168.0.1;lr;maddr=192.168.0.1;user=ip;ttl=140;transport=sctp;method=INVITE;rport=5060";
	belle_sip_uri_parse(uri);
}

// antlr: 75 µs
// belr: 54 µs
BENCHMARK(SipUriWithHeadersParsing) {
	static const char *uri =
	    "sip:eNgwBpkNcH6EdTHlX0cq8@example.org?P-Group-Id=Fu0hHIQ23H4hveVT:New%20Group&P-Expert-Profile-Id="
	    "zKQOBOB2jTmUOjkB:New%20Group&P-Reverse-Charging=0&P-Campaign-Id=none&P-Embed-Url=https://example.org/caller/"
	    "?1.4.0-dev-42-91bdf0c%26id%3DFu0hHIQ23H4hveVT%26CAMPAIGN_ID%3Dnone";
	belle_sip_uri_parse(uri);
}

// antlr: 279 µs
// belr: 162 µs
BENCHMARK(OptionsRequestParsing) {
	static const char *message = "\
OPTIONS sip:carol@chicago.com SIP/2.0\r\n\
Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKhjhs8ass877\r\n\
Max-Forwards: 70\r\n\
To: <sip:carol@chicago.com>\r\n\
From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n\
Call-ID: a84b4c76e66710\r\n\
CSeq: 63104 OPTIONS\r\n\
Contact: <sip:alice@pc33.atlanta.com>\r\n\
Accept: application/sdp\r\n\
Content-Length: 0\r\n\
\r\n";
	belle_sip_message_parse(message);
}

// antlr: 258 µs
// belr: 252 µs
BENCHMARK(OkResponseToOptionsRequestParsing) {
	static const char *message = "\
SIP/2.0 200 OK\r\n\
Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKhjhs8ass877;received=192.0.2.4\r\n\
To: <sip:carol@chicago.com>;tag=93810874\r\n\
From: Alice <sip:alice@atlanta.com>;tag=1928301774\r\n\
Call-ID: a84b4c76e66710\r\n\
CSeq: 63104 OPTIONS\r\n\
Contact: <sip:carol@chicago.com>\r\n\
Contact: <mailto:carol@chicago.com>\r\n\
Allow: INVITE, ACK, CANCEL, OPTIONS, BYE\r\n\
Accept: application/sdp\r\n\
Accept-Encoding: gzip\r\n\
Accept-Language: en\r\n\
Supported: foo\r\n\
Content-Type: application/sdp\r\n\
Content-Length: 274\r\n\
\r\n";
	belle_sip_message_parse(message);
}

// antlr: 186 µs
// belr: 120 µs
BENCHMARK(InviteRequestParsing) {
	static const char *message = "\
INVITE sip:bob@biloxi.com SIP/2.0\r\n\
Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKkjshdyff\r\n\
To: Bob <sip:bob@biloxi.com>\r\n\
From: Alice <sip:alice@atlanta.com>;tag=88sja8x\r\n\
Max-Forwards: 70\r\n\
Call-ID: 987asjd97y7atg\r\n\
CSeq: 986759 INVITE\r\n\
\r\n";
	belle_sip_message_parse(message);
}

// antlr: 171 µs
// belr: 117 µs
BENCHMARK(AckRequestForNon2xxResponseToInviteRequestParsing) {
	static const char *message = "\
ACK sip:bob@biloxi.com SIP/2.0\r\n\
Via: SIP/2.0/UDP pc33.atlanta.com;branch=z9hG4bKkjshdyff\r\n\
To: Bob <sip:bob@biloxi.com>;tag=99sa0xk\r\n\
From: Alice <sip:alice@atlanta.com>;tag=88sja8x\r\n\
Max-Forwards: 70\r\n\
Call-ID: 987asjd97y7atg\r\n\
CSeq: 986759 ACK\r\n\
\r\n";
	belle_sip_message_parse(message);
}

// antlr: 210 µs
// belr: 236 µs
BENCHMARK(DirectCallOverIpv6InviteRequestParsing) {
	static const char *message = "\
INVITE sip:[::1]:34551;transport=tcp SIP/2.0\r\n\
Via: SIP/2.0/TCP [::1]:40870;branch=z9hG4bK.XvHcutOcb;rport\r\n\
From: <sip:bc@[2a01:e0a:27e:8210:d0ca:242:ac11:6]>;tag=EnVHckRZW\r\n\
To: sip:[::1]\r\n\
CSeq: 20 INVITE\r\n\
Call-ID: 8gNJtplN0O\r\n\
Max-Forwards: 70\r\n\
Supported: replaces, outbound, gruu, path\r\n\
Allow: INVITE, ACK, CANCEL, OPTIONS, BYE, REFER, NOTIFY, MESSAGE, SUBSCRIBE, INFO, PRACK, UPDATE\r\n\
Content-Type: application/sdp\r\n\
Content-Length: 261\r\n\
Contact: <sip:[::1]:40870;transport=tcp>;+org.linphone.specs=\"lime\"\r\n\
User-Agent: Unknown\r\n\
\r\n";
	belle_sip_message_parse(message);
}

// antlr: 373 µs
// belr: 321 µs
BENCHMARK(GroupChatNotifyRequestParsing) {
	static const char *message = "\
NOTIFY sip:laure_987xhma@[2a01:e0a:27e:8210:d0ca:242:ac11:6]:50598;transport=tcp SIP/2.0\r\n\
Via: SIP/2.0/TCP sip.example.org;rport;branch=z9hG4bK.XN8m49gUFm7rapNv7Urm4DHQra\r\n\
Via: SIP/2.0/TCP 127.0.0.1:6099;branch=z9hG4bK.rkum5TKqa;rport=50716\r\n\
From: <sip:chatroom-x3q~~zLM7kgZVb2F@conf.example.org;gr=227ee6f2-7305-007b-a8ef-91d80190c1c1>;tag=5vcw~em\r\n\
To: <sip:laure_987xhma@sip.example.org;gr=urn:uuid:1ffae8c7-23cf-4c5d-9680-0b3b37848031>;tag=x3OWQUqIh\r\n\
CSeq: 116 NOTIFY\r\n\
Call-ID: Y-f~ZmLO70\r\n\
Max-Forwards: 69\r\n\
Event: conference\r\n\
Subscription-State: active;expires=600\r\n\
Content-Type: application/conference-info+xml\r\n\
Content-Length: 631\r\n\
Content-Encoding: deflate\r\n\
Contact: <sip:127.0.0.1:6099;transport=tcp>;+org.linphone.specs=\"ephemeral/1.1,groupchat/1.2\"\r\n\
User-Agent: Flexisip-conference/2.3.3-37-g42beb0cf\r\n\
\r\n";
	belle_sip_message_parse(message);
}

// antlr: 2110 µs
// belr: 268 µs
BENCHMARK(SubscribeRequestParsing) {
	static const char *message = "\
SUBSCRIBE sip:marie%20laroueverte_qfsm0vm@sip.example.org;conf-id=jEEp9HfH3y3sxPUqf~wCFXsp3Ds6AGA SIP/2.0\r\n\
Via: SIP/2.0/TLS [2a01:e0a:27e:8210:d0ca:242:ac11:6]:54522;alias;branch=z9hG4bK.vdRpEA9~i;rport\r\n\
From: <sip:chloe_p-tkcud@sip.example.org>;tag=0DglXbedv\r\n\
To: <sip:marie%20laroueverte_qfsm0vm@sip.example.org;conf-id=jEEp9HfH3y3sxPUqf~wCFXsp3Ds6AGA>\r\n\
CSeq: 20 SUBSCRIBE\r\n\
Call-ID: 50qVGs8IbH\r\n\
Max-Forwards: 70\r\n\
Supported: replaces, outbound, gruu, path\r\n\
Event: conference\r\n\
Expires: 600\r\n\
Contact: <sip:chloe_p-tkcud@sip.example.org;gr=urn:uuid:cdac7b83-adb5-413c-a1a0-3947bb740f7b>;+sip.instance=\"<urn:uuid:cdac7b83-adb5-413c-a1a0-3947bb740f7b>\";+org.linphone.specs=\"lime\"\r\n\
Last-Notify-Version: 0\r\n\
User-Agent: Unknown\r\n\
Content-Length: 0\r\n\
\r\n";
	belle_sip_message_parse(message);
}

CRITERION_BENCHMARK_MAIN()