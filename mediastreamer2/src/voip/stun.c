/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2012-2016  Belledonne Communications

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include <mediastreamer2/stun.h>
#include <bctoolbox/crypto.h>
#include <bctoolbox/port.h>


#define IANA_PROTOCOL_NUMBERS_UDP 17

#define STUN_FLAG_CHANGE_IP   0x04
#define STUN_FLAG_CHANGE_PORT 0x02

#define STUN_MESSAGE_HEADER_LENGTH  20
#define STUN_MAX_USERNAME_LENGTH   513
#define STUN_MAX_REASON_LENGTH     127
#define STUN_MAX_SOFTWARE_LENGTH   763 /* Length in bytes, it is supposed to be less than 128 UTF-8 characters (TODO) */
#define STUN_MAX_REALM_LENGTH      127
#define STUN_MAX_NONCE_LENGTH      127


#define STUN_STR_SETTER(field, value) \
	if ((field) != NULL) ms_free(field); \
	(field) = ((value) == NULL) ? NULL : ms_strdup(value)


#if defined(htonq)
#elif defined(ORTP_BIGENDIAN)
#define htonq(n) n
#define ntohq(n) n
#else /* little endian */
static ORTP_INLINE uint64_t htonq(uint64_t v) {
	return htonl((uint32_t) (v >> 32)) | (uint64_t) htonl((uint32_t) v) << 32;
}
static ORTP_INLINE uint64_t ntohq (uint64_t v) {
	return ntohl((uint32_t) (v >> 32)) | (uint64_t) ntohl((uint32_t) v) << 32;
}
#endif /* little endian */



typedef struct {
	char *buffer;
	char *ptr;
	char *lenptr;
	size_t cursize;
	size_t remaining;
} StunMessageEncoder;


static void stun_message_encoder_init(StunMessageEncoder *encoder) {
	memset(encoder, 0, sizeof(StunMessageEncoder));
	encoder->cursize = 128;
	encoder->remaining = encoder->cursize;
	encoder->buffer = ms_malloc(encoder->cursize);
	encoder->ptr = encoder->buffer;
}

static void stun_message_encoder_check_size(StunMessageEncoder *encoder, size_t sz) {
	while (encoder->remaining < sz) {
		size_t offset = encoder->ptr - encoder->buffer;
		encoder->cursize *= 2;
		encoder->buffer = ms_realloc(encoder->buffer, encoder->cursize);
		encoder->ptr = encoder->buffer + offset;
		encoder->lenptr = encoder->buffer + 2; /* Update pointer to the message length */
		encoder->remaining = encoder->cursize - offset;
	}
}

static size_t stun_message_encoder_get_message_length(const StunMessageEncoder *encoder) {
	return encoder->ptr - encoder->buffer;
}

static void stun_message_encoder_memcpy(StunMessageEncoder *encoder, const void *src, size_t len) {
	stun_message_encoder_check_size(encoder, len);
	memcpy(encoder->ptr, src, len);
	encoder->remaining -= len;
	encoder->ptr += len;
}

static void stun_address_xor(MSStunAddress *addr, const UInt96 *tr_id) {
	if (addr->family == MS_STUN_ADDR_FAMILY_IPV4) {
		addr->ip.v4.addr ^= MS_STUN_MAGIC_COOKIE;
		addr->ip.v4.port ^= MS_STUN_MAGIC_COOKIE >> 16;
	} else if (addr->family == MS_STUN_ADDR_FAMILY_IPV6) {
		int i;
		uint32_t magic_cookie = htonl(MS_STUN_MAGIC_COOKIE);
		for (i = 0; i < 4; i++) {
			addr->ip.v6.addr.octet[i] ^= ((uint8_t *)&magic_cookie)[i];
		}
		for (i = 0; i < 12; i++) {
			addr->ip.v6.addr.octet[i + 4] ^= tr_id->octet[i];
		}
		addr->ip.v6.port ^= MS_STUN_MAGIC_COOKIE >> 16;
	}
}

static void encode8(StunMessageEncoder *encoder, uint8_t data) {
	stun_message_encoder_memcpy(encoder, &data, sizeof(data));
}

static void encode16(StunMessageEncoder *encoder, uint16_t data) {
	uint16_t ndata = htons(data);
	stun_message_encoder_memcpy(encoder, &ndata, sizeof(ndata));
}

static void encode32(StunMessageEncoder *encoder, uint32_t data) {
	uint32_t ndata = htonl(data);
	stun_message_encoder_memcpy(encoder, &ndata, sizeof(ndata));
}

static void encode64(StunMessageEncoder *encoder, uint64_t data) {
	uint64_t ndata = htonq(data);
	stun_message_encoder_memcpy(encoder, &ndata, sizeof(ndata));
}

static void encode(StunMessageEncoder *encoder, const void *src, size_t len) {
	stun_message_encoder_memcpy(encoder, src, len);
}

static void encode_message_length(StunMessageEncoder *encoder, size_t len) {
	uint16_t ndata = htons((uint16_t)len);
	memcpy(encoder->lenptr, &ndata, sizeof(ndata));
}

static void encode_message_header(StunMessageEncoder *encoder, uint16_t type, uint16_t method, const UInt96 *tr_id) {
	encode16(encoder, type | method);
	encoder->lenptr = encoder->ptr;
	encode16(encoder, 0);	/* Initialize length to 0, it will be updated later */
	encode32(encoder, MS_STUN_MAGIC_COOKIE); /* magic cookie */
	encode(encoder, tr_id, sizeof(UInt96));
}

static void encode_addr(StunMessageEncoder *encoder, uint16_t type, const MSStunAddress *addr) {
	encode16(encoder, type);
	if (addr->family == MS_STUN_ADDR_FAMILY_IPV6) {
		encode16(encoder, 20);
	} else {
		encode16(encoder, 8);
	}
	encode8(encoder, 0);
	encode8(encoder, addr->family);
	if (addr->family == MS_STUN_ADDR_FAMILY_IPV6) {
		encode16(encoder, addr->ip.v6.port);
		encode(encoder, &addr->ip.v6.addr, sizeof(UInt128));
	} else {
		encode16(encoder, addr->ip.v4.port);
		encode32(encoder, addr->ip.v4.addr);
	}
}

static void encode_xor_addr(StunMessageEncoder *encoder, uint16_t type, const MSStunAddress *addr, const UInt96 *tr_id) {
	MSStunAddress xor_addr = *addr;
	stun_address_xor(&xor_addr, tr_id);
	encode_addr(encoder, type, &xor_addr);
}

static void encode_change_request(StunMessageEncoder *encoder, uint32_t data) {
	encode16(encoder, MS_STUN_ATTR_CHANGE_REQUEST);
	encode16(encoder, 4);
	encode32(encoder, data);
}

static void encode_string(StunMessageEncoder *encoder, uint16_t type, const char *data, uint16_t max_length) {
	size_t len = strlen(data);
	size_t padding;

	if (len > max_length) {
		len = max_length;
		ms_warning("STUN encoded string truncated");
	}
	padding = 4 - (len % 4);
	encode16(encoder, type);
	encode16(encoder, (uint16_t)len);
	encode(encoder, data, len);
	if (padding < 4) {
		size_t i;
		for (i = 0; i < padding; i++) encode8(encoder, 0);
	}
}

static void encode_error_code(StunMessageEncoder *encoder, uint16_t number, const char *reason) {
	size_t reason_len = 0;
	size_t padding = 4 - (reason_len % 4);
	if (reason != NULL) reason_len = strlen(reason);
	encode16(encoder, MS_STUN_ATTR_ERROR_CODE);
	encode16(encoder, 4 + (uint16_t)reason_len);
	encode16(encoder, 0);
	encode8(encoder, number / 100);
	encode8(encoder, number - ((number / 100) * 100));
	if (reason != NULL) encode(encoder, reason, reason_len);
	if (padding < 4) {
		size_t i;
		for (i = 0; i < padding; i++) encode8(encoder, 0);
	}
}

static void encode_priority(StunMessageEncoder *encoder, uint32_t priority) {
	encode16(encoder, MS_ICE_ATTR_PRIORITY);
	encode16(encoder, 4);
	encode32(encoder, priority);
}

static void encode_use_candidate(StunMessageEncoder *encoder) {
	encode16(encoder, MS_ICE_ATTR_USE_CANDIDATE);
	encode16(encoder, 0);
}

