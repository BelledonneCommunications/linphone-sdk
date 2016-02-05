/*
crypto_polarssl.c
Copyright (C) 2016  Belledonne Communications SARL

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
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "utils.h"
#include <bctoolbox/crypto.h>

#include <polarssl/ssl.h>
#include <polarssl/error.h>
#include <polarssl/pem.h>
#include "polarssl/base64.h"
#include <polarssl/x509.h>
#include <polarssl/entropy.h>
#include <polarssl/ctr_drbg.h>
#include <polarssl/sha1.h>
#include <polarssl/sha256.h>
#include <polarssl/sha512.h>

#define bctoolbox_error printf

#ifdef _WIN32
#define snprintf _snprintf
#endif

static int bctoolbox_ssl_sendrecv_callback_return_remap(int32_t ret_code) {
	switch (ret_code) {
		case BCTOOLBOX_ERROR_NET_WANT_READ:
			return POLARSSL_ERR_NET_WANT_READ;
		case BCTOOLBOX_ERROR_NET_WANT_WRITE:
			return POLARSSL_ERR_NET_WANT_WRITE;
		case BCTOOLBOX_ERROR_NET_CONN_RESET:
			return POLARSSL_ERR_NET_CONN_RESET;
		default:
			return (int)ret_code;
	}
}

void bctoolbox_strerror(int32_t error_code, char *buffer, size_t buffer_length) {
	if (error_code>0) {
		snprintf(buffer, buffer_length, "%s", "Invalid Error code");
		return ;
	}

	/* polarssl error code are all negatived and bas smaller than 0x0000F000 */
	/* bctoolbox defined error codes are all in format -0x7XXXXXXX */
	if (-error_code<0x00010000) { /* it's a polarssl error code */
		error_strerror(error_code, buffer, buffer_length);
		return;
	}

	snprintf(buffer, buffer_length, "%s [-0x%x]", "bctoolbox defined error code", -error_code);
	return;
}

int32_t bctoolbox_base64_encode(unsigned char *output, size_t *output_length, const unsigned char *input, size_t input_length) {
	int ret = base64_encode(output, output_length, input, input_length);
	if (ret == POLARSSL_ERR_BASE64_BUFFER_TOO_SMALL) {
		return BCTOOLBOX_ERROR_OUTPUT_BUFFER_TOO_SMALL;
	}
	return ret;
}

int32_t bctoolbox_base64_decode(unsigned char *output, size_t *output_length, const unsigned char *input, size_t input_length) {
	int ret = base64_decode(output, output_length, input, input_length);
	if (ret == POLARSSL_ERR_BASE64_BUFFER_TOO_SMALL) {
		return BCTOOLBOX_ERROR_OUTPUT_BUFFER_TOO_SMALL;
	}
	if (ret == POLARSSL_ERR_BASE64_INVALID_CHARACTER) {
		return BCTOOLBOX_ERROR_INVALID_BASE64_INPUT;
	}

	return ret;
}

/*** Random Number Generation ***/
struct bctoolbox_rng_context_struct {
	entropy_context entropy;
	ctr_drbg_context ctr_drbg;
};

bctoolbox_rng_context_t *bctoolbox_rng_context_new(void) {
	bctoolbox_rng_context_t *ctx = bctoolbox_malloc0(sizeof(bctoolbox_rng_context_t));
	entropy_init(&(ctx->entropy));
	ctr_drbg_init(&(ctx->ctr_drbg), entropy_func, &(ctx->entropy), NULL, 0);
	return ctx;
}

int32_t bctoolbox_rng_get(bctoolbox_rng_context_t *context, unsigned char*output, size_t output_length) {
	return ctr_drbg_random(&(context->ctr_drbg), output, output_length);
}

void bctoolbox_rng_context_free(bctoolbox_rng_context_t *context) {
	ctr_drbg_free(&(context->ctr_drbg));
	entropy_free(&(context->entropy));
	bctoolbox_free(context);
}

/*** signing key ***/
bctoolbox_signing_key_t *bctoolbox_signing_key_new(void) {
	pk_context *key = bctoolbox_malloc0(sizeof(pk_context));
	pk_init(key);
	return (bctoolbox_signing_key_t *)key;
}

void bctoolbox_signing_key_free(bctoolbox_signing_key_t *key) {
	pk_free((pk_context *)key);
	bctoolbox_free(key);
}

char *bctoolbox_signing_key_get_pem(bctoolbox_signing_key_t *key) {
	char *pem_key;
	if (key == NULL) return NULL;
	pem_key = (char *)bctoolbox_malloc0(4096);
	pk_write_key_pem( (pk_context *)key, (unsigned char *)pem_key, 4096);
	return pem_key;
}

int32_t bctoolbox_signing_key_parse(bctoolbox_signing_key_t *key, const char *buffer, size_t buffer_length, const unsigned char *password, size_t password_length) {
	int err;
	err=pk_parse_key((pk_context *)key, (const unsigned char *)buffer, buffer_length+1, password, password_length);
	if(err==0 && !pk_can_do((pk_context *)key, POLARSSL_PK_RSA)) {
		err=POLARSSL_ERR_PK_TYPE_MISMATCH;
	}
	if (err<0) {
		char tmp[128];
		error_strerror(err,tmp,sizeof(tmp));
		bctoolbox_error("cannot parse public key because [%s]",tmp);
		return BCTOOLBOX_ERROR_UNABLE_TO_PARSE_KEY;
	}
	return 0;
}

int32_t bctoolbox_signing_key_parse_file(bctoolbox_signing_key_t *key, const char *path, const char *password) {
	int err;
	err=pk_parse_keyfile((pk_context *)key, path, password);
	if(err==0 && !pk_can_do((pk_context *)key,POLARSSL_PK_RSA)) {
		err=POLARSSL_ERR_PK_TYPE_MISMATCH;
	}
	if (err<0) {
		char tmp[128];
		error_strerror(err,tmp,sizeof(tmp));
		bctoolbox_error("cannot parse public key because [%s]",tmp);
		return BCTOOLBOX_ERROR_UNABLE_TO_PARSE_KEY;
	}
	return 0;
}



