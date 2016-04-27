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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#include <mediastreamer2/stun.h>
#include <bctoolbox/crypto.h>


#define IANA_PROTOCOL_NUMBERS_UDP 17

#define STUN_FLAG_CHANGE_IP   0x04
#define STUN_FLAG_CHANGE_PORT 0x02

#define STUN_MESSAGE_HEADER_LENGTH  20
#define STUN_MAX_USERNAME_LENGTH   513
#define STUN_MAX_REASON_LENGTH     127
#define STUN_MAX_SOFTWARE_LENGTH   763 /* Length in bytes, it is supposed to be less than 128 UTF-8 characters (TODO) */


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


static void stun_message_encoder_init(StunMessageEncoder *encoder, char **buf) {
	memset(encoder, 0, sizeof(StunMessageEncoder));
	encoder->cursize = 128;
	encoder->remaining = encoder->cursize;
	encoder->buffer = ms_malloc(encoder->cursize);
	encoder->ptr = encoder->buffer;
	*buf = encoder->buffer;
}

static void stun_message_encoder_check_size(StunMessageEncoder *encoder, size_t sz) {
	while (encoder->remaining < sz) {
		size_t offset = encoder->ptr - encoder->buffer;
		encoder->cursize *= 2;
		encoder->buffer = ms_realloc(encoder->buffer, encoder->cursize);
		encoder->ptr = encoder->buffer + offset;
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
	encode16(encoder, 8);
	encode8(encoder, 0);
	encode8(encoder, addr->family);
	if (addr->family == MS_STUN_ADDR_FAMILY_IPV6) {
		/* TODO */
	} else {
		encode16(encoder, addr->ipv4.port);
		encode32(encoder, addr->ipv4.addr);
	}
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
	encode16(encoder, len);
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
	encode16(encoder, 4 + reason_len);
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


typedef struct {
	const char *buffer;
	const char *ptr;
	size_t size;
	size_t remaining;
	bool_t error;
} StunMessageDecoder;


static void stun_message_decoder_init(StunMessageDecoder *decoder, const char *buf, size_t bufsize) {
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
	decoder->remaining -= len;
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
	if (length != 8) {
		ms_warning("STUN address attribute with wrong length");
		decoder->error = TRUE;
		goto error;
	}

	decode8(decoder);
	stun_addr.family = decode8(decoder);
	if (stun_addr.family == MS_STUN_ADDR_FAMILY_IPV6) {
		/* TODO */
	} else {
		stun_addr.ipv4.port = decode16(decoder);
		stun_addr.ipv4.addr = decode32(decoder);
	}

error:
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


MSStunAddress4 ms_stun_hostname_to_stun_addr(const char *hostname, uint16_t default_port) {
	char *host;
	char *sep;
	int port = default_port;
	MSStunAddress4 stun_addr;

	memset(&stun_addr, 0, sizeof(stun_addr));
	host = ms_strdup(hostname);
	/* Get the port part if present. */
	sep = strchr(host, ':');
	if (sep != NULL) {
		char *end_ptr = NULL;
		char *portstr = sep + 1;
		*sep = '\0';
		port = strtol(portstr, &end_ptr, 10);
		if (end_ptr != NULL) {
			if (*end_ptr != '\0') port = default_port;
		}
	}

	if (port < 1024) goto error;
	if (port >= 0xFFFF) goto error;
	stun_addr.port = port;

	/* Figure out the host part */
#if	defined(_WIN32) || defined(_WIN32_WCE)
	if (isdigit(host[0])) {
		/* Assume it is a ip address */
		unsigned long a = inet_addr(host);
		stun_addr.addr = ntohl(a);
	} else
		/* Assume it is a host name */
#endif
	{
		struct hostent *h = gethostbyname(host);
		if (h == NULL) {
			stun_addr.addr = ntohl(0x7F000001L);
		} else {
			struct in_addr sin_addr = *(struct in_addr *)h->h_addr;
			stun_addr.addr = ntohl(sin_addr.s_addr);
		}
	}

error:
	ms_free(host);
	return stun_addr;
}

char * ms_stun_calculate_integrity_short_term(const char *buf, size_t bufsize, const char *key) {
	char *hmac = ms_malloc(21);
	memset(hmac, 0, 21);
	/* SHA1 output length is 20 bytes, get them all */
	bctbx_hmacSha1((const unsigned char *)key, strlen(key), (const unsigned char *)buf, bufsize, 20, (unsigned char *)hmac);
	return hmac;
}

char * ms_stun_calculate_integrity_long_term(const char *buf, size_t bufsize, const char *realm, const char *username, const char *password) {
	unsigned char HA1[16];
	char HA1_text[1024];
	char *hmac = ms_malloc(21);

	memset(hmac, 0, 21);
	snprintf(HA1_text, sizeof(HA1_text), "%s:%s:%s", username, realm, password);
	bctbx_md5((unsigned char *)HA1_text, strlen(HA1_text), HA1);

	/* SHA1 output length is 20 bytes, get them all */
	bctbx_hmacSha1(HA1, sizeof(HA1), (const unsigned char *)buf, bufsize, 20, (unsigned char *)hmac);
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

MSStunMessage * ms_stun_message_create_from_buffer_parsing(const char *buf, size_t bufsize) {
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
					MSStunAddress stun_addr = decode_addr(&decoder, length);
					stun_addr.ipv4.addr ^= MS_STUN_MAGIC_COOKIE;
					stun_addr.ipv4.port ^= MS_STUN_MAGIC_COOKIE >> 16;
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
			case MS_TURN_ATTR_XOR_RELAYED_ADDRESS:
				{
					MSStunAddress stun_addr = decode_addr(&decoder, length);
					stun_addr.ipv4.addr ^= MS_STUN_MAGIC_COOKIE;
					stun_addr.ipv4.port ^= MS_STUN_MAGIC_COOKIE >> 16;
					ms_stun_message_set_xor_relayed_address(msg, stun_addr);
				}
				break;
			case MS_TURN_ATTR_LIFETIME:
				ms_stun_message_set_lifetime(msg, decode_lifetime(&decoder, length));
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
	if (msg->password) ms_free(msg->password);
	if (msg->realm) ms_free(msg->realm);
	if (msg->message_integrity) ms_free(msg->message_integrity);
	if (msg->software) ms_free(msg->software);
	ms_free(msg);
}

size_t ms_stun_message_encode(const MSStunMessage *msg, char **buf) {
	StunMessageEncoder encoder;
	const MSStunAddress *stun_addr;
	uint16_t message_length;

	stun_message_encoder_init(&encoder, buf);
	encode_message_header(&encoder, msg->type, msg->method, &msg->tr_id);

	stun_addr = ms_stun_message_get_mapped_address(msg);
	if (stun_addr != NULL) encode_addr(&encoder, MS_STUN_ATTR_MAPPED_ADDRESS, stun_addr);
	if (msg->change_request != 0) encode_change_request(&encoder, msg->change_request);
	if (msg->username != NULL) encode_string(&encoder, MS_STUN_ATTR_USERNAME, msg->username, STUN_MAX_USERNAME_LENGTH);
	if (ms_stun_message_has_error_code(msg)) {
		char *reason = NULL;
		uint16_t number = ms_stun_message_get_error_code(msg, &reason);
		encode_error_code(&encoder, number, reason);
	}
	stun_addr = ms_stun_message_get_xor_mapped_address(msg);
	if (stun_addr != NULL) encode_addr(&encoder, MS_STUN_ATTR_XOR_MAPPED_ADDRESS, stun_addr);

	stun_addr = ms_stun_message_get_xor_relayed_address(msg);
	if (stun_addr != NULL) encode_addr(&encoder, MS_TURN_ATTR_XOR_RELAYED_ADDRESS, stun_addr);
	if (ms_stun_message_has_requested_transport(msg)) encode_requested_transport(&encoder, ms_stun_message_get_requested_transport(msg));

	if (ms_stun_message_has_priority(msg)) encode_priority(&encoder, ms_stun_message_get_priority(msg));
	if (ms_stun_message_use_candidate_enabled(msg)) encode_use_candidate(&encoder);
	if (ms_stun_message_has_ice_controlled(msg))
		encode_ice_control(&encoder, MS_ICE_ATTR_ICE_CONTROLLED, ms_stun_message_get_ice_controlled(msg));
	if (ms_stun_message_has_ice_controlling(msg))
		encode_ice_control(&encoder, MS_ICE_ATTR_ICE_CONTROLLING, ms_stun_message_get_ice_controlling(msg));
	if (ms_stun_message_message_integrity_enabled(msg)) {
		const char *username = ms_stun_message_get_username(msg);
		const char *password = ms_stun_message_get_password(msg);
		if ((username != NULL) && (password != NULL) && (strlen(username) > 0) && (strlen(password) > 0)) {
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
	return message_length;
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

bool_t ms_stun_message_has_requested_transport(const MSStunMessage *msg) {
	return msg->has_requested_transport;
}

uint8_t ms_stun_message_get_requested_transport(const MSStunMessage *msg) {
	return msg->requested_transport;
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