static void encode_ice_control(StunMessageEncoder *encoder, uint16_t type, uint64_t value) {
	encode16(encoder, type);
	encode16(encoder, 8);
	encode64(encoder, value);
}

static void encode_integrity(StunMessageEncoder *encoder, const char *hmac) {
	encode16(encoder, MS_STUN_ATTR_MESSAGE_INTEGRITY);
	encode16(encoder, 20);
	encode(encoder, hmac, 20);
}

static void encode_long_term_integrity_from_ha1(StunMessageEncoder *encoder, const char *ha1_text) {
	char *hmac;
	size_t message_length = stun_message_encoder_get_message_length(encoder);
	encode_message_length(encoder, message_length - STUN_MESSAGE_HEADER_LENGTH + 24);
	hmac = ms_stun_calculate_integrity_long_term_from_ha1(encoder->buffer, message_length, ha1_text);
	encode_integrity(encoder, hmac);
	ms_free(hmac);
}

static void encode_long_term_integrity(StunMessageEncoder *encoder, const char *realm, const char *username, const char *password) {
	char *hmac;
	size_t message_length = stun_message_encoder_get_message_length(encoder);
	encode_message_length(encoder, message_length - STUN_MESSAGE_HEADER_LENGTH + 24);
	hmac = ms_stun_calculate_integrity_long_term(encoder->buffer, message_length, realm, username, password);
	encode_integrity(encoder, hmac);
	ms_free(hmac);
}

static void encode_short_term_integrity(StunMessageEncoder *encoder, const char *password, bool_t dummy) {
	char *hmac;
	size_t message_length = stun_message_encoder_get_message_length(encoder);
	encode_message_length(encoder, message_length - STUN_MESSAGE_HEADER_LENGTH + 24);
	if (dummy) {
		hmac = ms_strdup("hmac-not-implemented");
		ms_warning("hmac not implemented by remote, using dummy integrity hash for stun message");
	} else {
		hmac = ms_stun_calculate_integrity_short_term(encoder->buffer, message_length, password);
	}
	encode_integrity(encoder, hmac);
	ms_free(hmac);
}

static void encode_fingerprint(StunMessageEncoder *encoder) {
	uint32_t fingerprint;
	size_t message_length = stun_message_encoder_get_message_length(encoder);
	encode_message_length(encoder, message_length - STUN_MESSAGE_HEADER_LENGTH + 8);
	fingerprint = ms_stun_calculate_fingerprint(encoder->buffer, message_length);
	encode16(encoder, MS_STUN_ATTR_FINGERPRINT);
	encode16(encoder, 4);
	fingerprint ^= 0x5354554E;
	encode32(encoder, fingerprint);
}

static void encode_requested_transport(StunMessageEncoder *encoder, uint8_t requested_transport) {
	encode16(encoder, MS_TURN_ATTR_REQUESTED_TRANSPORT);
	encode16(encoder, 4);
	encode8(encoder, requested_transport);
	encode8(encoder, 0);
	encode16(encoder, 0);
}

static void encode_requested_address_family(StunMessageEncoder *encoder, uint8_t family) {
	encode16(encoder, MS_TURN_ATTR_REQUESTED_ADDRESS_FAMILY);
	encode16(encoder, 4);
	encode8(encoder, family);
	encode8(encoder, 0);
	encode16(encoder, 0);
}

static void encode_lifetime(StunMessageEncoder *encoder, uint32_t lifetime) {
	encode16(encoder, MS_TURN_ATTR_LIFETIME);
	encode16(encoder, 4);
	encode32(encoder, lifetime);
}

static void encode_channel_number(StunMessageEncoder *encoder, uint16_t channel_number) {
	encode16(encoder, MS_TURN_ATTR_CHANNEL_NUMBER);
	encode16(encoder, 4);
	encode16(encoder, channel_number);
	encode16(encoder, 0);
}

static void encode_data(StunMessageEncoder *encoder, uint8_t *data, uint16_t datalen) {
	size_t padding = 4 - (datalen % 4);
	encode16(encoder, MS_TURN_ATTR_DATA);
	encode16(encoder, datalen);
	encode(encoder, data, datalen);
	if (padding < 4) {
		size_t i;
		for (i = 0; i < padding; i++) encode8(encoder, 0);
	}
}


typedef struct {
	const uint8_t *buffer;
	const uint8_t *ptr;
	ssize_t size;
	ssize_t remaining;
	bool_t error;
} StunMessageDecoder;


static void stun_message_decoder_init(StunMessageDecoder *decoder, const uint8_t *buf, ssize_t bufsize) {
	decoder->buffer = decoder->ptr = buf;
	decoder->size = decoder->remaining = bufsize;
	decoder->error = FALSE;
}

static uint8_t decode8(StunMessageDecoder *decoder) {
	uint8_t value = *((uint8_t *)decoder->ptr);
	decoder->ptr += sizeof(uint8_t);
	decoder->remaining -= sizeof(uint8_t);
	if (decoder->remaining < 0) decoder->error = TRUE;
	return value;
}

static uint16_t decode16(StunMessageDecoder *decoder) {
	uint16_t value = ntohs(*((uint16_t *)decoder->ptr));
	decoder->ptr += sizeof(uint16_t);
	decoder->remaining -= sizeof(uint16_t);
	if (decoder->remaining < 0) decoder->error = TRUE;
	return value;
}

static uint32_t decode32(StunMessageDecoder *decoder) {
	uint32_t value = ntohl(*((uint32_t *)decoder->ptr));
	decoder->ptr += sizeof(uint32_t);
	decoder->remaining -= sizeof(uint32_t);
	if (decoder->remaining < 0) decoder->error = TRUE;
	return value;
}

static uint64_t decode64(StunMessageDecoder *decoder) {
	uint64_t value = ntohq(*((uint64_t *)decoder->ptr));
	decoder->ptr += sizeof(uint64_t);
	decoder->remaining -= sizeof(uint64_t);
	if (decoder->remaining < 0) decoder->error = TRUE;
	return value;
}

static const void * decode(StunMessageDecoder *decoder, size_t len) {
	const void *value = decoder->ptr;
	decoder->ptr += len;
	decoder->remaining -= (ssize_t)len;
	if (decoder->remaining < 0) decoder->error = TRUE;
	return value;
}

static void decode_message_header(StunMessageDecoder *decoder, MSStunMessage *msg) {
	uint16_t type = decode16(decoder);
	uint32_t magic_cookie;
	UInt96 *tr_id;

	msg->type = type & 0x0110;
	msg->method = type & 0x3EEF;
	msg->length = decode16(decoder);
	magic_cookie = decode32(decoder);
	if (magic_cookie != MS_STUN_MAGIC_COOKIE) {
		ms_warning("STUN magic cookie is incorrect");
		decoder->error = TRUE;
		return;
	}
	tr_id = (UInt96 *)decode(decoder, sizeof(UInt96));
	ms_stun_message_set_tr_id(msg, *tr_id);
}

static void decode_attribute_header(StunMessageDecoder *decoder, uint16_t *type ,uint16_t *length) {
	*type = decode16(decoder);
	*length = decode16(decoder);
}

static MSStunAddress decode_addr(StunMessageDecoder *decoder, uint16_t length) {
	MSStunAddress stun_addr;

	memset(&stun_addr, 0, sizeof(stun_addr));
	if ((length != 8) && (length != 20)) {
		ms_warning("STUN address attribute with wrong length");
		decoder->error = TRUE;
		goto error;
	}

	decode8(decoder);
	stun_addr.family = decode8(decoder);
	if (stun_addr.family == MS_STUN_ADDR_FAMILY_IPV6) {
		stun_addr.ip.v6.port = decode16(decoder);
		memcpy(&stun_addr.ip.v6.addr, decode(decoder, sizeof(UInt128)), sizeof(UInt128));
	} else {
		stun_addr.ip.v4.port = decode16(decoder);
		stun_addr.ip.v4.addr = decode32(decoder);
	}

error:
	return stun_addr;
}

static MSStunAddress decode_xor_addr(StunMessageDecoder *decoder, uint16_t length, UInt96 tr_id) {
	MSStunAddress stun_addr = decode_addr(decoder, length);
	stun_address_xor(&stun_addr, &tr_id);
	return stun_addr;
}