/*** Certificate ***/
char *bctoolbox_x509_certificates_chain_get_pem(bctoolbox_x509_certificate_t *cert) {
	char *pem_certificate = NULL;
	size_t olen=0;

	pem_certificate = (char *)bctoolbox_malloc0(4096);
	pem_write_buffer("-----BEGIN CERTIFICATE-----\n", "-----END CERTIFICATE-----\n", ((x509_crt *)cert)->raw.p, ((x509_crt *)cert)->raw.len, (unsigned char*)pem_certificate, 4096, &olen );
	return pem_certificate;
}


bctoolbox_x509_certificate_t *bctoolbox_x509_certificate_new(void) {
	x509_crt *cert = bctoolbox_malloc0(sizeof(x509_crt));
	x509_crt_init(cert);
	return (bctoolbox_x509_certificate_t *)cert;
}

void bctoolbox_x509_certificate_free(bctoolbox_x509_certificate_t *cert) {
	x509_crt_free((x509_crt *)cert);
	bctoolbox_free(cert);
}

int32_t bctoolbox_x509_certificate_get_info_string(char *buf, size_t size, const char *prefix, const bctoolbox_x509_certificate_t *cert) {
	return x509_crt_info(buf, size, prefix, (x509_crt *)cert);
}

int32_t bctoolbox_x509_certificate_parse_file(bctoolbox_x509_certificate_t *cert, const char *path) {
	return x509_crt_parse_file((x509_crt *)cert, path);
}

int32_t bctoolbox_x509_certificate_parse_path(bctoolbox_x509_certificate_t *cert, const char *path) {
	return x509_crt_parse_path((x509_crt *)cert, path);
}

int32_t bctoolbox_x509_certificate_parse(bctoolbox_x509_certificate_t *cert, const char *buffer, size_t buffer_length) {
	return x509_crt_parse((x509_crt *)cert, (const unsigned char *)buffer, buffer_length+1);
}

int32_t bctoolbox_x509_certificate_get_der_length(bctoolbox_x509_certificate_t *cert) {
	if (cert!=NULL) {
		return ((x509_crt *)cert)->raw.len;
	}
	return 0;
}

int32_t bctoolbox_x509_certificate_get_der(bctoolbox_x509_certificate_t *cert, unsigned char *buffer, size_t buffer_length) {
	if (cert==NULL) {
		return BCTOOLBOX_ERROR_INVALID_CERTIFICATE;
	}
	if (((x509_crt *)cert)->raw.len>buffer_length-1) { /* check buffer size is ok, +1 for the NULL termination added at the end */
		return BCTOOLBOX_ERROR_OUTPUT_BUFFER_TOO_SMALL;
	}
	memcpy(buffer, ((x509_crt *)cert)->raw.p, ((x509_crt *)cert)->raw.len);
	buffer[((x509_crt *)cert)->raw.len] = '\0'; /* add a null termination char */

	return 0;
}

int32_t bctoolbox_x509_certificate_get_subject_dn(bctoolbox_x509_certificate_t *cert, char *dn, size_t dn_length) {
	if (cert==NULL) {
		return BCTOOLBOX_ERROR_INVALID_CERTIFICATE;
	}

	return x509_dn_gets(dn, dn_length, &(((x509_crt *)cert)->subject));
}

int32_t bctoolbox_x509_certificate_generate_selfsigned(const char *subject, bctoolbox_x509_certificate_t *certificate, bctoolbox_signing_key_t *pkey, char * pem, size_t pem_length) {
	entropy_context entropy;
	ctr_drbg_context ctr_drbg;
	int ret;
	mpi serial;
	x509write_cert crt;
	char file_buffer[8192];
	size_t file_buffer_len = 0;
	char formatted_subject[512];

	/* subject may be a sip URL or linphone-dtls-default-identity, add CN= before it to make a valid name */
	memcpy(formatted_subject, "CN=", 3);
	memcpy(formatted_subject+3, subject, strlen(subject)+1); /* +1 to get the \0 termination */

	entropy_init( &entropy );
	if( ( ret = ctr_drbg_init( &ctr_drbg, entropy_func, &entropy, NULL, 0 ) )   != 0 )
	{
		bctoolbox_error("Certificate generation can't init ctr_drbg: -%x", -ret);
		return BCTOOLBOX_ERROR_CERTIFICATE_GENERATION_FAIL;
	}

	/* generate 3072 bits RSA public/private key */
	if ( (ret = pk_init_ctx( (pk_context *)pkey, pk_info_from_type( POLARSSL_PK_RSA ) )) != 0) {
		bctoolbox_error("Certificate generation can't init pk_ctx: -%x", -ret);
		return BCTOOLBOX_ERROR_CERTIFICATE_GENERATION_FAIL;
	}

	if ( ( ret = rsa_gen_key( pk_rsa( *(pk_context *)pkey ), ctr_drbg_random, &ctr_drbg, 3072, 65537 ) ) != 0) {
		bctoolbox_error("Certificate generation can't generate rsa key: -%x", -ret);
		return BCTOOLBOX_ERROR_CERTIFICATE_GENERATION_FAIL;
	}

	/* if there is no pem pointer, don't save the key in pem format */
	if (pem!=NULL) {
		pk_write_key_pem((pk_context *)pkey, (unsigned char *)file_buffer, 4096);
		file_buffer_len = strlen(file_buffer);
	}

	/* generate the certificate */
	x509write_crt_init( &crt );
	x509write_crt_set_md_alg( &crt, POLARSSL_MD_SHA256 );

	mpi_init( &serial );

	if ( (ret = mpi_read_string( &serial, 10, "1" ) ) != 0 ) {
		bctoolbox_error("Certificate generation can't read serial mpi: -%x", -ret);
		return BCTOOLBOX_ERROR_CERTIFICATE_GENERATION_FAIL;
	}

	x509write_crt_set_subject_key( &crt, (pk_context *)pkey);
	x509write_crt_set_issuer_key( &crt, (pk_context *)pkey);

	if ( (ret = x509write_crt_set_subject_name( &crt, formatted_subject) ) != 0) {
		bctoolbox_error("Certificate generation can't set subject name: -%x", -ret);
		return BCTOOLBOX_ERROR_CERTIFICATE_GENERATION_FAIL;
	}

	if ( (ret = x509write_crt_set_issuer_name( &crt, formatted_subject) ) != 0) {
		bctoolbox_error("Certificate generation can't set issuer name: -%x", -ret);
		return BCTOOLBOX_ERROR_CERTIFICATE_GENERATION_FAIL;
	}

	if ( (ret = x509write_crt_set_serial( &crt, &serial ) ) != 0) {
		bctoolbox_error("Certificate generation can't set serial: -%x", -ret);
		return BCTOOLBOX_ERROR_CERTIFICATE_GENERATION_FAIL;
	}
	mpi_free(&serial);

	if ( (ret = x509write_crt_set_validity( &crt, "20010101000000", "20300101000000" ) ) != 0) {
		bctoolbox_error("Certificate generation can't set validity: -%x", -ret);
		return BCTOOLBOX_ERROR_CERTIFICATE_GENERATION_FAIL;
	}

	/* store anyway certificate in pem format in a string even if we do not have file to write as we need it to get it in a x509_crt structure */
	if ( (ret = x509write_crt_pem( &crt, (unsigned char *)file_buffer+file_buffer_len, 4096, ctr_drbg_random, &ctr_drbg ) ) != 0) {
		bctoolbox_error("Certificate generation can't write crt pem: -%x", -ret);
		return BCTOOLBOX_ERROR_CERTIFICATE_WRITE_PEM;
	}

	x509write_crt_free(&crt);
	ctr_drbg_free(&ctr_drbg);
	entropy_free(&entropy);

	/* copy the key+cert in pem format into the given buffer */
	if (pem != NULL) {
		if (strlen(file_buffer)+1>pem_length) {
			bctoolbox_error("Certificate generation can't copy the certificate to pem buffer: too short [%ld] but need [%ld] bytes", (long)pem_length, (long)strlen(file_buffer));
			return BCTOOLBOX_ERROR_OUTPUT_BUFFER_TOO_SMALL;
		}
		strncpy(pem, file_buffer, pem_length);
	}

	if ( (ret = x509_crt_parse((x509_crt *)certificate, (unsigned char *)file_buffer, strlen(file_buffer)) ) != 0) {
		bctoolbox_error("Certificate generation can't parse crt pem: -%x", -ret);
		return BCTOOLBOX_ERROR_CERTIFICATE_PARSE_PEM;
	}

	return 0;
}