static uint32_t decode_change_request(StunMessageDecoder *decoder, uint16_t length) {
	if (length != 4) {
		ms_warning("STUN change address attribute with wrong length");
		decoder->error = TRUE;
		return 0;
	}
	return decode32(decoder);
}

static char * decode_string(StunMessageDecoder *decoder, uint16_t length, uint16_t max_length) {
	char *str;

	if (length > max_length) {
		ms_warning("STUN string attribute too long");
		decoder->error = TRUE;
		return NULL;
	}
	str = ms_malloc(length + 1);
	memcpy(str, decoder->ptr, length);
	str[length] = '\0';
	decoder->ptr += length;
	decoder->remaining -= length;
	if (decoder->remaining < 0) decoder->error = TRUE;
	return str;
}

static uint16_t decode_error_code(StunMessageDecoder *decoder, uint16_t length, char **reason) {
	uint16_t number;
	uint16_t reason_length;
	uint8_t clazz;
	uint8_t code;

	if ((length < 4) || (length > (STUN_MAX_REASON_LENGTH + 4))) {
		ms_warning("STUN error code attribute with wrong length");
		decoder->error = TRUE;
		return 0;
	}

	reason_length = length - 4;
	decode16(decoder);
	clazz = decode8(decoder);
	code = decode8(decoder);
	number = clazz * 100 + code;
	if (reason_length > 0) {
		*reason = ms_malloc(reason_length + 1);
		memcpy(*reason, decode(decoder, reason_length), reason_length);
		(*reason)[reason_length] = '\0';
	}
	return number;
}

static char * decode_message_integrity(StunMessageDecoder *decoder, uint16_t length) {
	char *hmac;

	if (length != 20) {
		ms_warning("STUN message integrity attribute with wrong length");
		decoder->error = TRUE;
		return NULL;
	}
	hmac = ms_malloc(21);
	memcpy(hmac, decode(decoder, 20), 20);
	hmac[20] = '\0';
	return hmac;
}

static uint32_t decode_fingerprint(StunMessageDecoder *decoder, uint16_t length) {
	if (length != 4) {
		ms_warning("STUN fingerprint attribute with wrong length");
		decoder->error = TRUE;
		return 0;
	}
	return decode32(decoder);
}

static uint32_t decode_priority(StunMessageDecoder *decoder, uint16_t length) {
	if (length != 4) {
		ms_warning("STUN priority attribute with wrong length");
		decoder->error = TRUE;
		return 0;
	}
	return decode32(decoder);
}

static uint64_t decode_ice_control(StunMessageDecoder *decoder, uint16_t length) {
	if (length != 8) {
		ms_warning("STUN ice-controlled/ice-controlling attribute with wrong length");
		decoder->error = TRUE;
		return 0;
	}
	return decode64(decoder);
}

static uint32_t decode_lifetime(StunMessageDecoder *decoder, uint16_t length) {
	if (length != 4) {
		ms_warning("STUN lifetime attribute with wrong length");
		decoder->error = TRUE;
		return 0;
	}
	return decode32(decoder);
}

static uint8_t * decode_data(StunMessageDecoder *decoder, uint16_t length) {
	uint8_t *data = ms_malloc(length);
	memcpy(data, decode(decoder, length), length);
	return data;
}


static void ms_stun_address_set_port(MSStunAddress *addr, uint16_t port) {
	if (addr->family == MS_STUN_ADDR_FAMILY_IPV4) {
		addr->ip.v4.port = port;
	} else if (addr->family == MS_STUN_ADDR_FAMILY_IPV6) {
		addr->ip.v6.port = port;
	}
}

bool_t ms_compare_stun_addresses(const MSStunAddress *a1, const MSStunAddress *a2) {
	if (a1->family != a2->family) return TRUE;
	if (a1->family == MS_STUN_ADDR_FAMILY_IPV4) {
		return !((a1->ip.v4.port == a2->ip.v4.port)
		&& (a1->ip.v4.addr == a2->ip.v4.addr));
	} else if (a1->family == MS_STUN_ADDR_FAMILY_IPV6) {
		return !((a1->ip.v6.port == a2->ip.v6.port)
		&& (memcmp(&a1->ip.v6.addr, &a2->ip.v6.addr, sizeof(UInt128)) == 0));
	}
	return TRUE;
}

int ms_stun_family_to_af(int stun_family) {
	if (stun_family == MS_STUN_ADDR_FAMILY_IPV4) return AF_INET;
	else if (stun_family == MS_STUN_ADDR_FAMILY_IPV6) return AF_INET6;
	else return 0;
}

void ms_stun_address_to_sockaddr(const MSStunAddress *stun_addr, struct sockaddr *addr, socklen_t *addrlen) {
	if (stun_addr->family == MS_STUN_ADDR_FAMILY_IPV4) {
		struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
		addr_in->sin_family = AF_INET;
		addr_in->sin_port = htons(stun_addr->ip.v4.port);
		addr_in->sin_addr.s_addr = htonl(stun_addr->ip.v4.addr);
		*addrlen = sizeof(struct sockaddr_in);
	} else if (stun_addr->family == MS_STUN_ADDR_FAMILY_IPV6) {
		struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
		addr_in6->sin6_family = AF_INET6;
		addr_in6->sin6_port = htons(stun_addr->ip.v6.port);
		memcpy(addr_in6->sin6_addr.s6_addr, &stun_addr->ip.v6.addr, sizeof(UInt128));
		*addrlen = sizeof(struct sockaddr_in6);
	} else {
		memset(addr, 0, *addrlen);
	}
}

void ms_sockaddr_to_stun_address(const struct sockaddr *addr, MSStunAddress *stun_addr) {
	if (addr->sa_family == AF_INET) {
		stun_addr->family = MS_STUN_ADDR_FAMILY_IPV4;
		stun_addr->ip.v4.port = ntohs(((const struct sockaddr_in *)addr)->sin_port);
		stun_addr->ip.v4.addr = ntohl(((const struct sockaddr_in *)addr)->sin_addr.s_addr);
	} else if (addr->sa_family == AF_INET6) {
		stun_addr->family = MS_STUN_ADDR_FAMILY_IPV6;
		stun_addr->ip.v6.port = ntohs(((const struct sockaddr_in6 *)addr)->sin6_port);
		memcpy(&stun_addr->ip.v6.addr, ((const struct sockaddr_in6 *)addr)->sin6_addr.s6_addr, sizeof(UInt128));
	} else {
		memset(stun_addr, 0, sizeof(MSStunAddress));
	}
}

MSStunAddress ms_ip_address_to_stun_address(int ai_family, int socktype, const char *hostname, int port) {
	MSStunAddress stun_addr;
	struct addrinfo *res = bctbx_ip_address_to_addrinfo(ai_family, socktype, hostname, port);
	memset(&stun_addr, 0, sizeof(stun_addr));
	if (res){
		ms_sockaddr_to_stun_address(res->ai_addr, &stun_addr);
		bctbx_freeaddrinfo(res);
	}
	return stun_addr;
}

void ms_stun_address_to_ip_address(const MSStunAddress *stun_address, char *ip, size_t ip_size, int *port) {
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	memset(&addr, 0, addrlen);
	ms_stun_address_to_sockaddr(stun_address, (struct sockaddr *)&addr, &addrlen);
	bctbx_sockaddr_to_ip_address((struct sockaddr *)&addr, addrlen, ip, ip_size, port);
}

void ms_stun_address_to_printable_ip_address(const MSStunAddress *stun_address, char *printable_ip, size_t printable_ip_size) {
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	memset(&addr, 0, addrlen);
	ms_stun_address_to_sockaddr(stun_address, (struct sockaddr *)&addr, &addrlen);
	bctbx_sockaddr_to_printable_ip_address((struct sockaddr *)&addr, addrlen, printable_ip, printable_ip_size);
}

char * ms_stun_calculate_integrity_short_term(const char *buf, size_t bufsize, const char *key) {
	char *hmac = ms_malloc(21);
	memset(hmac, 0, 21);
	/* SHA1 output length is 20 bytes, get them all */
	bctbx_hmacSha1((const unsigned char *)key, strlen(key), (const unsigned char *)buf, bufsize, 20, (unsigned char *)hmac);
	return hmac;
}

char * ms_stun_calculate_integrity_long_term_from_ha1(const char *buf, size_t bufsize, const char *ha1_text) {
	unsigned char ha1[16];
	unsigned int i, j;
	char *hmac = ms_malloc(21);
	memset(hmac, 0, 21);
	memset(ha1, 0, sizeof(ha1));
	for (i = 0, j = 0; (i < strlen(ha1_text)) && (j < sizeof(ha1)); i += 2, j++) {
		char buf[5] = { '0', 'x', ha1_text[i], ha1_text[i + 1], '\0' };
		ha1[j] = (unsigned char)strtol(buf, NULL, 0);
	}
	/* SHA1 output length is 20 bytes, get them all */
	bctbx_hmacSha1(ha1, sizeof(ha1), (const unsigned char *)buf, bufsize, 20, (unsigned char *)hmac);
	return hmac;
}

char * ms_stun_calculate_integrity_long_term(const char *buf, size_t bufsize, const char *realm, const char *username, const char *password) {
	unsigned char ha1[16];
	char ha1_text[1024];
	char *hmac = ms_malloc(21);
	memset(hmac, 0, 21);
	snprintf(ha1_text, sizeof(ha1_text), "%s:%s:%s", username, realm, password);
	bctbx_md5((unsigned char *)ha1_text, strlen(ha1_text), ha1);
	/* SHA1 output length is 20 bytes, get them all */
	bctbx_hmacSha1(ha1, sizeof(ha1), (const unsigned char *)buf, bufsize, 20, (unsigned char *)hmac);
	return hmac;
}

uint32_t ms_stun_calculate_fingerprint(const char *buf, size_t bufsize) {
	/*
	 *  COPYRIGHT (C) 1986 Gary S. Brown.  You may use this program, or
	 *  code or tables extracted from it, as desired without restriction.
	 */
	static uint32_t crc32_tab[] = {
		0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
		0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
		0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
		0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
		0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
		0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
		0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
		0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
		0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
		0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
		0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
		0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
		0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
		0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
		0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
		0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
		0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
		0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
		0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
		0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
		0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
		0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
		0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
		0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
		0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
		0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
		0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
		0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
		0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
		0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
		0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
		0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
		0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
		0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
		0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
		0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
		0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
		0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
		0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
		0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
		0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
		0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
		0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
	};
	const uint8_t *p = (uint8_t*)buf;
	uint32_t crc;

	crc = ~0U;
	while (bufsize--)
		crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
	return crc ^ ~0U;
}


MSStunMessage * ms_stun_message_create(uint16_t type, uint16_t method) {
	MSStunMessage *msg = ms_new0(MSStunMessage, 1);
	msg->type = type;
	msg->method = method;
	ms_stun_message_set_random_tr_id(msg);
	return msg;
}

MSStunMessage * ms_stun_message_create_from_buffer_parsing(const uint8_t *buf, ssize_t bufsize) {
	StunMessageDecoder decoder;
	MSStunMessage *msg = NULL;

	if (bufsize < STUN_MESSAGE_HEADER_LENGTH) {
		ms_warning("STUN message too short!");
		goto error;
	}

	msg = ms_new0(MSStunMessage, 1);
	stun_message_decoder_init(&decoder, buf, bufsize);
	decode_message_header(&decoder, msg);
	if (decoder.error) goto error;
	if ((ms_stun_message_get_length(msg) + STUN_MESSAGE_HEADER_LENGTH) != bufsize) {
		ms_warning("STUN message header length does not match message size: %i - %zd",
			ms_stun_message_get_length(msg), bufsize);
		goto error;
	}

	while (decoder.remaining > 0) {
		size_t padding;
		uint16_t type;
		uint16_t length;

		decode_attribute_header(&decoder, &type, &length);
		if (length > decoder.remaining) {
			ms_error("STUN attribute larger than message (attribute type: 0x%4x)", type);
			decoder.error = TRUE;
		}
		if (decoder.error) goto error;
		switch (type) {
			case MS_STUN_ATTR_MAPPED_ADDRESS:
				ms_stun_message_set_mapped_address(msg, decode_addr(&decoder, length));
				break;
			case MS_STUN_ATTR_CHANGE_REQUEST:
				msg->change_request = decode_change_request(&decoder, length);
				break;
			case MS_STUN_ATTR_RESPONSE_ADDRESS:
			case MS_STUN_ATTR_SOURCE_ADDRESS:
			case MS_STUN_ATTR_CHANGED_ADDRESS:
				/* Ignore these deprecated attributes. */
				decode_addr(&decoder, length);
				break;
			case MS_STUN_ATTR_USERNAME:
				{
					char *username = decode_string(&decoder, length, STUN_MAX_USERNAME_LENGTH);
					ms_stun_message_set_username(msg, username);
					if (username != NULL) ms_free(username);
				}
				break;
			case MS_STUN_ATTR_PASSWORD:
				/* Ignore this deprecated attribute. */
				{
					char *password = decode_string(&decoder, length, STUN_MAX_USERNAME_LENGTH);
					if (password != NULL) ms_free(password);
				}
				break;
			case MS_STUN_ATTR_MESSAGE_INTEGRITY:
				msg->message_integrity = decode_message_integrity(&decoder, length);
				msg->has_message_integrity = TRUE;
				if (strcmp(ms_stun_message_get_message_integrity(msg), "hmac-not-implemented") == 0) msg->has_dummy_message_integrity = TRUE;
				break;
			case MS_STUN_ATTR_ERROR_CODE:
				{
					char *reason = NULL;
					uint16_t number = decode_error_code(&decoder, length, &reason);
					ms_stun_message_set_error_code(msg, number, reason);
					if (reason != NULL) ms_free(reason);
				}
				break;
			case MS_STUN_ATTR_XOR_MAPPED_ADDRESS:
				{
					MSStunAddress stun_addr = decode_xor_addr(&decoder, length, ms_stun_message_get_tr_id(msg));
					ms_stun_message_set_xor_mapped_address(msg, stun_addr);
				}
				break;
			case MS_STUN_ATTR_SOFTWARE:
				{
					char *software = decode_string(&decoder, length, STUN_MAX_SOFTWARE_LENGTH);
					ms_stun_message_set_software(msg, software);
					if (software != NULL) ms_free(software);
				}
				break;
			case MS_STUN_ATTR_FINGERPRINT:
				msg->fingerprint = decode_fingerprint(&decoder, length);
				msg->has_fingerprint = TRUE;
				break;
			case MS_ICE_ATTR_PRIORITY:
				ms_stun_message_set_priority(msg, decode_priority(&decoder, length));
				break;
			case MS_ICE_ATTR_USE_CANDIDATE:
				ms_stun_message_enable_use_candidate(msg, TRUE);
				break;
			case MS_ICE_ATTR_ICE_CONTROLLED:
				ms_stun_message_set_ice_controlled(msg, decode_ice_control(&decoder, length));
				break;
			case MS_ICE_ATTR_ICE_CONTROLLING:
				ms_stun_message_set_ice_controlling(msg, decode_ice_control(&decoder, length));
				break;
			case MS_TURN_ATTR_XOR_PEER_ADDRESS:
				{
					MSStunAddress stun_addr = decode_xor_addr(&decoder, length, ms_stun_message_get_tr_id(msg));
					ms_stun_message_set_xor_peer_address(msg, stun_addr);
				}
				break;
			case MS_TURN_ATTR_XOR_RELAYED_ADDRESS:
				{
					MSStunAddress stun_addr = decode_xor_addr(&decoder, length, ms_stun_message_get_tr_id(msg));
					ms_stun_message_set_xor_relayed_address(msg, stun_addr);
				}
				break;
			case MS_TURN_ATTR_LIFETIME:
				ms_stun_message_set_lifetime(msg, decode_lifetime(&decoder, length));
				break;
			case MS_TURN_ATTR_DATA:
				ms_stun_message_set_data(msg, decode_data(&decoder, length), length);
				break;
			case MS_STUN_ATTR_REALM:
				{
					char *realm = decode_string(&decoder, length, STUN_MAX_REALM_LENGTH);
					ms_stun_message_set_realm(msg, realm);
					if (realm != NULL) ms_free(realm);
				}
				break;
			case MS_STUN_ATTR_NONCE:
				{
					char *nonce = decode_string(&decoder, length, STUN_MAX_NONCE_LENGTH);
					ms_stun_message_set_nonce(msg, nonce);
					if (nonce != NULL) ms_free(nonce);
				}
				break;
			default:
				if (type <= 0x7FFF) {
					ms_error("STUN unknown Comprehension-Required attribute: 0x%04x", type);
					goto error;
				} else {
					ms_warning("STUN unknown attribute: 0x%04x", type);
					decode(&decoder, length);
				}
				break;
		}
		if (decoder.error) goto error;
		padding = 4 - (length % 4);
		if (padding < 4) {
			size_t i;
			for (i = 0; i < padding; i++) decode8(&decoder);
			if (decoder.error) goto error;
		}
	}

	return msg;

error:
	if (msg != NULL) {
		ms_free(msg);
	}
	return NULL;
}