int32_t bctoolbox_x509_certificate_get_signature_hash_function(const bctoolbox_x509_certificate_t *certificate, bctoolbox_md_type_t *hash_algorithm) {

	x509_crt *crt;
	if (certificate == NULL) return BCTOOLBOX_ERROR_INVALID_CERTIFICATE;

	crt = (x509_crt *)certificate;

	switch (crt->sig_md) {
		case POLARSSL_MD_SHA1:
			*hash_algorithm = BCTOOLBOX_MD_SHA1;
		break;

		case POLARSSL_MD_SHA224:
			*hash_algorithm = BCTOOLBOX_MD_SHA224;
		break;

		case POLARSSL_MD_SHA256:
			*hash_algorithm = BCTOOLBOX_MD_SHA256;
		break;

		case POLARSSL_MD_SHA384:
			*hash_algorithm = BCTOOLBOX_MD_SHA384;
		break;

		case POLARSSL_MD_SHA512:
			*hash_algorithm = BCTOOLBOX_MD_SHA512;
		break;

		default:
			*hash_algorithm = BCTOOLBOX_MD_UNDEFINED;
			return BCTOOLBOX_ERROR_UNSUPPORTED_HASH_FUNCTION;
		break;
	}

	return 0;

}

/* maximum length of returned buffer will be 7(SHA-512 string)+3*hash_length(64)+null char = 200 bytes */
int32_t bctoolbox_x509_certificate_get_fingerprint(const bctoolbox_x509_certificate_t *certificate, char *fingerprint, size_t fingerprint_length, bctoolbox_md_type_t hash_algorithm) {
	unsigned char buffer[64]={0}; /* buffer is max length of returned hash, which is 64 in case we use sha-512 */
	size_t hash_length = 0;
	const char *hash_alg_string=NULL;
	size_t fingerprint_size;
	x509_crt *crt;
	md_type_t hash_id;
	if (certificate == NULL) return BCTOOLBOX_ERROR_INVALID_CERTIFICATE;

	crt = (x509_crt *)certificate;

	/* if there is a specified hash algorithm, use it*/
	switch (hash_algorithm) {
		case BCTOOLBOX_MD_SHA1:
			hash_id = POLARSSL_MD_SHA1;
			break;
		case BCTOOLBOX_MD_SHA224:
			hash_id = POLARSSL_MD_SHA224;
			break;
		case BCTOOLBOX_MD_SHA256:
			hash_id = POLARSSL_MD_SHA256;
			break;
		case BCTOOLBOX_MD_SHA384:
			hash_id = POLARSSL_MD_SHA384;
			break;
		case BCTOOLBOX_MD_SHA512:
			hash_id = POLARSSL_MD_SHA512;
			break;
		default: /* nothing specified, use the hash algo used in the certificate signature */
			hash_id = crt->sig_md;
			break;
	}

	/* fingerprint is a hash of the DER formated certificate (found in crt->raw.p) using the same hash function used by certificate signature */
	switch (hash_id) {
		case POLARSSL_MD_SHA1:
			sha1(crt->raw.p, crt->raw.len, buffer);
			hash_length = 20;
			hash_alg_string="SHA-1";
		break;

		case POLARSSL_MD_SHA224:
			sha256(crt->raw.p, crt->raw.len, buffer, 1); /* last argument is a boolean, indicate to output sha-224 and not sha-256 */
			hash_length = 28;
			hash_alg_string="SHA-224";
		break;

		case POLARSSL_MD_SHA256:
			sha256(crt->raw.p, crt->raw.len, buffer, 0);
			hash_length = 32;
			hash_alg_string="SHA-256";
		break;

		case POLARSSL_MD_SHA384:
			sha512(crt->raw.p, crt->raw.len, buffer, 1); /* last argument is a boolean, indicate to output sha-384 and not sha-512 */
			hash_length = 48;
			hash_alg_string="SHA-384";
		break;

		case POLARSSL_MD_SHA512:
			sha512(crt->raw.p, crt->raw.len, buffer, 1); /* last argument is a boolean, indicate to output sha-384 and not sha-512 */
			hash_length = 64;
			hash_alg_string="SHA-512";
		break;

		default:
			return BCTOOLBOX_ERROR_UNSUPPORTED_HASH_FUNCTION;
		break;
	}

	if (hash_length>0) {
		int i;
		int fingerprint_index = strlen(hash_alg_string);
		char prefix=' ';

		fingerprint_size=fingerprint_index+3*hash_length+1;
		/* fingerprint will be : hash_alg_string+' '+HEX : separated values: length is strlen(hash_alg_string)+3*hash_lenght + 1 for null termination */
		if (fingerprint_length<fingerprint_size) {
			return BCTOOLBOX_ERROR_OUTPUT_BUFFER_TOO_SMALL;
		}

		snprintf(fingerprint, fingerprint_size, "%s", hash_alg_string);
		for (i=0; i<hash_length; i++, fingerprint_index+=3) {
			snprintf((char*)fingerprint+fingerprint_index, fingerprint_size-fingerprint_index, "%c%02X", prefix,buffer[i]);
			prefix=':';
		}
		*(fingerprint+fingerprint_index) = '\0';
	}

	return (int32_t)fingerprint_size;
}

#define BCTOOLBOX_MAX_CERTIFICATE_FLAGS_STRING_LENGTH 128
int32_t bctoolbox_x509_certificate_flags_to_string(char *buffer, size_t buffer_size, uint32_t flags) {
	size_t i=0;
	char outputString[BCTOOLBOX_MAX_CERTIFICATE_FLAGS_STRING_LENGTH];

	if (flags & BADCERT_EXPIRED)
		i+=snprintf(outputString+i, BCTOOLBOX_MAX_CERTIFICATE_FLAGS_STRING_LENGTH-i, "expired ");
	if (flags & BADCERT_REVOKED)
		i+=snprintf(outputString+i, BCTOOLBOX_MAX_CERTIFICATE_FLAGS_STRING_LENGTH-i, "revoked ");
	if (flags & BADCERT_CN_MISMATCH)
		i+=snprintf(outputString+i, BCTOOLBOX_MAX_CERTIFICATE_FLAGS_STRING_LENGTH-i, "CN-mismatch ");
	if (flags & BADCERT_NOT_TRUSTED)
		i+=snprintf(outputString+i, BCTOOLBOX_MAX_CERTIFICATE_FLAGS_STRING_LENGTH-i, "not-trusted ");
	if (flags & BADCERT_MISSING)
		i+=snprintf(outputString+i, BCTOOLBOX_MAX_CERTIFICATE_FLAGS_STRING_LENGTH-i, "missing ");
	if (flags & BADCRL_NOT_TRUSTED)
		i+=snprintf(outputString+i, BCTOOLBOX_MAX_CERTIFICATE_FLAGS_STRING_LENGTH-i, "crl-not-trusted ");
	if (flags & BADCRL_EXPIRED)
		i+=snprintf(outputString+i, BCTOOLBOX_MAX_CERTIFICATE_FLAGS_STRING_LENGTH-i, "crl-expired ");

	outputString[i] = '\0'; /* null terminate the string */

	if (i+1>buffer_size) {
		return BCTOOLBOX_ERROR_OUTPUT_BUFFER_TOO_SMALL;
	}

	strncpy(buffer, outputString, buffer_size);

	return 0;
}

int32_t bctoolbox_x509_certificate_set_flag(uint32_t *flags, uint32_t flags_to_set) {
	if (flags_to_set & BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_EXPIRED)
		*flags |= BADCERT_EXPIRED;
	if (flags_to_set & BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_REVOKED)
		*flags |= BADCERT_REVOKED;
	if (flags_to_set & BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_CN_MISMATCH)
		*flags |= BADCERT_CN_MISMATCH;
	if (flags_to_set & BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_NOT_TRUSTED)
		*flags |= BADCERT_NOT_TRUSTED;
	if (flags_to_set & BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_MISSING)
		*flags |= BADCERT_MISSING;
	if (flags_to_set & BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_NOT_TRUSTED)
		*flags |= BADCRL_NOT_TRUSTED;
	if (flags_to_set & BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_EXPIRED)
		*flags |= BADCRL_EXPIRED;

	return 0;
}

uint32_t bctoolbox_x509_certificate_remap_flag(uint32_t flags) {
	uint32_t ret = 0;
	if (flags & BADCERT_EXPIRED)
		ret |= BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_EXPIRED;
	if (flags & BADCERT_REVOKED)
		ret |= BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_REVOKED;
	if (flags & BADCERT_CN_MISMATCH)
		ret |= BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_CN_MISMATCH;
	if (flags & BADCERT_NOT_TRUSTED)
		ret |= BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_NOT_TRUSTED;
	if (flags & BADCERT_MISSING)
		ret |= BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_MISSING;
	if (flags & BADCRL_NOT_TRUSTED)
		ret |= BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_NOT_TRUSTED;
	if (flags & BADCRL_EXPIRED)
		ret |= BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_EXPIRED;

	return ret;
}

int32_t bctoolbox_x509_certificate_unset_flag(uint32_t *flags, uint32_t flags_to_unset) {
	if (flags_to_unset & BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_EXPIRED)
		*flags &= ~BADCERT_EXPIRED;
	if (flags_to_unset & BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_REVOKED)
		*flags &= ~BADCERT_REVOKED;
	if (flags_to_unset & BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_CN_MISMATCH)
		*flags &= ~BADCERT_CN_MISMATCH;
	if (flags_to_unset & BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_NOT_TRUSTED)
		*flags &= ~BADCERT_NOT_TRUSTED;
	if (flags_to_unset & BCTOOLBOX_CERTIFICATE_VERIFY_BADCERT_MISSING)
		*flags &= ~BADCERT_MISSING;
	if (flags_to_unset & BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_NOT_TRUSTED)
		*flags &= ~BADCRL_NOT_TRUSTED;
	if (flags_to_unset & BCTOOLBOX_CERTIFICATE_VERIFY_BADCRL_EXPIRED)
		*flags &= ~BADCRL_EXPIRED;

	return 0;
}