MSStunMessage * ms_stun_binding_request_create(void) {
	return ms_stun_message_create(MS_STUN_TYPE_REQUEST, MS_STUN_METHOD_BINDING);
}

MSStunMessage * ms_stun_binding_success_response_create(void) {
	return ms_stun_message_create(MS_STUN_TYPE_SUCCESS_RESPONSE, MS_STUN_METHOD_BINDING);
}

MSStunMessage * ms_stun_binding_error_response_create(void) {
	return ms_stun_message_create(MS_STUN_TYPE_ERROR_RESPONSE, MS_STUN_METHOD_BINDING);
}

MSStunMessage * ms_stun_binding_indication_create(void) {
	return ms_stun_message_create(MS_STUN_TYPE_INDICATION, MS_STUN_METHOD_BINDING);
}

bool_t ms_stun_message_is_request(const MSStunMessage *msg) {
	return (((msg->type) & 0x0110) == MS_STUN_TYPE_REQUEST) ? TRUE : FALSE;
}

bool_t ms_stun_message_is_success_response(const MSStunMessage *msg) {
	return (((msg->type) & 0x0110) == MS_STUN_TYPE_SUCCESS_RESPONSE) ? TRUE : FALSE;
}

bool_t ms_stun_message_is_error_response(const MSStunMessage *msg) {
	return (((msg->type) & 0x0110) == MS_STUN_TYPE_ERROR_RESPONSE) ? TRUE : FALSE;
}

bool_t ms_stun_message_is_indication(const MSStunMessage *msg) {
	return (((msg->type) & 0x0110) == MS_STUN_TYPE_INDICATION) ? TRUE : FALSE;
}

void ms_stun_message_destroy(MSStunMessage *msg) {
	if (msg->username) ms_free(msg->username);
	if (msg->password) {
		memset(msg->password, '\0', strlen(msg->password));
		ms_free(msg->password);
	}
	if (msg->ha1) ms_free(msg->ha1);
	if (msg->realm) ms_free(msg->realm);
	if (msg->nonce) ms_free(msg->nonce);
	if (msg->message_integrity) ms_free(msg->message_integrity);
	if (msg->software) ms_free(msg->software);
	if (msg->error_code.reason) ms_free(msg->error_code.reason);
	if (msg->data) ms_free(msg->data);
	ms_free(msg);
}

size_t ms_stun_message_encode(const MSStunMessage *msg, char **buf) {
	StunMessageEncoder encoder;
	const MSStunAddress *stun_addr;
	size_t message_length;

	stun_message_encoder_init(&encoder);
	encode_message_header(&encoder, msg->type, msg->method, &msg->tr_id);

	stun_addr = ms_stun_message_get_mapped_address(msg);
	if (stun_addr != NULL) encode_addr(&encoder, MS_STUN_ATTR_MAPPED_ADDRESS, stun_addr);
	if (msg->change_request != 0) encode_change_request(&encoder, msg->change_request);
	if (msg->username != NULL) encode_string(&encoder, MS_STUN_ATTR_USERNAME, msg->username, STUN_MAX_USERNAME_LENGTH);
	if (msg->realm != NULL) encode_string(&encoder, MS_STUN_ATTR_REALM, msg->realm, STUN_MAX_REALM_LENGTH);
	if (msg->nonce != NULL) encode_string(&encoder, MS_STUN_ATTR_NONCE, msg->nonce, STUN_MAX_NONCE_LENGTH);
	if (ms_stun_message_has_error_code(msg)) {
		char *reason = NULL;
		uint16_t number = ms_stun_message_get_error_code(msg, &reason);
		encode_error_code(&encoder, number, reason);
	}
	stun_addr = ms_stun_message_get_xor_mapped_address(msg);
	if (stun_addr != NULL) encode_xor_addr(&encoder, MS_STUN_ATTR_XOR_MAPPED_ADDRESS, stun_addr, &msg->tr_id);

	stun_addr = ms_stun_message_get_xor_peer_address(msg);
	if (stun_addr != NULL) encode_xor_addr(&encoder, MS_TURN_ATTR_XOR_PEER_ADDRESS, stun_addr, &msg->tr_id);
	stun_addr = ms_stun_message_get_xor_relayed_address(msg);
	if (stun_addr != NULL) encode_xor_addr(&encoder, MS_TURN_ATTR_XOR_RELAYED_ADDRESS, stun_addr, &msg->tr_id);
	if (ms_stun_message_has_requested_transport(msg)) encode_requested_transport(&encoder, ms_stun_message_get_requested_transport(msg));
	if (ms_stun_message_has_requested_address_family(msg)) encode_requested_address_family(&encoder, ms_stun_message_get_requested_address_family(msg));
	if (ms_stun_message_has_lifetime(msg)) encode_lifetime(&encoder, ms_stun_message_get_lifetime(msg));
	if (ms_stun_message_has_channel_number(msg)) encode_channel_number(&encoder, ms_stun_message_get_channel_number(msg));
	if ((ms_stun_message_get_data(msg) != NULL) && (ms_stun_message_get_data_length(msg) > 0))
		encode_data(&encoder, ms_stun_message_get_data(msg), ms_stun_message_get_data_length(msg));

	if (ms_stun_message_has_priority(msg)) encode_priority(&encoder, ms_stun_message_get_priority(msg));
	if (ms_stun_message_use_candidate_enabled(msg)) encode_use_candidate(&encoder);
	if (ms_stun_message_has_ice_controlled(msg))
		encode_ice_control(&encoder, MS_ICE_ATTR_ICE_CONTROLLED, ms_stun_message_get_ice_controlled(msg));
	if (ms_stun_message_has_ice_controlling(msg))
		encode_ice_control(&encoder, MS_ICE_ATTR_ICE_CONTROLLING, ms_stun_message_get_ice_controlling(msg));
	if (ms_stun_message_message_integrity_enabled(msg)) {
		const char *username = ms_stun_message_get_username(msg);
		const char *password = ms_stun_message_get_password(msg);
		if (msg->ha1 != NULL) {
			encode_long_term_integrity_from_ha1(&encoder, msg->ha1);
		} else if ((username != NULL) && (password != NULL) && (strlen(username) > 0) && (strlen(password) > 0)) {
			const char *realm = ms_stun_message_get_realm(msg);
			if ((realm != NULL) && (strlen(realm) > 0)) {
				encode_long_term_integrity(&encoder, realm, username, password);
			} else {
				encode_short_term_integrity(&encoder, password, ms_stun_message_dummy_message_integrity_enabled(msg));
			}
		}
	}
	if (ms_stun_message_fingerprint_enabled(msg)) encode_fingerprint(&encoder);

	message_length = stun_message_encoder_get_message_length(&encoder);
	encode_message_length(&encoder, message_length - STUN_MESSAGE_HEADER_LENGTH);
	*buf = encoder.buffer;
	return message_length;
}

uint16_t ms_stun_message_get_method(const MSStunMessage *msg) {
	return msg->method;
}

uint16_t ms_stun_message_get_length(const MSStunMessage *msg) {
	return msg->length;
}

UInt96 ms_stun_message_get_tr_id(const MSStunMessage *msg) {
	return  msg->tr_id;
}

void ms_stun_message_set_tr_id(MSStunMessage *msg, UInt96 tr_id) {
	msg->tr_id = tr_id;
}

void ms_stun_message_set_random_tr_id(MSStunMessage *msg) {
	UInt96 tr_id;
	int i;

	for (i = 0; i < 12; i += 4) {
		unsigned int r = ortp_random();
		tr_id.octet[i + 0] = r >> 0;
		tr_id.octet[i + 1] = r >> 8;
		tr_id.octet[i + 2] = r >> 16;
		tr_id.octet[i + 3] = r >> 24;
	}
	ms_stun_message_set_tr_id(msg, tr_id);
}