/*** SSL Client ***/
/** context **/
struct bctoolbox_ssl_context_struct {
	ssl_context ssl_ctx;
	int(*callback_cli_cert_function)(void *, bctoolbox_ssl_context_t *, unsigned char *, size_t); /**< pointer to the callback called to update client certificate during handshake
													callback params are user_data, ssl_context, certificate distinguished name, name length */
	void *callback_cli_cert_data; /**< data passed to the client cert callback */
	int(*callback_send_function)(void *, const unsigned char *, size_t); /* callbacks args are: callback data, data buffer to be send, size of data buffer */
	int(*callback_recv_function)(void *, unsigned char *, size_t); /* args: callback data, data buffer to be read, size of data buffer */
	void *callback_sendrecv_data; /**< data passed to send/recv callbacks */
};

bctoolbox_ssl_context_t *bctoolbox_ssl_context_new(void) {
	bctoolbox_ssl_context_t *ssl_ctx = bctoolbox_malloc0(sizeof(bctoolbox_ssl_context_t));
	ssl_init(&(ssl_ctx->ssl_ctx));
	ssl_ctx->callback_cli_cert_function = NULL;
	ssl_ctx->callback_cli_cert_data = NULL;
	ssl_ctx->callback_send_function = NULL;
	ssl_ctx->callback_recv_function = NULL;
	ssl_ctx->callback_sendrecv_data = NULL;
	return ssl_ctx;
}

void bctoolbox_ssl_context_free(bctoolbox_ssl_context_t *ssl_ctx) {
	ssl_free(&(ssl_ctx->ssl_ctx));
	bctoolbox_free(ssl_ctx);
}

int32_t bctoolbox_ssl_close_notify(bctoolbox_ssl_context_t *ssl_ctx) {
	return ssl_close_notify(&(ssl_ctx->ssl_ctx));
}

int32_t bctoolbox_ssl_session_reset(bctoolbox_ssl_context_t *ssl_ctx) {
	return ssl_session_reset(&(ssl_ctx->ssl_ctx));
}

int32_t bctoolbox_ssl_write(bctoolbox_ssl_context_t *ssl_ctx, const unsigned char *buf, size_t buf_length) {
	int ret = ssl_write(&(ssl_ctx->ssl_ctx), buf, buf_length);
	/* remap some output code */
	if (ret == POLARSSL_ERR_NET_WANT_WRITE) {
		ret = BCTOOLBOX_ERROR_NET_WANT_WRITE;
	}
	return ret;
}

int32_t bctoolbox_ssl_read(bctoolbox_ssl_context_t *ssl_ctx, unsigned char *buf, size_t buf_length) {
	int ret = ssl_read(&(ssl_ctx->ssl_ctx), buf, buf_length);
	/* remap some output code */
	if (ret == POLARSSL_ERR_SSL_PEER_CLOSE_NOTIFY) {
		ret = BCTOOLBOX_ERROR_SSL_PEER_CLOSE_NOTIFY;
	}
	if (ret == POLARSSL_ERR_NET_WANT_READ) {
		ret = BCTOOLBOX_ERROR_NET_WANT_READ;
	}
	return ret;
}

int32_t bctoolbox_ssl_handshake(bctoolbox_ssl_context_t *ssl_ctx) {

	int ret = 0;
	while( ssl_ctx->ssl_ctx.state != SSL_HANDSHAKE_OVER )
	{
		ret = ssl_handshake_step(&(ssl_ctx->ssl_ctx));
		if( ret != 0 ) {
			break;
		}

		/* insert the callback function for client certificate request */
		if (ssl_ctx->callback_cli_cert_function != NULL) { /* check we have a callback function */
			/* when in state SSL_CLIENT_CERTIFICATE - which means, next call to ssl_handshake_step will send the client certificate to server -
			 * and the client_auth flag is set - which means the server requested a client certificate - */
			if (ssl_ctx->ssl_ctx.state == SSL_CLIENT_CERTIFICATE && ssl_ctx->ssl_ctx.client_auth > 0) {
				/* note: polarssl 1.3 is unable to retrieve certificate dn during handshake from server certificate request
				 * so the dn params in the callback are set to NULL and 0(dn string length) */
				if (ssl_ctx->callback_cli_cert_function(ssl_ctx->callback_cli_cert_data, ssl_ctx, NULL, 0)!=0) {
					if((ret=ssl_send_fatal_handshake_failure(&(ssl_ctx->ssl_ctx))) != 0 )
						return( ret );
				}
			}
		}

	}

	/* remap some output codes */
	if (ret == POLARSSL_ERR_NET_WANT_READ) {
		ret = BCTOOLBOX_ERROR_NET_WANT_READ;
	} else if (ret == POLARSSL_ERR_NET_WANT_WRITE) {
		ret = BCTOOLBOX_ERROR_NET_WANT_WRITE;
	}

	return(ret);
}

int32_t bctoolbox_ssl_set_hs_own_cert(bctoolbox_ssl_context_t *ssl_ctx, bctoolbox_x509_certificate_t *cert, bctoolbox_signing_key_t *key) {
	return ssl_set_own_cert(&(ssl_ctx->ssl_ctx) , (x509_crt *)cert , (pk_context *)key);
}

int bctoolbox_ssl_send_callback(void *data, const unsigned char *buffer, size_t buffer_length) {
	int ret = 0;
	/* data is the ssl_context which contains the actual callback and data */
	bctoolbox_ssl_context_t *ssl_ctx = (bctoolbox_ssl_context_t *)data;

	ret = ssl_ctx->callback_send_function(ssl_ctx->callback_sendrecv_data, buffer, buffer_length);

	return bctoolbox_ssl_sendrecv_callback_return_remap(ret);
}

int bctoolbox_ssl_recv_callback(void *data, unsigned char *buffer, size_t buffer_length) {
	int ret = 0;
	/* data is the ssl_context which contains the actual callback and data */
	bctoolbox_ssl_context_t *ssl_ctx = (bctoolbox_ssl_context_t *)data;

	ret = ssl_ctx->callback_recv_function(ssl_ctx->callback_sendrecv_data, buffer, buffer_length);

	return bctoolbox_ssl_sendrecv_callback_return_remap(ret);
}