const char * ms_stun_message_get_username(const MSStunMessage *msg) {
	return msg->username;
}

void ms_stun_message_set_username(MSStunMessage *msg, const char *username) {
	STUN_STR_SETTER(msg->username, username);
	msg->include_username_attribute = TRUE;
}

void ms_stun_message_include_username_attribute(MSStunMessage *msg, bool_t include) {
	msg->include_username_attribute = include;
}

const char * ms_stun_message_get_password(const MSStunMessage *msg) {
	return msg->password;
}

void ms_stun_message_set_password(MSStunMessage *msg, const char *password) {
	STUN_STR_SETTER(msg->password, password);
}

void ms_stun_message_set_ha1(MSStunMessage *msg, const char *ha1) {
	STUN_STR_SETTER(msg->ha1, ha1);
}

const char * ms_stun_message_get_realm(const MSStunMessage *msg) {
	return msg->realm;
}

void ms_stun_message_set_realm(MSStunMessage *msg, const char *realm) {
	STUN_STR_SETTER(msg->realm, realm);
}

const char * ms_stun_message_get_software(const MSStunMessage *msg) {
	return msg->software;
}

void ms_stun_message_set_software(MSStunMessage *msg, const char *software) {
	STUN_STR_SETTER(msg->software, software);
}

const char * ms_stun_message_get_nonce(const MSStunMessage *msg) {
	return msg->nonce;
}

void ms_stun_message_set_nonce(MSStunMessage *msg, const char *nonce) {
	STUN_STR_SETTER(msg->nonce, nonce);
}

bool_t ms_stun_message_has_error_code(const MSStunMessage *msg) {
	return msg->has_error_code;
}

uint16_t ms_stun_message_get_error_code(const MSStunMessage *msg, char **reason) {
	if (reason != NULL) {
		*reason = msg->error_code.reason;
	}
	return msg->error_code.number;
}

void ms_stun_message_set_error_code(MSStunMessage *msg, uint16_t number, const char *reason) {
	msg->error_code.number = number;
	STUN_STR_SETTER(msg->error_code.reason, reason);
	msg->has_error_code = TRUE;
}

bool_t ms_stun_message_message_integrity_enabled(const MSStunMessage *msg) {
	return msg->has_message_integrity;
}

void ms_stun_message_enable_message_integrity(MSStunMessage *msg, bool_t enable) {
	msg->has_message_integrity = enable;
}

const char * ms_stun_message_get_message_integrity(const MSStunMessage *msg) {
	return msg->message_integrity;
}

bool_t ms_stun_message_fingerprint_enabled(const MSStunMessage *msg) {
	return msg->has_fingerprint;
}

void ms_stun_message_enable_fingerprint(MSStunMessage *msg, bool_t enable) {
	msg->has_fingerprint = enable;
}

const MSStunAddress * ms_stun_message_get_mapped_address(const MSStunMessage *msg) {
	if (msg->has_mapped_address) return &msg->mapped_address;
	return NULL;
}

void ms_stun_message_set_mapped_address(MSStunMessage *msg, MSStunAddress mapped_address) {
	msg->mapped_address = mapped_address;
	msg->has_mapped_address = TRUE;
}

const MSStunAddress * ms_stun_message_get_xor_mapped_address(const MSStunMessage *msg) {
	if (msg->has_xor_mapped_address) return &msg->xor_mapped_address;
	return NULL;
}

void ms_stun_message_set_xor_mapped_address(MSStunMessage *msg, MSStunAddress xor_mapped_address) {
	msg->xor_mapped_address = xor_mapped_address;
	msg->has_xor_mapped_address = TRUE;
}

const MSStunAddress * ms_stun_message_get_xor_peer_address(const MSStunMessage *msg) {
	if (msg->has_xor_peer_address) return &msg->xor_peer_address;
	return NULL;
}

void ms_stun_message_set_xor_peer_address(MSStunMessage *msg, MSStunAddress xor_peer_address) {
	msg->xor_peer_address = xor_peer_address;
	msg->has_xor_peer_address = TRUE;
}

const MSStunAddress * ms_stun_message_get_xor_relayed_address(const MSStunMessage *msg) {
	if (msg->has_xor_relayed_address) return &msg->xor_relayed_address;
	return NULL;
}

void ms_stun_message_set_xor_relayed_address(MSStunMessage *msg, MSStunAddress xor_relayed_address) {
	msg->xor_relayed_address = xor_relayed_address;
	msg->has_xor_relayed_address = TRUE;
}

void ms_stun_message_enable_change_ip(MSStunMessage *msg, bool_t enable) {
	if (enable) msg->change_request |= STUN_FLAG_CHANGE_IP;
	else {
		uint32_t mask = STUN_FLAG_CHANGE_IP;
		msg->change_request &= ~mask;
	}
}

void ms_stun_message_enable_change_port(MSStunMessage *msg, bool_t enable) {
	if (enable) msg->change_request |= STUN_FLAG_CHANGE_PORT;
	else {
		uint32_t mask = STUN_FLAG_CHANGE_PORT;
		msg->change_request &= ~mask;
	}
}

bool_t ms_stun_message_has_priority(const MSStunMessage *msg) {
	return msg->has_priority;
}

uint32_t ms_stun_message_get_priority(const MSStunMessage *msg) {
	return msg->priority;
}

void ms_stun_message_set_priority(MSStunMessage *msg, uint32_t priority) {
	msg->priority = priority;
	msg->has_priority = TRUE;
}

bool_t ms_stun_message_use_candidate_enabled(const MSStunMessage *msg) {
	return msg->has_use_candidate;
}

void ms_stun_message_enable_use_candidate(MSStunMessage *msg, bool_t enable) {
	msg->has_use_candidate = enable;
}

bool_t ms_stun_message_has_ice_controlling(const MSStunMessage *msg) {
	return msg->has_ice_controlling;
}

uint64_t ms_stun_message_get_ice_controlling(const MSStunMessage *msg) {
	return msg->ice_controlling;
}

void ms_stun_message_set_ice_controlling(MSStunMessage *msg, uint64_t value) {
	msg->ice_controlling = value;
	msg->has_ice_controlling = TRUE;
}

bool_t ms_stun_message_has_ice_controlled(const MSStunMessage *msg) {
	return msg->has_ice_controlled;
}

uint64_t ms_stun_message_get_ice_controlled(const MSStunMessage *msg) {
	return msg->ice_controlled;
}

void ms_stun_message_set_ice_controlled(MSStunMessage *msg, uint64_t value) {
	msg->ice_controlled = value;
	msg->has_ice_controlled = TRUE;
}

bool_t ms_stun_message_dummy_message_integrity_enabled(const MSStunMessage *msg) {
	return msg->has_dummy_message_integrity;
}

void ms_stun_message_enable_dummy_message_integrity(MSStunMessage *msg, bool_t enable) {
	msg->has_dummy_message_integrity = enable;
}

MSStunMessage * ms_turn_allocate_request_create(void) {
	MSStunMessage *msg = ms_stun_message_create(MS_STUN_TYPE_REQUEST, MS_TURN_METHOD_ALLOCATE);
	msg->requested_transport = IANA_PROTOCOL_NUMBERS_UDP;
	msg->has_requested_transport = TRUE;
	return msg;
}

MSStunMessage * ms_turn_refresh_request_create(uint32_t lifetime) {
	MSStunMessage *msg = ms_stun_message_create(MS_STUN_TYPE_REQUEST, MS_TURN_METHOD_REFRESH);
	ms_stun_message_set_lifetime(msg, lifetime);
	return msg;
}

MSStunMessage * ms_turn_create_permission_request_create(MSStunAddress peer_address) {
	MSStunMessage *msg = ms_stun_message_create(MS_STUN_TYPE_REQUEST, MS_TURN_METHOD_CREATE_PERMISSION);
	ms_stun_message_set_xor_peer_address(msg, peer_address);
	return msg;
}

MSStunMessage * ms_turn_send_indication_create(MSStunAddress peer_address) {
	MSStunMessage *msg = ms_stun_message_create(MS_STUN_TYPE_INDICATION, MS_TURN_METHOD_SEND);
	ms_stun_message_set_xor_peer_address(msg, peer_address);
	return msg;
}

MSStunMessage * ms_turn_channel_bind_request_create(MSStunAddress peer_address, uint16_t channel_number) {
	MSStunMessage *msg = ms_stun_message_create(MS_STUN_TYPE_REQUEST, MS_TURN_METHOD_CHANNEL_BIND);
	ms_stun_message_set_xor_peer_address(msg, peer_address);
	ms_stun_message_set_channel_number(msg, channel_number);
	return msg;
}

bool_t ms_stun_message_has_requested_transport(const MSStunMessage *msg) {
	return msg->has_requested_transport;
}

uint8_t ms_stun_message_get_requested_transport(const MSStunMessage *msg) {
	return msg->requested_transport;
}

bool_t ms_stun_message_has_requested_address_family(const MSStunMessage *msg) {
	return msg->has_requested_address_family;
}

uint8_t ms_stun_message_get_requested_address_family(const MSStunMessage *msg) {
	return msg->requested_address_family;
}

void ms_stun_message_set_requested_address_family(MSStunMessage *msg, uint8_t family) {
	msg->requested_address_family = family;
	msg->has_requested_address_family = TRUE;
}

bool_t ms_stun_message_has_lifetime(const MSStunMessage *msg) {
	return msg->has_lifetime;
}

uint32_t ms_stun_message_get_lifetime(const MSStunMessage *msg) {
	return msg->lifetime;
}

void ms_stun_message_set_lifetime(MSStunMessage *msg, uint32_t lifetime) {
	msg->lifetime = lifetime;
	msg->has_lifetime = TRUE;
}

bool_t ms_stun_message_has_channel_number(const MSStunMessage *msg) {
	return msg->has_channel_number;
}

uint16_t ms_stun_message_get_channel_number(const MSStunMessage *msg) {
	return msg->channel_number;
}

void ms_stun_message_set_channel_number(MSStunMessage *msg, uint16_t channel_number) {
	msg->channel_number = channel_number;
	msg->has_channel_number = TRUE;
}

uint8_t * ms_stun_message_get_data(const MSStunMessage *msg) {
	return msg->data;
}

uint16_t ms_stun_message_get_data_length(const MSStunMessage *msg) {
	return msg->data_length;
}

void ms_stun_message_set_data(MSStunMessage *msg, uint8_t *data, uint16_t length) {
	if (msg->data != NULL) {
		ms_free(msg->data);
		msg->data = NULL;
	}
	msg->data = data;
	msg->data_length = length;
}


MSTurnContext * ms_turn_context_new(MSTurnContextType type, RtpSession *rtp_session) {
	MSTurnContext *context = ms_new0(MSTurnContext, 1);
	context->state = MS_TURN_CONTEXT_STATE_IDLE;
	context->type = type;
	context->rtp_session = rtp_session;
	return context;
}

void ms_turn_context_destroy(MSTurnContext *context) {
	if (context->realm != NULL) ms_free(context->realm);
	if (context->nonce != NULL) ms_free(context->nonce);
	if (context->username != NULL) ms_free(context->username);
	if (context->password != NULL) {
		memset(context->password, '\0', strlen(context->password));
		ms_free(context->password);
	}
	if (context->ha1 != NULL) ms_free(context->ha1);
	if (context->endpoint != NULL) context->endpoint->data = NULL;
	bctbx_list_for_each(context->allowed_peer_addresses, (MSIterateFunc)ms_free);
	bctbx_list_free(context->allowed_peer_addresses);
	ms_free(context);
}

void ms_turn_context_set_server_addr(MSTurnContext *context, struct sockaddr *addr, socklen_t addrlen) {
	context->turn_server_addr = addr;
	context->turn_server_addrlen = addrlen;
}

MSTurnContextState ms_turn_context_get_state(const MSTurnContext *context) {
	return context->state;
}

void ms_turn_context_set_state(MSTurnContext *context, MSTurnContextState state) {
	context->state = state;
	if (state == MS_TURN_CONTEXT_STATE_ALLOCATION_CREATED) context->stats.nb_successful_allocate++;
	else if (state == MS_TURN_CONTEXT_STATE_CHANNEL_BOUND) context->stats.nb_successful_channel_bind++;
}

const char * ms_turn_context_get_realm(const MSTurnContext *context) {
	return context->realm;
}

void ms_turn_context_set_realm(MSTurnContext *context, const char *realm) {
	STUN_STR_SETTER(context->realm, realm);
}

const char * ms_turn_context_get_nonce(const MSTurnContext *context) {
	return context->nonce;
}

void ms_turn_context_set_nonce(MSTurnContext *context, const char *nonce) {
	STUN_STR_SETTER(context->nonce, nonce);
}

const char * ms_turn_context_get_username(const MSTurnContext *context) {
	return context->username;
}

void ms_turn_context_set_username(MSTurnContext *context, const char *username) {
	STUN_STR_SETTER(context->username, username);
}

const char * ms_turn_context_get_password(const MSTurnContext *context) {
	return context->password;
}

void ms_turn_context_set_password(MSTurnContext *context, const char *password) {
	STUN_STR_SETTER(context->password, password);
}

const char * ms_turn_context_get_ha1(const MSTurnContext *context) {
	return context->ha1;
}

void ms_turn_context_set_ha1(MSTurnContext *context, const char *ha1) {
	STUN_STR_SETTER(context->ha1, ha1);
}

uint32_t ms_turn_context_get_lifetime(const MSTurnContext *context) {
	return context->lifetime;
}

void ms_turn_context_set_lifetime(MSTurnContext *context, uint32_t lifetime) {
	context->lifetime = lifetime;
}

uint16_t ms_turn_context_get_channel_number(const MSTurnContext *context) {
	return context->channel_number;
}

void ms_turn_context_set_channel_number(MSTurnContext *context, uint16_t channel_number) {
	context->channel_number = channel_number;
}

void ms_turn_context_set_allocated_relay_addr(MSTurnContext *context, MSStunAddress relay_addr) {
	context->relay_addr = relay_addr;
}

void ms_turn_context_set_force_rtp_sending_via_relay(MSTurnContext *context, bool_t force) {
	context->force_rtp_sending_via_relay = force;
}

bool_t ms_turn_context_peer_address_allowed(const MSTurnContext *context, const MSStunAddress *peer_address) {
	bctbx_list_t *elem = context->allowed_peer_addresses;
	while (elem != NULL) {
		MSStunAddress *allowed_peer = (MSStunAddress *)elem->data;
		if (ms_compare_stun_addresses(allowed_peer, peer_address) == FALSE) return TRUE;
		elem = elem->next;
	}
	return FALSE;
}

void ms_turn_context_allow_peer_address(MSTurnContext *context, const MSStunAddress *peer_address) {
	if (!ms_turn_context_peer_address_allowed(context, peer_address)) {
		MSStunAddress *new_peer = ms_malloc(sizeof(MSStunAddress));
		memcpy(new_peer, peer_address, sizeof(MSStunAddress));
		context->allowed_peer_addresses = bctbx_list_append(context->allowed_peer_addresses, new_peer);
		context->stats.nb_successful_create_permission++;
	}
}