void bctoolbox_ssl_set_io_callbacks(bctoolbox_ssl_context_t *ssl_ctx, void *callback_data,
		int(*callback_send_function)(void *, const unsigned char *, size_t), /* callbacks args are: callback data, data buffer to be send, size of data buffer */
		int(*callback_recv_function)(void *, unsigned char *, size_t)){ /* args: callback data, data buffer to be read, size of data buffer */

	if (ssl_ctx==NULL) {
		return;
	}

	ssl_ctx->callback_send_function = callback_send_function;
	ssl_ctx->callback_recv_function = callback_recv_function;
	ssl_ctx->callback_sendrecv_data = callback_data;

	ssl_set_bio(&(ssl_ctx->ssl_ctx), bctoolbox_ssl_recv_callback, ssl_ctx, bctoolbox_ssl_send_callback, ssl_ctx);
}

const bctoolbox_x509_certificate_t *bctoolbox_ssl_get_peer_certificate(bctoolbox_ssl_context_t *ssl_ctx) {
	return (const bctoolbox_x509_certificate_t *)ssl_get_peer_cert(&(ssl_ctx->ssl_ctx));
}

/** DTLS SRTP functions **/
#ifdef HAVE_DTLS_SRTP
uint8_t bctoolbox_dtls_srtp_supported(void) {
	return 1;
}

static bctoolbox_dtls_srtp_profile_t bctoolbox_srtp_profile_polarssl2bctoolbox(enum DTLS_SRTP_protection_profiles polarssl_profile) {
	switch (polarssl_profile) {
		case SRTP_AES128_CM_HMAC_SHA1_80:
			return BCTOOLBOX_SRTP_AES128_CM_HMAC_SHA1_80;
		case SRTP_AES128_CM_HMAC_SHA1_32:
			return BCTOOLBOX_SRTP_AES128_CM_HMAC_SHA1_32;
		case SRTP_NULL_HMAC_SHA1_80:
			return BCTOOLBOX_SRTP_NULL_HMAC_SHA1_80;
		case SRTP_NULL_HMAC_SHA1_32:
			return BCTOOLBOX_SRTP_NULL_HMAC_SHA1_32;
		default:
			return BCTOOLBOX_SRTP_UNDEFINED;
	}
}

static enum DTLS_SRTP_protection_profiles bctoolbox_srtp_profile_bctoolbox2polarssl(bctoolbox_dtls_srtp_profile_t bctoolbox_profile) {
	switch (bctoolbox_profile) {
		case BCTOOLBOX_SRTP_AES128_CM_HMAC_SHA1_80:
			return SRTP_AES128_CM_HMAC_SHA1_80;
		case BCTOOLBOX_SRTP_AES128_CM_HMAC_SHA1_32:
			return SRTP_AES128_CM_HMAC_SHA1_32;
		case BCTOOLBOX_SRTP_NULL_HMAC_SHA1_80:
			return SRTP_NULL_HMAC_SHA1_80;
		case BCTOOLBOX_SRTP_NULL_HMAC_SHA1_32:
			return SRTP_NULL_HMAC_SHA1_32;
		default:
			return SRTP_UNSET_PROFILE;
	}
}

bctoolbox_dtls_srtp_profile_t bctoolbox_ssl_get_dtls_srtp_protection_profile(bctoolbox_ssl_context_t *ssl_ctx) {
	if (ssl_ctx==NULL) {
		return BCTOOLBOX_ERROR_INVALID_SSL_CONTEXT;
	}

	return bctoolbox_srtp_profile_polarssl2bctoolbox(ssl_get_dtls_srtp_protection_profile(&(ssl_ctx->ssl_ctx)));
};


int32_t bctoolbox_ssl_get_dtls_srtp_key_material(bctoolbox_ssl_context_t *ssl_ctx, char *output, size_t *output_length) {
	if (ssl_ctx==NULL) {
		return BCTOOLBOX_ERROR_INVALID_SSL_CONTEXT;
	}

	/* check output buffer size */
	if (*output_length<ssl_ctx->ssl_ctx.dtls_srtp_keys_len) {
		return BCTOOLBOX_ERROR_OUTPUT_BUFFER_TOO_SMALL;
	}

	memcpy(output, ssl_ctx->ssl_ctx.dtls_srtp_keys, ssl_ctx->ssl_ctx.dtls_srtp_keys_len);
	*output_length = ssl_ctx->ssl_ctx.dtls_srtp_keys_len;

	return 0;
}
#else /* HAVE_DTLS_SRTP */
/* dummy DTLS api when not available */
uint8_t bctoolbox_dtls_srtp_supported(void) {
	return 0;
}

bctoolbox_dtls_srtp_profile_t bctoolbox_ssl_get_dtls_srtp_protection_profile(bctoolbox_ssl_context_t *ssl_ctx) {
	return BCTOOLBOX_SRTP_UNDEFINED;
}

int32_t bctoolbox_ssl_get_dtls_srtp_key_material(bctoolbox_ssl_context_t *ssl_ctx, char *output, size_t *output_length) {
	*output_length = 0;
	return BCTOOLBOX_ERROR_UNAVAILABLE_FUNCTION;
}
#endif /* HAVE_DTLS_SRTP */

/** DTLS SRTP functions **/

/** config **/
struct bctoolbox_ssl_config_struct {
	int8_t endpoint; /**< BCTOOLBOX_SSL_IS_CLIENT or BCTOOLBOX_SSL_IS_SERVER */
	int8_t authmode; /**< BCTOOLBOX_SSL_VERIFY_NONE, BCTOOLBOX_SSL_VERIFY_OPTIONAL, BCTOOLBOX_SSL_VERIFY_REQUIRED */
	int8_t transport; /**< BCTOOLBOX_SSL_TRANSPORT_STREAM(TLS) or BCTOOLBOX_SSL_TRANSPORT_DATAGRAM(DTLS) */
	int(*rng_function)(void *, unsigned char *, size_t); /**< pointer to a random number generator function */
	void *rng_context; /**< pointer to a the random number generator context */
	int(*callback_verify_function)(void *, x509_crt *, int, int *); /**< pointer to the verify callback function */
	void *callback_verify_data; /**< data passed to the verify callback */
	x509_crt *ca_chain; /**< trusted CA chain */
	char *peer_cn; /**< expected peer Common name */
	x509_crt *own_cert;
	pk_context *own_cert_pk;
	int(*callback_cli_cert_function)(void *, bctoolbox_ssl_context_t *, unsigned char *, size_t); /**< pointer to the callback called to update client certificate during handshake
													callback params are user_data, ssl_context, certificate distinguished name, name length */
	void *callback_cli_cert_data; /**< data passed to the client cert callback */
	enum DTLS_SRTP_protection_profiles dtls_srtp_profiles[4]; /**< Store a maximum of 4 DTLS SRTP profiles to use */
	int dtls_srtp_profiles_number; /**< Number of DTLS SRTP profiles in use */
};