static int ms_turn_rtp_endpoint_recvfrom(RtpTransport *rtptp, mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen) {
	MSTurnContext *context = (MSTurnContext *)rtptp->data;
	int msgsize = 0;

	if ((context != NULL) && (context->rtp_session != NULL)) {
		msgsize = rtp_session_recvfrom(context->rtp_session, context->type == MS_TURN_CONTEXT_TYPE_RTP, msg, flags, from, fromlen);
		if ((msgsize >= RTP_FIXED_HEADER_SIZE) && (rtp_get_version(msg) != 2)) {
			/* This is not a RTP packet, try to see if it is a TURN ChannelData message */
			if ((ms_turn_context_get_state(context) >= MS_TURN_CONTEXT_STATE_BINDING_CHANNEL) && (*msg->b_rptr & 0x40)) {
				uint16_t channel = ntohs(*((uint16_t *)msg->b_rptr));
				uint16_t datasize = ntohs(*(((uint16_t *)msg->b_rptr) + 1));
				if ((channel == ms_turn_context_get_channel_number(context)) && (msgsize >= (datasize + 4))) {
					msg->b_rptr += 4; /* Unpack the TURN ChannelData message */
					context->stats.nb_received_channel_msg++;
				}
			} else {
				/* This is not a RTP packet and not a TURN ChannelData message, try to see if it is a STUN one */
				uint16_t stunlen = ntohs(*((uint16_t*)(msg->b_rptr + sizeof(uint16_t))));
				if (msgsize == (stunlen + 20)) {
					/* It seems to be a STUN packet */
					MSStunMessage *stun_msg = ms_stun_message_create_from_buffer_parsing(msg->b_rptr, msgsize);
					if (stun_msg != NULL) {
						if (ms_stun_message_is_indication(stun_msg)
							&& (ms_stun_message_get_data(stun_msg) != NULL) && (ms_stun_message_get_data_length(stun_msg) > 0)) {
							/* This is TURN data indication */
							const MSStunAddress *stun_addr = ms_stun_message_get_xor_peer_address(stun_msg);
							if (stun_addr != NULL) {
								MSStunAddress permission_addr = *stun_addr;
								ms_stun_address_set_port(&permission_addr, 0);
								if (ms_turn_context_peer_address_allowed(context, &permission_addr) == TRUE) {
									struct sockaddr_storage relay_ss;
									struct sockaddr *relay_sa = (struct sockaddr *)&relay_ss;
									socklen_t relay_sa_len = sizeof(relay_ss);
									memset(relay_sa, 0, relay_sa_len);
									/* Copy the data of the TURN data indication in the mblk_t so that it contains the unpacked data */
									msgsize = ms_stun_message_get_data_length(stun_msg);
									memcpy(msg->b_rptr, ms_stun_message_get_data(stun_msg), msgsize);
									/* Overwrite the ortp_recv_addr of the mblk_t so that ICE source address is correct */
									ms_stun_address_to_sockaddr(&context->relay_addr, relay_sa, &relay_sa_len);
									msg->recv_addr.family = relay_sa->sa_family;
									if (relay_sa->sa_family == AF_INET) {
										msg->recv_addr.addr.ipi_addr = ((struct sockaddr_in *)relay_sa)->sin_addr;
										msg->recv_addr.port = ((struct sockaddr_in *)relay_sa)->sin_port;
									} else if (relay_sa->sa_family == AF_INET6) {
										memcpy(&msg->recv_addr.addr.ipi6_addr, &((struct sockaddr_in6 *)relay_sa)->sin6_addr, sizeof(struct in6_addr));
										msg->recv_addr.port = ((struct sockaddr_in6 *)relay_sa)->sin6_port;
									} else {
										ms_warning("turn: Unknown address family in relay_addr");
										msgsize = 0;
									}
									/* Overwrite the source address of the packet so that it uses the peer address instead of the TURN server one */
									ms_stun_address_to_sockaddr(stun_addr, from, fromlen);
									if (msgsize > 0) context->stats.nb_data_indication++;
								}
							}
						}
						ms_stun_message_destroy(stun_msg);
					}
				}
			}
		}
	}
	return msgsize;
}

static bool_t ms_turn_rtp_endpoint_send_via_turn_server(MSTurnContext *context, const struct sockaddr *from, socklen_t fromlen) {
	struct sockaddr_storage relay_ss;
	struct sockaddr *relay_sa = (struct sockaddr *)&relay_ss;
	socklen_t relay_sa_len = sizeof(relay_ss);

	memset(relay_sa, 0, relay_sa_len);
	ms_stun_address_to_sockaddr(&context->relay_addr, relay_sa, &relay_sa_len);
	if (relay_sa->sa_family != from->sa_family) return FALSE;
	if (relay_sa->sa_family == AF_INET) {
		struct sockaddr_in *relay_sa_in = (struct sockaddr_in *)relay_sa;
		struct sockaddr_in *from_in = (struct sockaddr_in *)from;
		return (relay_sa_in->sin_port == from_in->sin_port) && (relay_sa_in->sin_addr.s_addr == from_in->sin_addr.s_addr);
	} else if (relay_sa->sa_family == AF_INET6) {
		struct sockaddr_in6 *relay_sa_in6 = (struct sockaddr_in6 *)relay_sa;
		struct sockaddr_in6 *from_in6 = (struct sockaddr_in6 *)from;
		return (relay_sa_in6->sin6_port == from_in6->sin6_port) && (memcmp(&relay_sa_in6->sin6_addr, &from_in6->sin6_addr, sizeof(struct in6_addr)) == 0);
	} else return FALSE;
}

static int ms_turn_rtp_endpoint_sendto(RtpTransport *rtptp, mblk_t *msg, int flags, const struct sockaddr *to, socklen_t tolen) {
	MSTurnContext *context = (MSTurnContext *)rtptp->data;
	MSStunMessage *stun_msg = NULL;
	bool_t rtp_packet = FALSE;
	int ret = 0;
	mblk_t *new_msg = NULL;

	if ((context != NULL) && (context->rtp_session != NULL)) {
		if ((msgdsize(msg) >= RTP_FIXED_HEADER_SIZE) && (rtp_get_version(msg) == 2)) rtp_packet = TRUE;
		if ((rtp_packet && context->force_rtp_sending_via_relay) || ms_turn_rtp_endpoint_send_via_turn_server(context, (struct sockaddr *)&msg->net_addr, msg->net_addrlen)) {
			if (ms_turn_context_get_state(context) >= MS_TURN_CONTEXT_STATE_CHANNEL_BOUND) {
				/* Use a TURN ChannelData message */
				new_msg = allocb(4, 0);
				*((uint16_t *)new_msg->b_wptr) = htons(ms_turn_context_get_channel_number(context));
				new_msg->b_wptr += 2;
				*((uint16_t *)new_msg->b_wptr) = htons((uint16_t)msgdsize(msg));
				new_msg->b_wptr += 2;
				mblk_meta_copy(msg, new_msg);
				concatb(new_msg, dupmsg(msg));
				msg = new_msg;
				context->stats.nb_sent_channel_msg++;
			} else {
				/* Use a TURN send indication to encapsulate the data to be sent */
				struct sockaddr_storage realto;
				socklen_t realtolen = sizeof(realto);
				MSStunAddress stun_addr;
				char *buf = NULL;
				size_t len;
				uint8_t *data;
				uint16_t datalen;
				msgpullup(msg, -1);
				datalen = (uint16_t)(msg->b_wptr - msg->b_rptr);
				bctbx_sockaddr_ipv6_to_ipv4(to, (struct sockaddr *)&realto, &realtolen);
				ms_sockaddr_to_stun_address((struct sockaddr *)&realto, &stun_addr);
				stun_msg = ms_turn_send_indication_create(stun_addr);
				data = ms_malloc(datalen);
				memcpy(data, msg->b_rptr, datalen);
				ms_stun_message_set_data(stun_msg, data, datalen);
				msg->b_rptr = msg->b_wptr;
				len = ms_stun_message_encode(stun_msg, &buf);
				msgappend(msg, buf, len, FALSE);
				ms_free(buf);
				context->stats.nb_send_indication++;
			}
			to = (const struct sockaddr *)context->turn_server_addr;
			tolen = context->turn_server_addrlen;
		}
		ret = rtp_session_sendto(context->rtp_session, context->type == MS_TURN_CONTEXT_TYPE_RTP, msg, flags, to, tolen);
	}
	if (stun_msg != NULL) ms_stun_message_destroy(stun_msg);
	if (new_msg != NULL) {
		freemsg(new_msg);
	}
	return ret;
}

static void ms_turn_rtp_endpoint_close(RtpTransport *rtptp) {
	MSTurnContext *context = (MSTurnContext *)rtptp->data;
	if (context != NULL) context->rtp_session = NULL;
}

static void ms_turn_rtp_endpoint_destroy(RtpTransport *rtptp) {
	ms_free(rtptp);
}

RtpTransport * ms_turn_context_create_endpoint(MSTurnContext *context) {
	RtpTransport *rtptp = ms_new0(RtpTransport, 1);
	rtptp->t_getsocket = NULL;
	rtptp->t_recvfrom = ms_turn_rtp_endpoint_recvfrom;
	rtptp->t_sendto = ms_turn_rtp_endpoint_sendto;
	rtptp->t_close = ms_turn_rtp_endpoint_close;
	rtptp->t_destroy = ms_turn_rtp_endpoint_destroy;
	rtptp->data = context;
	context->endpoint = rtptp;
	return rtptp;
}