bctoolbox_ssl_config_t *bctoolbox_ssl_config_new(void) {
	int i;
	bctoolbox_ssl_config_t *ssl_config = bctoolbox_malloc0(sizeof(bctoolbox_ssl_config_t));

	/* set all properties to BCTOOLBOX_SSL_UNSET or NULL */
	ssl_config->endpoint = BCTOOLBOX_SSL_UNSET;
	ssl_config->authmode = BCTOOLBOX_SSL_UNSET;
	ssl_config->transport = BCTOOLBOX_SSL_UNSET;
	ssl_config->rng_function = NULL;
	ssl_config->rng_context = NULL;
	ssl_config->callback_verify_function= NULL;
	ssl_config->callback_verify_data = NULL;
	ssl_config->callback_cli_cert_function = NULL;
	ssl_config->callback_cli_cert_data = NULL;
	ssl_config->ca_chain = NULL;
	ssl_config->peer_cn = NULL;
	ssl_config->own_cert = NULL;
	ssl_config->own_cert_pk = NULL;
	for (i=0; i<4; i++) {
		ssl_config->dtls_srtp_profiles[i] = SRTP_UNSET_PROFILE;
	}
	ssl_config->dtls_srtp_profiles_number = 0;

	return ssl_config;
}

int32_t bctoolbox_ssl_config_set_crypto_library_config(bctoolbox_ssl_config_t *ssl_config, void *internal_config) {
	return BCTOOLBOX_ERROR_UNAVAILABLE_FUNCTION;
}

void bctoolbox_ssl_config_free(bctoolbox_ssl_config_t *ssl_config) {
	bctoolbox_free(ssl_config);
}

int32_t bctoolbox_ssl_config_defaults(bctoolbox_ssl_config_t *ssl_config, int endpoint, int transport) {
	if (ssl_config != NULL) {
		if (endpoint == BCTOOLBOX_SSL_IS_CLIENT) {
			ssl_config->endpoint = SSL_IS_CLIENT;
		} else if (endpoint == BCTOOLBOX_SSL_IS_SERVER) {
			ssl_config->endpoint = SSL_IS_SERVER;
		} else {
			return BCTOOLBOX_ERROR_INVALID_SSL_ENDPOINT;
		}
/* useful only for versions of polarssl which have SSL_TRANSPORT_XXX defined */
#ifdef SSL_TRANSPORT_DATAGRAM
		if (transport == BCTOOLBOX_SSL_TRANSPORT_STREAM) {
			ssl_config->transport = SSL_TRANSPORT_STREAM;
		} else if (transport == BCTOOLBOX_SSL_TRANSPORT_DATAGRAM) {
			ssl_config->transport = SSL_TRANSPORT_DATAGRAM;
		} else {
			return BCTOOLBOX_ERROR_INVALID_SSL_TRANSPORT;
		}
#endif
		return 0;
	}
	return BCTOOLBOX_ERROR_INVALID_SSL_CONFIG;
}

int32_t bctoolbox_ssl_config_set_endpoint(bctoolbox_ssl_config_t *ssl_config, int endpoint) {
	if (ssl_config == NULL) {
		return BCTOOLBOX_ERROR_INVALID_SSL_CONFIG;
	}

	if (endpoint == BCTOOLBOX_SSL_IS_CLIENT) {
		ssl_config->endpoint = SSL_IS_CLIENT;
	} else if (endpoint == BCTOOLBOX_SSL_IS_SERVER) {
		ssl_config->endpoint = SSL_IS_SERVER;
	} else {
		return BCTOOLBOX_ERROR_INVALID_SSL_ENDPOINT;
	}

	return 0;
}

int32_t bctoolbox_ssl_config_set_transport (bctoolbox_ssl_config_t *ssl_config, int transport) {
	if (ssl_config == NULL) {
		return BCTOOLBOX_ERROR_INVALID_SSL_CONFIG;
	}

/* useful only for versions of polarssl which have SSL_TRANSPORT_XXX defined */
#ifdef SSL_TRANSPORT_DATAGRAM
		if (transport == BCTOOLBOX_SSL_TRANSPORT_STREAM) {
			ssl_config->transport = SSL_TRANSPORT_STREAM;
		} else if (transport == BCTOOLBOX_SSL_TRANSPORT_DATAGRAM) {
			ssl_config->transport = SSL_TRANSPORT_DATAGRAM;
		} else {
			return BCTOOLBOX_ERROR_INVALID_SSL_TRANSPORT;
		}
#endif
	return 0;
}

int32_t bctoolbox_ssl_config_set_authmode(bctoolbox_ssl_config_t *ssl_config, int authmode) {
	if (ssl_config != NULL) {
		switch (authmode) {
			case BCTOOLBOX_SSL_VERIFY_NONE:
				ssl_config->authmode = SSL_VERIFY_NONE;
				break;
			case BCTOOLBOX_SSL_VERIFY_OPTIONAL:
				ssl_config->authmode = SSL_VERIFY_OPTIONAL;
				break;
			case BCTOOLBOX_SSL_VERIFY_REQUIRED:
				ssl_config->authmode = SSL_VERIFY_REQUIRED;
				break;
			default:
				return BCTOOLBOX_ERROR_INVALID_SSL_AUTHMODE;
				break;
		}
		return 0;
	}
	return BCTOOLBOX_ERROR_INVALID_SSL_CONFIG;
}

int32_t bctoolbox_ssl_config_set_rng(bctoolbox_ssl_config_t *ssl_config, int(*rng_function)(void *, unsigned char *, size_t), void *rng_context) {
	if (ssl_config != NULL) {
		ssl_config->rng_function = rng_function;
		ssl_config->rng_context = rng_context;
	}
	return BCTOOLBOX_ERROR_INVALID_SSL_CONFIG;
}

int32_t bctoolbox_ssl_config_set_callback_verify(bctoolbox_ssl_config_t *ssl_config, int(*callback_function)(void *, bctoolbox_x509_certificate_t *, int, uint32_t *), void *callback_data) {
	if (ssl_config != NULL) {
		ssl_config->callback_verify_function = (int(*)(void *, x509_crt *, int, int *))callback_function;
		ssl_config->callback_verify_data = callback_data;
	}
	return BCTOOLBOX_ERROR_INVALID_SSL_CONFIG;
}

int32_t bctoolbox_ssl_config_set_callback_cli_cert(bctoolbox_ssl_config_t *ssl_config, int(*callback_function)(void *, bctoolbox_ssl_context_t *, unsigned char *, size_t), void *callback_data) {
	if (ssl_config != NULL) {
		ssl_config->callback_cli_cert_function = callback_function;
		ssl_config->callback_cli_cert_data = callback_data;
	}
	return BCTOOLBOX_ERROR_INVALID_SSL_CONFIG;
}

int32_t bctoolbox_ssl_config_set_ca_chain(bctoolbox_ssl_config_t *ssl_config, bctoolbox_x509_certificate_t *ca_chain, char *peer_cn) {
	if (ssl_config != NULL) {
		ssl_config->ca_chain = (x509_crt *)ca_chain;
		ssl_config->peer_cn = peer_cn;
	}
	return BCTOOLBOX_ERROR_INVALID_SSL_CONFIG;
}

int32_t bctoolbox_ssl_config_set_own_cert(bctoolbox_ssl_config_t *ssl_config, bctoolbox_x509_certificate_t *cert, bctoolbox_signing_key_t *key) {
	if (ssl_config != NULL) {
		ssl_config->own_cert = (x509_crt *)cert;
		ssl_config->own_cert_pk = (pk_context *)key;
	}
	return BCTOOLBOX_ERROR_INVALID_SSL_CONFIG;
}


/** DTLS SRTP functions **/
#ifdef HAVE_DTLS_SRTP
int32_t bctoolbox_ssl_config_set_dtls_srtp_protection_profiles(bctoolbox_ssl_config_t *ssl_config, const bctoolbox_dtls_srtp_profile_t *profiles, size_t profiles_number) {
	int i;

	if (ssl_config == NULL) {
		return BCTOOLBOX_ERROR_INVALID_SSL_CONFIG;
	}

	/* convert the profiles array into a polarssl profiles array */
	for (i=0; i<profiles_number && i<4; i++) { /* 4 profiles defined max */
		ssl_config->dtls_srtp_profiles[i] = bctoolbox_srtp_profile_bctoolbox2polarssl(profiles[i]);
	}
	for (;i<4; i++) { /* make sure to have harmless values in the rest of the array */
		ssl_config->dtls_srtp_profiles[i] = SRTP_UNSET_PROFILE;
	}

	ssl_config->dtls_srtp_profiles_number = profiles_number;

	return 0;
}

#else /* HAVE_DTLS_SRTP */
int32_t bctoolbox_ssl_config_set_dtls_srtp_protection_profiles(bctoolbox_ssl_config_t *ssl_config, const bctoolbox_dtls_srtp_profile_t *profiles, size_t profiles_number) {
	return BCTOOLBOX_ERROR_UNAVAILABLE_FUNCTION;
}
#endif /* HAVE_DTLS_SRTP */
/** DTLS SRTP functions **/

int32_t bctoolbox_ssl_context_setup(bctoolbox_ssl_context_t *ssl_ctx, bctoolbox_ssl_config_t *ssl_config) {
	/* Check validity of context and config */
	if (ssl_config == NULL) {
		return BCTOOLBOX_ERROR_INVALID_SSL_CONFIG;
	}

	if (ssl_ctx == NULL) {
		return BCTOOLBOX_ERROR_INVALID_SSL_CONTEXT;
	}

	/* apply all valids settings to the ssl_context */
	if (ssl_config->endpoint != BCTOOLBOX_SSL_UNSET) {
		ssl_set_endpoint(&(ssl_ctx->ssl_ctx), ssl_config->endpoint);
	}

	if (ssl_config->authmode != BCTOOLBOX_SSL_UNSET) {
		ssl_set_authmode(&(ssl_ctx->ssl_ctx), ssl_config->authmode);
	}

	if (ssl_config->transport != BCTOOLBOX_SSL_UNSET) {
		ssl_set_transport(&(ssl_ctx->ssl_ctx), ssl_config->transport);
	}

	if (ssl_config->rng_function != NULL) {
		ssl_set_rng(&(ssl_ctx->ssl_ctx), ssl_config->rng_function, ssl_config->rng_context);
	}

	if (ssl_config->callback_verify_function != NULL) {
		ssl_set_verify(&(ssl_ctx->ssl_ctx), ssl_config->callback_verify_function, ssl_config->callback_verify_data);
	}

	if (ssl_config->callback_cli_cert_function != NULL) {
		ssl_ctx->callback_cli_cert_function = ssl_config->callback_cli_cert_function;
		ssl_ctx->callback_cli_cert_data = ssl_config->callback_cli_cert_data;
	}

	if (ssl_config->ca_chain != NULL) {
		ssl_set_ca_chain(&(ssl_ctx->ssl_ctx), ssl_config->ca_chain, NULL, ssl_config->peer_cn);
	}

	if (ssl_config->own_cert != NULL && ssl_config->own_cert_pk != NULL) {
		ssl_set_own_cert(&(ssl_ctx->ssl_ctx) , ssl_config->own_cert , ssl_config->own_cert_pk);
	}

#ifdef HAVE_DTLS_SRTP
	if (ssl_config->dtls_srtp_profiles_number > 0) {
		ssl_set_dtls_srtp_protection_profiles(&(ssl_ctx->ssl_ctx), ssl_config->dtls_srtp_profiles, ssl_config->dtls_srtp_profiles_number );
	}
#endif

	return 0;
}
