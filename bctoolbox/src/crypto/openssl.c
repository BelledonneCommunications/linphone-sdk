/*
 * Copyright (c) 2023 Belledonne Communications SARL.
 *
 * This file is part of bctoolbox.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "bctoolbox/crypto.h"
#include "bctoolbox/defs.h"
#include "bctoolbox/logging.h"

#include "openssl/core_names.h"
#include "openssl/crypto.h"
#include "openssl/err.h"
#include "openssl/evp.h"
#include "openssl/kdf.h"
#include "openssl/objects.h"
#include "openssl/pem.h"
#include "openssl/ssl.h"
#include "openssl/x509.h"
#include "openssl/x509v3.h"

#include <dirent.h>
#include <string.h>

#define OPENSSL_CLIENT_CERT_CALLBACK_EX_DATA_IDX 42
#define OPENSSL_BCTBX_SSL_CTX_EX_DATA_IDX 43

/*** Cleaning ***/
/**
 * @brief force a buffer value to zero in a way that shall prevent the compiler from optimizing it out
 *
 * @param[in/out]	buffer	the buffer to be cleared
 * @param[in]		size	buffer size
 */
void bctbx_clean(void *buffer, size_t size) {
	OPENSSL_cleanse(buffer, size);
}

/*** Error code translation ***/
void bctbx_strerror(int32_t error_code, char *buffer, size_t buffer_length) {
	if (error_code <= -0x70000000L && error_code > -0x80000000L) {
		/* bctoolbox defined error codes are all in format -0x7XXXXXXX */
		snprintf(buffer, buffer_length, "%s [-0x%x]", "bctoolbox defined error code", -error_code);
	} else {
		/* it's a openssl error code */
		/* https://www.openssl.org/docs/man3.1/man3/ERR_put_error.html */
		ERR_error_string_n(error_code, buffer, buffer_length);
	}
}

/*** base64 ***/
int32_t
bctbx_base64_encode(unsigned char *output, size_t *output_length, const unsigned char *input, size_t input_length) {
	size_t calculated_output_length = 4 * (input_length / 3);
	size_t calculated_output_padding = (input_length % 3) > 0 ? 4 : 0;
	int byte_written = 0;

	if (output == NULL) {
		*output_length = calculated_output_length + calculated_output_padding;
	} else {
		if (*output_length < (calculated_output_length + calculated_output_padding)) {
			return BCTBX_ERROR_OUTPUT_BUFFER_TOO_SMALL;
		}
		byte_written = EVP_EncodeBlock(output, input, input_length);
		*output_length = byte_written > 0 ? byte_written : 0;
	}
	if (byte_written == -1) {
		return BCTBX_ERROR_INVALID_BASE64_INPUT;
	}

	return 0;
}

int32_t
bctbx_base64_decode(unsigned char *output, size_t *output_length, const unsigned char *input, size_t input_length) {
	// Add padding if needed
	size_t missingPaddingSize = 0;
	unsigned char *paddedInput = NULL;
	if ((input_length % 4) != 0) {
		missingPaddingSize = 4 - (input_length % 4);
		if (output != NULL) {
			paddedInput = bctbx_malloc(input_length + 2);
			memcpy(paddedInput, input, input_length);

			if (missingPaddingSize > 0) {
				paddedInput[input_length] = '=';
			}
			if (missingPaddingSize > 1) {
				paddedInput[input_length + 1] = '=';
			}
		}
	}

	size_t calculated_output_length = 3 * ((input_length + missingPaddingSize) / 4);
	int byte_written = 0;

	if (output == NULL) {
		*output_length = calculated_output_length;
		return 0;
	}
	if (*output_length < calculated_output_length) {
		if (paddedInput != NULL) {
			bctbx_free(paddedInput);
		}
		return BCTBX_ERROR_OUTPUT_BUFFER_TOO_SMALL;
	}

	byte_written =
	    EVP_DecodeBlock(output, paddedInput == NULL ? input : paddedInput, input_length + missingPaddingSize);
	if (paddedInput != NULL) {
		bctbx_free(paddedInput);
	}
	if (byte_written == -1) {
		return BCTBX_ERROR_INVALID_BASE64_INPUT;
	}

	if (missingPaddingSize) {
		byte_written -= missingPaddingSize;
	}
	if (*(input + input_length - 1) == '=') {
		byte_written--;
		if (*(input + input_length - 2) == '=') {
			byte_written--;
		}
	}
	*output_length = byte_written > 0 ? byte_written : 0;

	return 0;
}

/*** signing key ***/
typedef struct bctbx_signing_key_struct {
	EVP_PKEY *evp_pkey;
} bctbx_signing_key_t;

bctbx_signing_key_t *bctbx_signing_key_new(void) {
	bctbx_signing_key_t *key = bctbx_malloc(sizeof(bctbx_signing_key_t));
	key->evp_pkey = EVP_PKEY_new();
	return (bctbx_signing_key_t *)key;
}

void bctbx_signing_key_free(bctbx_signing_key_t *key) {
	if (key && key->evp_pkey) {
		EVP_PKEY_free(key->evp_pkey);
	}
	if (key) {
		bctbx_free(key);
	}
}

char *bctbx_signing_key_get_pem(bctbx_signing_key_t *key) {
	if (!key && !key->evp_pkey) return NULL;
	const int pem_key_length = 4096;
	char *pem_key = (char *)bctbx_malloc0(pem_key_length);
	BIO *buffer = BIO_new(BIO_s_mem());
	if (PEM_write_bio_PrivateKey(buffer, key->evp_pkey, NULL, NULL, 0, NULL, NULL) &&
	    BIO_read(buffer, pem_key, pem_key_length)) {
		BIO_free(buffer);
		return pem_key;
	}
	bctbx_clean(pem_key, pem_key_length);
	bctbx_free(pem_key);
	BIO_free(buffer);
	return NULL;
}

int32_t bctbx_signing_key_parse(bctbx_signing_key_t *key,
                                const char *buffer,
                                size_t buffer_length,
                                const unsigned char *password,
                                size_t password_length) {
	(void)password_length;
	BIO *bio_buffer = BIO_new_mem_buf(buffer, buffer_length);
	EVP_PKEY *ret = PEM_read_bio_PrivateKey(bio_buffer, &(key->evp_pkey), NULL, (void *)password);
	BIO_free(bio_buffer);
	return ret == NULL ? BCTBX_ERROR_UNABLE_TO_PARSE_KEY : 0;
}

int32_t bctbx_signing_key_parse_file(bctbx_signing_key_t *key, const char *path, const char *password) {
	FILE *key_file = fopen(path, "rb");
	EVP_PKEY *ret = NULL;
	if (key_file != NULL) {
		ret = PEM_read_PrivateKey(key_file, &(key->evp_pkey), NULL, (void *)password);
		fclose(key_file);
	}
	return ret == NULL ? BCTBX_ERROR_UNABLE_TO_PARSE_KEY : 0;
}

/*** Certificate ***/
char *bctbx_x509_certificates_chain_get_pem(const bctbx_x509_certificate_t *cert) {
	BIO *buffer = BIO_new(BIO_s_mem());
	if (buffer == NULL) {
		return NULL;
	}

	for (int i = 0; i < sk_X509_num((STACK_OF(X509) *)cert); i++) {
		X509 *certificate = sk_X509_value((STACK_OF(X509) *)cert, i);
		if (certificate != NULL && !PEM_write_bio_X509(buffer, (X509 *)certificate)) {
			BIO_free(buffer);
			return NULL;
		}
	}

	char *pem_certificate = (char *)bctbx_malloc0(BIO_number_written(buffer) + 1);
	if (!BIO_read(buffer, pem_certificate, BIO_number_written(buffer))) {
		bctbx_free(pem_certificate);
		pem_certificate = NULL;
	}

	BIO_free(buffer);
	return pem_certificate;
}

bctbx_x509_certificate_t *bctbx_x509_certificate_new(void) {
	return (bctbx_x509_certificate_t *)sk_X509_new_null();
}

void bctbx_x509_certificate_free(bctbx_x509_certificate_t *cert) {
	sk_X509_pop_free((STACK_OF(X509) *)cert, X509_free);
}

int32_t bctbx_x509_certificate_get_info_string(BCTBX_UNUSED(char *buf),
                                               BCTBX_UNUSED(size_t size),
                                               BCTBX_UNUSED(const char *prefix),
                                               BCTBX_UNUSED(const bctbx_x509_certificate_t *cert)) {
	return 0; // TODO: implement
}

unsigned long stack_x509_info_to_x509(STACK_OF(X509) * crt_stack, const STACK_OF(X509_INFO) * crt_info_stack) {
	for (int i = 0; i < sk_X509_INFO_num(crt_info_stack); i++) {
		X509 *certificate = sk_X509_INFO_value(crt_info_stack, i)->x509;
		if (certificate != NULL) {
			if (sk_X509_push(crt_stack, certificate)) {
				X509_up_ref(certificate);
			} else {
				return ERR_get_error();
			}
		}
	}
	return 0;
}

int32_t bctbx_x509_certificate_parse_file(bctbx_x509_certificate_t *cert, const char *path) {
	unsigned long ret = BCTBX_ERROR_INVALID_INPUT_DATA;
	STACK_OF(X509_INFO) *stackOfParsedPemObjects = NULL;
	FILE *cert_file = fopen(path, "rb");
	if (cert_file != NULL) {
		stackOfParsedPemObjects = PEM_X509_INFO_read(cert_file, NULL, NULL, NULL);
		fclose(cert_file);
	}
	if (stackOfParsedPemObjects != NULL) {
		ret = stack_x509_info_to_x509((STACK_OF(X509) *)cert, stackOfParsedPemObjects);
		sk_X509_INFO_pop_free(stackOfParsedPemObjects, X509_INFO_free);
	}
	return ret;
}

int32_t bctbx_x509_certificate_parse_path(bctbx_x509_certificate_t *cert, const char *path) {
	DIR *directory = opendir(path);
	if (directory == NULL) {
		return BCTBX_ERROR_INVALID_INPUT_DATA;
	}

	struct dirent *entry;
	while ((entry = readdir(directory)) != NULL) {
		if (entry->d_type == DT_REG) { // Only process regular files
			char file_path[PATH_MAX];
			snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
			int32_t ret;
			if ((ret = bctbx_x509_certificate_parse_file(cert, file_path)) != 0) {
				return ret;
			}
		}
	}

	closedir(directory);
	return 0;
}

int32_t bctbx_x509_certificate_parse(bctbx_x509_certificate_t *cert, const char *buffer, size_t buffer_length) {
	unsigned long ret = BCTBX_ERROR_INVALID_INPUT_DATA;
	STACK_OF(X509_INFO) *stackOfParsedPemObjects = NULL;
	BIO *bio_buffer = BIO_new_mem_buf(buffer, buffer_length);
	if (bio_buffer != NULL) {
		stackOfParsedPemObjects = PEM_X509_INFO_read_bio(bio_buffer, NULL, NULL, NULL);
		BIO_free(bio_buffer);
	}
	if (stackOfParsedPemObjects != NULL) {
		ret = stack_x509_info_to_x509((STACK_OF(X509) *)cert, stackOfParsedPemObjects);
		sk_X509_INFO_pop_free(stackOfParsedPemObjects, X509_INFO_free);
	}
	return ret;
}

int32_t bctbx_x509_certificate_get_der_length(bctbx_x509_certificate_t *cert) {
	if (cert == NULL || sk_X509_num((STACK_OF(X509) *)cert) != 1) {
		return BCTBX_ERROR_INVALID_INPUT_DATA;
	}
	X509 *certificate = sk_X509_value((STACK_OF(X509) *)cert, 0);
	const int len = i2d_X509(certificate, NULL);
	return len > 0 ? len : 0;
}

int32_t bctbx_x509_certificate_get_der(bctbx_x509_certificate_t *cert, unsigned char *buffer, size_t buffer_length) {
	if (cert == NULL) {
		return BCTBX_ERROR_INVALID_CERTIFICATE;
	}
	if (sk_X509_num((STACK_OF(X509) *)cert) != 1) {
		return BCTBX_ERROR_INVALID_INPUT_DATA;
	}
	size_t der_length = bctbx_x509_certificate_get_der_length(cert);

	if (der_length + 1 > buffer_length) { /* check buffer size is ok, +1 for the NULL termination added at the end */
		return BCTBX_ERROR_OUTPUT_BUFFER_TOO_SMALL;
	}
	unsigned char *p = buffer;
	X509 *certificate = sk_X509_value((STACK_OF(X509) *)cert, 0);
	i2d_X509(certificate, &p);

	return 0;
}

int32_t bctbx_x509_certificate_get_subject_dn(const bctbx_x509_certificate_t *cert, char *dn, size_t dn_length) {
	int certStackSize = cert == NULL ? 0 : sk_X509_num((STACK_OF(X509) *)cert);
	if (certStackSize < 1) {
		return BCTBX_ERROR_INVALID_CERTIFICATE;
	}
	char name_buf[256];
	X509 *certificate = sk_X509_value((STACK_OF(X509) *)cert, 0);
	X509_NAME *subject_name = X509_get_subject_name(certificate);
	X509_NAME_oneline(subject_name, name_buf, 256);
	int offset = snprintf(dn, dn_length, "%s", name_buf);

	for (int i = 1; i < certStackSize; i++) {
		certificate = sk_X509_value((STACK_OF(X509) *)cert, i);
		if (certificate != NULL) {
			subject_name = X509_get_subject_name(certificate);
			X509_NAME_oneline(subject_name, name_buf, 256);
			offset += snprintf(dn + offset, dn_length - offset, "\n%s", name_buf);
		}
	}
	return strlen(dn);
}

bctbx_list_t *bctbx_x509_certificate_get_subjects(const bctbx_x509_certificate_t *cert) {
	bctbx_list_t *ret = NULL;
	int certStackSize = cert == NULL ? 0 : sk_X509_num((STACK_OF(X509) *)cert);
	for (int i = 0; i < certStackSize; i++) {
		X509 *certificate = sk_X509_value((STACK_OF(X509) *)cert, i);
		if (certificate != NULL) {
			X509_NAME *subject_name = X509_get_subject_name(certificate);
			const size_t subject_name_buf_len = 256;
			char *subject_name_buf = bctbx_malloc0(subject_name_buf_len);
			X509_NAME_get_text_by_NID(subject_name, NID_commonName, subject_name_buf, subject_name_buf_len);
			ret = bctbx_list_append(ret, subject_name_buf);

			GENERAL_NAMES *pSubAltNames = X509_get_ext_d2i(certificate, NID_subject_alt_name, NULL, NULL);
			int nSubAltNames = pSubAltNames == NULL ? 0 : sk_GENERAL_NAME_num(pSubAltNames);
			for (int j = 0; j < nSubAltNames; j++) {
				GENERAL_NAME *subAltName = sk_GENERAL_NAME_value(pSubAltNames, i);
				if (subAltName && subAltName->type == GEN_DNS) {
					BIO *buffer = BIO_new(BIO_s_mem());
					char *subject_alt_name_buf = bctbx_malloc0(subject_name_buf_len);
					ASN1_STRING_print(buffer, subAltName->d.ia5);
					BIO_read(buffer, subject_alt_name_buf, subject_name_buf_len);
					BIO_free(buffer);
					ret = bctbx_list_append(ret, subject_alt_name_buf);
				}
			}
			GENERAL_NAMES_free(pSubAltNames);
		}
	}
	return ret;
}

static void generate_x509(X509 *x509, EVP_PKEY *pkey, const char *subject) {
	if (x509 == NULL || pkey == NULL) {
		bctbx_error("Unable to create X509 structure.");
		return;
	}

	/* Set the serial number. */
	ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

	/* This certificate is valid from now until exactly one year from now. */
	X509_gmtime_adj(X509_get_notBefore(x509), 0);
	X509_gmtime_adj(X509_get_notAfter(x509), 31536000L);

	/* Set the public key for our certificate. */
	X509_set_pubkey(x509, pkey);

	/* We want to copy the subject name to the issuer name. */
	X509_NAME *name = X509_get_subject_name(x509);

	/* Set the country code and common name. */
	X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *)"FR", -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char *)"Belledonne", -1, -1, 0);
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *)subject, -1, -1, 0);

	/* Now set the issuer name. */
	X509_set_issuer_name(x509, name);

	/* Actually sign the certificate with our key. */
	if (0 == X509_sign(x509, pkey, EVP_sha1())) {
		bctbx_error("Error signing certificate.");
	}
}

int32_t bctbx_x509_certificate_generate_selfsigned(const char *subject,
                                                   bctbx_x509_certificate_t *certificate,
                                                   bctbx_signing_key_t *pkey,
                                                   char *pem,
                                                   size_t pem_length) {
	pkey->evp_pkey = EVP_RSA_gen(3072); // 'e' defaults to 65537
	if (pkey->evp_pkey == NULL) {
		bctbx_error("Couldn't generate an rsa key.");
		return BCTBX_ERROR_CERTIFICATE_GENERATION_FAIL;
	}
	X509 *cert = X509_new();
	generate_x509(cert, pkey->evp_pkey, subject);

	if (certificate != NULL) {
		sk_X509_push((STACK_OF(X509) *)certificate, cert);
	}

	char *self_signed_cert_pem = bctbx_x509_certificates_chain_get_pem((bctbx_x509_certificate_t *)certificate);
	char *private_key_pem = bctbx_signing_key_get_pem((bctbx_signing_key_t *)pkey);

	if (self_signed_cert_pem == NULL || private_key_pem == NULL) {
		bctbx_error("Couldn't write private key and/or certificate to pem format.");
		return BCTBX_ERROR_CERTIFICATE_GENERATION_FAIL;
	}

	size_t pem_minimum_length = strlen(self_signed_cert_pem) + strlen(private_key_pem) + 1;

	if (pem_length <= pem_minimum_length) {
		bctbx_error(
		    "Certificate generation can't copy the certificate to pem buffer: too short [%ld] but need [%ld] bytes",
		    (long)pem_length, (long)pem_minimum_length);
		return BCTBX_ERROR_OUTPUT_BUFFER_TOO_SMALL;
	}

	strncpy(pem, private_key_pem, pem_length);
	strncat(pem, self_signed_cert_pem, pem_length);

	return 0;
}

static enum bctbx_md_type nid_to_bctbx_md_type(int nid) {
	switch (nid) {
		case NID_sha1:
			return BCTBX_MD_SHA1;
		case NID_sha224:
			return BCTBX_MD_SHA224;
		case NID_sha256:
			return BCTBX_MD_SHA256;
		case NID_sha384:
			return BCTBX_MD_SHA384;
		case NID_sha512:
			return BCTBX_MD_SHA512;
		default:
			return BCTBX_MD_UNDEFINED;
	}
}

static int bctbx_md_type_to_nid(enum bctbx_md_type md) {
	switch (md) {
		case BCTBX_MD_SHA1:
			return NID_sha1;
		case BCTBX_MD_SHA224:
			return NID_sha224;
		case BCTBX_MD_SHA256:
			return NID_sha256;
		case BCTBX_MD_SHA384:
			return NID_sha384;
		case BCTBX_MD_SHA512:
			return NID_sha512;
		default:
			return 0;
	}
}

static const char *nid_to_string(int nid) {
	switch (nid) {
		case NID_sha1:
			return "SHA-1";
		case NID_sha224:
			return "SHA-224";
		case NID_sha256:
			return "SHA-256";
		case NID_sha384:
			return "SHA-384";
		case NID_sha512:
			return "SHA-512";
		default:
			return "UNDEFINED";
	}
}

int32_t bctbx_x509_certificate_get_signature_hash_function(const bctbx_x509_certificate_t *certificate,
                                                           bctbx_md_type_t *hash_algorithm) {
	if (certificate == NULL || sk_X509_num((const STACK_OF(X509) *)certificate) != 1) {
		return BCTBX_ERROR_INVALID_CERTIFICATE;
	}

	int mdnid;
	X509 *cert = sk_X509_value((const STACK_OF(X509) *)certificate, 0);
	X509_get_signature_info(cert, &mdnid, NULL, NULL, NULL);
	*hash_algorithm = nid_to_bctbx_md_type(mdnid);

	if (*hash_algorithm == BCTBX_MD_UNDEFINED) {
		return BCTBX_ERROR_UNSUPPORTED_HASH_FUNCTION;
	}
	return 0;
}

/* maximum length of returned buffer will be 7(SHA-512 string)+3*hash_length(64)+null char = 200 bytes */
int32_t bctbx_x509_certificate_get_fingerprint(const bctbx_x509_certificate_t *certificate,
                                               char *fingerprint,
                                               size_t fingerprint_length,
                                               bctbx_md_type_t hash_algorithm) {
	unsigned char buffer[EVP_MAX_MD_SIZE] = {0};
	unsigned int md_written = 0;

	if (certificate == NULL || sk_X509_num((const STACK_OF(X509) *)certificate) != 1) {
		return BCTBX_ERROR_INVALID_CERTIFICATE;
	}

	/* if there is a specified hash algorithm, use it*/
	int nid = bctbx_md_type_to_nid(hash_algorithm);
	const X509 *cert = sk_X509_value((const STACK_OF(X509) *)certificate, 0);
	if (nid == 0) {
		if (!X509_get_signature_info((X509 *)cert, &nid, NULL, NULL, NULL)) {
			return BCTBX_ERROR_UNSUPPORTED_HASH_FUNCTION;
		}
	}

	if (!X509_digest(cert, EVP_get_digestbynid(nid), buffer, &md_written)) {
		return BCTBX_ERROR_UNSUPPORTED_HASH_FUNCTION;
	}

	const char *md_string = nid_to_string(nid);
	/* Fingerprint example: 'SHA-512 01:02:03:FF\0' */
	size_t fingerprint_size = md_written > 0 ? strlen(md_string) + 3 * md_written + 1 : 0;
	if (fingerprint_length < fingerprint_size || fingerprint == NULL) {
		return BCTBX_ERROR_OUTPUT_BUFFER_TOO_SMALL;
	}

	if (md_written > 0) {
		size_t offset = snprintf(fingerprint, fingerprint_length, "%s", md_string);
		offset += snprintf(fingerprint + offset, fingerprint_length - offset, "%c%02X", ' ', buffer[0]);
		for (size_t i = 1; i < md_written; i++) {
			offset += snprintf(fingerprint + offset, fingerprint_length - offset, "%c%02X", ':', buffer[i]);
		}
	}

	return (int32_t)fingerprint_size;
}

int32_t bctbx_x509_certificate_flags_to_string(BCTBX_UNUSED(char *buffer),
                                               BCTBX_UNUSED(size_t buffer_size),
                                               BCTBX_UNUSED(uint32_t flags)) {
	return 0;
	/*@TODO: This is use, but how should it be supported?*/
}

/*@TODO: This isn't used anywhere, report to belledonne?*/
int32_t bctbx_x509_certificate_set_flag(BCTBX_UNUSED(uint32_t *flags), BCTBX_UNUSED(uint32_t flags_to_set)) {
	return 0;
}

/*@TODO: This isn't used anywhere, report to belledonne?*/
uint32_t bctbx_x509_certificate_remap_flag(BCTBX_UNUSED(uint32_t flags)) {
	uint32_t ret = 0;
	return ret;
}

/*@TODO: This is use, but it is something we want to support?*/
int32_t bctbx_x509_certificate_unset_flag(BCTBX_UNUSED(uint32_t *flags), BCTBX_UNUSED(uint32_t flags_to_unset)) {
	return 0;
}

/*** Diffie-Hellman-Merkle ***/
static EVP_PKEY *create_dh(char *safe_prime, int priv_len) {
	OSSL_PARAM params[3];
	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_from_name(NULL, "DH", NULL);

	params[0] = OSSL_PARAM_construct_utf8_string("group", safe_prime, 0);
	params[1] = OSSL_PARAM_construct_int("priv_len", &priv_len);
	params[2] = OSSL_PARAM_construct_end();

	EVP_PKEY_keygen_init(pctx);
	EVP_PKEY_CTX_set_params(pctx, params);
	EVP_PKEY_generate(pctx, &pkey);

	EVP_PKEY_CTX_free(pctx);
	return pkey;
}

/* initialise de DHM context according to requested algorithm */
bctbx_DHMContext_t *bctbx_CreateDHMContext(uint8_t DHMAlgo, uint8_t secretLength) {
	bctbx_DHMContext_t *dh_ctx = bctbx_malloc0(sizeof(bctbx_DHMContext_t));

	if (dh_ctx == NULL) {
		return NULL;
	}

	dh_ctx->algo = DHMAlgo;
	dh_ctx->secretLength = secretLength;

	switch (DHMAlgo) {
		case BCTBX_DHM_2048:
			dh_ctx->primeLength = 256;
			dh_ctx->cryptoModuleData = create_dh("modp_2048", dh_ctx->primeLength);
			break;
		case BCTBX_DHM_3072:
			dh_ctx->primeLength = 384;
			dh_ctx->cryptoModuleData = create_dh("modp_3072", dh_ctx->primeLength);
			break;
		default:
			bctbx_error("Cannot create Diffie-Hellman context from %d", DHMAlgo);
			bctbx_DestroyDHMContext(dh_ctx);
			return NULL;
	}
	return dh_ctx;
}

/* generate the random secret and compute the public value */
void bctbx_DHMCreatePublic(bctbx_DHMContext_t *context,
                           BCTBX_UNUSED(int (*rngFunction)(void *, uint8_t *, size_t)),
                           BCTBX_UNUSED(void *rngContext)) {
	EVP_PKEY *dh = context->cryptoModuleData;
	// Generate the private/public keys for server and client
	if (!dh) {
		bctbx_error("Key generation failed");
		return;
	}

	if (context->self) {
		OPENSSL_free(context->self);
	}
	context->primeLength = EVP_PKEY_get1_encoded_public_key(dh, &context->self);

	if (context->primeLength == 0) {
		bctbx_error("EVP_PKEY_get1_encoded_public_key() failed");
	}
}

/* compute secret - the ->peer field of context must have been set before calling this function */
void bctbx_DHMComputeSecret(bctbx_DHMContext_t *context,
                            BCTBX_UNUSED(int (*rngFunction)(void *, uint8_t *, size_t)),
                            BCTBX_UNUSED(void *rngContext)) {
	EVP_PKEY *peerkey = EVP_PKEY_new();

	if (peerkey == NULL || EVP_PKEY_copy_parameters(peerkey, context->cryptoModuleData) <= 0) {
		return;
	}

	if (EVP_PKEY_set1_encoded_public_key(peerkey, context->peer, context->primeLength) <= 0) {
		return;
	}

	EVP_PKEY_CTX *kex_ctx = EVP_PKEY_CTX_new(context->cryptoModuleData, NULL);
	if (!kex_ctx) {
		bctbx_error("EVP_PKEY_CTX_dup() failed");
	}
	size_t skeylen;
	if (EVP_PKEY_derive_init(kex_ctx) <= 0 || EVP_PKEY_derive_set_peer(kex_ctx, peerkey) <= 0 ||
	    EVP_PKEY_derive(kex_ctx, NULL, &skeylen) <= 0) {
		bctbx_error("EVP_PKEY_derive_init() or EVP_PKEY_derive_set_peer() or EVP_PKEY_derive() failed");
		EVP_PKEY_free(peerkey);
		EVP_PKEY_CTX_free(kex_ctx);
		return;
	}

	if (context->key != NULL) {
		bctbx_clean(context->key, context->primeLength);
		bctbx_free(context->key);
	}
	context->key = OPENSSL_malloc(skeylen);

	if (!context->key || EVP_PKEY_derive(kex_ctx, context->key, &skeylen) <= 0 || skeylen != context->primeLength) {
		bctbx_error("OPENSSL_malloc() or EVP_PKEY_derive() failed or skeylen != context->primeLength");
	}

	EVP_PKEY_free(peerkey);
	EVP_PKEY_CTX_free(kex_ctx);
}

/* clean DHM context */
void bctbx_DestroyDHMContext(bctbx_DHMContext_t *context) {
	if (context->secret != NULL) {
		bctbx_clean(context->secret, context->secretLength);
		bctbx_free(context->secret);
	}
	if (context->key != NULL) {
		bctbx_clean(context->key, context->primeLength);
		bctbx_free(context->key);
	}
	if (context->self != NULL) {
		OPENSSL_free(context->self);
	}
	if (context->peer != NULL) {
		bctbx_free(context->peer);
	}
	if (context->cryptoModuleData != NULL) {
		EVP_PKEY_free(context->cryptoModuleData);
	}

	bctbx_free(context);
}

/*** SSL Client ***/

static int bctbx_ssl_sendrecv_bctbx_err_to_ssl_err(int32_t ret_code) {
	switch (ret_code) {
		case BCTBX_ERROR_NET_WANT_READ:
			return SSL_ERROR_WANT_READ;
		case BCTBX_ERROR_NET_WANT_WRITE:
			return SSL_ERROR_WANT_WRITE;
		case BCTBX_ERROR_SSL_PEER_CLOSE_NOTIFY:
			return SSL_ERROR_ZERO_RETURN;
		case BCTBX_ERROR_NET_CONN_RESET:
		default:
			return (int)ret_code;
	}
}

/** context **/
struct bctbx_ssl_context_struct {
	SSL *ssl;

	int (*callback_send_function)(
	    void *,
	    const unsigned char *,
	    size_t); /* callbacks args are: callback data, data buffer to be send, size of data buffer */
	int (*callback_recv_function)(void *,
	                              unsigned char *,
	                              size_t); /* args: callback data, data buffer to be read, size of data buffer */
	void *callback_sendrecv_data;          /**< data passed to send/recv callbacks */

	STACK_OF(X509 *) peer_cert;
};

bctbx_ssl_context_t *bctbx_ssl_context_new(void) {
	bctbx_ssl_context_t *ssl_ctx = bctbx_malloc0(sizeof(bctbx_ssl_context_t));
	ssl_ctx->ssl = NULL;
	ssl_ctx->callback_send_function = NULL;
	ssl_ctx->callback_recv_function = NULL;
	ssl_ctx->callback_sendrecv_data = NULL;
	ssl_ctx->peer_cert = NULL;
	return ssl_ctx;
}

void bctbx_ssl_context_free(bctbx_ssl_context_t *ssl_ctx) {
	if (ssl_ctx) {
		if (ssl_ctx->ssl) {
			void *client_cert_cb_ctx = SSL_get_ex_data(ssl_ctx->ssl, OPENSSL_CLIENT_CERT_CALLBACK_EX_DATA_IDX);
			if (client_cert_cb_ctx != NULL) bctbx_free(client_cert_cb_ctx);
			SSL_free(ssl_ctx->ssl);
		}
		if (ssl_ctx->peer_cert != NULL) {
			sk_X509_pop_free(ssl_ctx->peer_cert, X509_free);
		}
		bctbx_free(ssl_ctx);
	}
}

int32_t bctbx_ssl_close_notify(bctbx_ssl_context_t *ssl_ctx) {
	return SSL_shutdown(ssl_ctx->ssl);
}

int32_t bctbx_ssl_session_reset(bctbx_ssl_context_t *ssl_ctx) {
	SSL_CTX *ctx = SSL_get_SSL_CTX(ssl_ctx->ssl);
	SSL_free(ssl_ctx->ssl);
	ssl_ctx->ssl = SSL_new(ctx);
	return ssl_ctx->ssl == NULL ? BCTBX_ERROR_INVALID_SSL_CONTEXT : 0;
}

#define CASE_OF_RET(x)                                                                                                 \
	case x:                                                                                                            \
		return #x;                                                                                                     \
		break

const char *ssl_error_to_string(int error) {
	switch (error) {
		CASE_OF_RET(SSL_ERROR_NONE);
		CASE_OF_RET(SSL_ERROR_SSL);
		CASE_OF_RET(SSL_ERROR_WANT_READ);
		CASE_OF_RET(SSL_ERROR_WANT_WRITE);
		CASE_OF_RET(SSL_ERROR_WANT_X509_LOOKUP);
		CASE_OF_RET(SSL_ERROR_SYSCALL);
		CASE_OF_RET(SSL_ERROR_ZERO_RETURN);
		CASE_OF_RET(SSL_ERROR_WANT_CONNECT);
		CASE_OF_RET(SSL_ERROR_WANT_ACCEPT);
		CASE_OF_RET(SSL_ERROR_WANT_ASYNC);
		CASE_OF_RET(SSL_ERROR_WANT_ASYNC_JOB);
		CASE_OF_RET(SSL_ERROR_WANT_CLIENT_HELLO_CB);
		default:
			return "";
	}
}

const char *bctbx_ssl_error_to_string(int error) {
	switch (error) {
		CASE_OF_RET(BCTBX_ERROR_NET_WANT_READ);
		CASE_OF_RET(BCTBX_ERROR_NET_WANT_WRITE);
		CASE_OF_RET(BCTBX_ERROR_NET_CONN_RESET);
		CASE_OF_RET(BCTBX_ERROR_SSL_PEER_CLOSE_NOTIFY);
		default:
			return "";
	}
}

int32_t bctbx_ssl_write(bctbx_ssl_context_t *ssl_ctx, const unsigned char *buf, size_t buf_length) {
	int ret = SSL_write(ssl_ctx->ssl, buf, buf_length);
	int err = SSL_get_error(ssl_ctx->ssl, bctbx_ssl_sendrecv_bctbx_err_to_ssl_err(ret));
	if (ret < 0) {
		bctbx_warning("bctbx_ssl_write(%p, %p, %zu), ret: '%s', err:'%s'", ssl_ctx, buf, buf_length,
		              bctbx_ssl_error_to_string(ret), ssl_error_to_string(err));
	}
	return ret;
}

int32_t bctbx_ssl_read(bctbx_ssl_context_t *ssl_ctx, unsigned char *buf, size_t buf_length) {
	int ret = SSL_read(ssl_ctx->ssl, buf, buf_length);
	int err = SSL_get_error(ssl_ctx->ssl, bctbx_ssl_sendrecv_bctbx_err_to_ssl_err(ret));
	if (ret < 0) {
		bctbx_warning("bctbx_ssl_read(%p, %p, %zu), ret: '%s', err:'%s'", ssl_ctx, buf, buf_length,
		              bctbx_ssl_error_to_string(ret), ssl_error_to_string(err));
	}
	return ret;
}

const char *ssl_state_to_string(OSSL_HANDSHAKE_STATE state) {
	switch (state) {
		CASE_OF_RET(TLS_ST_BEFORE);
		CASE_OF_RET(TLS_ST_OK);
		CASE_OF_RET(DTLS_ST_CR_HELLO_VERIFY_REQUEST);
		CASE_OF_RET(TLS_ST_CR_SRVR_HELLO);
		CASE_OF_RET(TLS_ST_CR_CERT);
		CASE_OF_RET(TLS_ST_CR_CERT_STATUS);
		CASE_OF_RET(TLS_ST_CR_KEY_EXCH);
		CASE_OF_RET(TLS_ST_CR_CERT_REQ);
		CASE_OF_RET(TLS_ST_CR_SRVR_DONE);
		CASE_OF_RET(TLS_ST_CR_SESSION_TICKET);
		CASE_OF_RET(TLS_ST_CR_CHANGE);
		CASE_OF_RET(TLS_ST_CR_FINISHED);
		CASE_OF_RET(TLS_ST_CW_CLNT_HELLO);
		CASE_OF_RET(TLS_ST_CW_CERT);
		CASE_OF_RET(TLS_ST_CW_KEY_EXCH);
		CASE_OF_RET(TLS_ST_CW_CERT_VRFY);
		CASE_OF_RET(TLS_ST_CW_CHANGE);
		CASE_OF_RET(TLS_ST_CW_NEXT_PROTO);
		CASE_OF_RET(TLS_ST_CW_FINISHED);
		CASE_OF_RET(TLS_ST_SW_HELLO_REQ);
		CASE_OF_RET(TLS_ST_SR_CLNT_HELLO);
		CASE_OF_RET(DTLS_ST_SW_HELLO_VERIFY_REQUEST);
		CASE_OF_RET(TLS_ST_SW_SRVR_HELLO);
		CASE_OF_RET(TLS_ST_SW_CERT);
		CASE_OF_RET(TLS_ST_SW_KEY_EXCH);
		CASE_OF_RET(TLS_ST_SW_CERT_REQ);
		CASE_OF_RET(TLS_ST_SW_SRVR_DONE);
		CASE_OF_RET(TLS_ST_SR_CERT);
		CASE_OF_RET(TLS_ST_SR_KEY_EXCH);
		CASE_OF_RET(TLS_ST_SR_CERT_VRFY);
		CASE_OF_RET(TLS_ST_SR_NEXT_PROTO);
		CASE_OF_RET(TLS_ST_SR_CHANGE);
		CASE_OF_RET(TLS_ST_SR_FINISHED);
		CASE_OF_RET(TLS_ST_SW_SESSION_TICKET);
		CASE_OF_RET(TLS_ST_SW_CERT_STATUS);
		CASE_OF_RET(TLS_ST_SW_CHANGE);
		CASE_OF_RET(TLS_ST_SW_FINISHED);
		CASE_OF_RET(TLS_ST_SW_ENCRYPTED_EXTENSIONS);
		CASE_OF_RET(TLS_ST_CR_ENCRYPTED_EXTENSIONS);
		CASE_OF_RET(TLS_ST_CR_CERT_VRFY);
		CASE_OF_RET(TLS_ST_SW_CERT_VRFY);
		CASE_OF_RET(TLS_ST_CR_HELLO_REQ);
		CASE_OF_RET(TLS_ST_SW_KEY_UPDATE);
		CASE_OF_RET(TLS_ST_CW_KEY_UPDATE);
		CASE_OF_RET(TLS_ST_SR_KEY_UPDATE);
		CASE_OF_RET(TLS_ST_CR_KEY_UPDATE);
		CASE_OF_RET(TLS_ST_EARLY_DATA);
		CASE_OF_RET(TLS_ST_PENDING_EARLY_DATA_END);
		CASE_OF_RET(TLS_ST_CW_END_OF_EARLY_DATA);
		CASE_OF_RET(TLS_ST_SR_END_OF_EARLY_DATA);
#if OPENSSL_VERSION_NUMBER >= 0x30200000L
		CASE_OF_RET(TLS_ST_CR_COMP_CERT);
		CASE_OF_RET(TLS_ST_CW_COMP_CERT);
		CASE_OF_RET(TLS_ST_SR_COMP_CERT);
		CASE_OF_RET(TLS_ST_SW_COMP_CERT);
#endif
	}
	return "";
}

int32_t bctbx_ssl_handshake(bctbx_ssl_context_t *ssl_ctx) {
	int ret = SSL_is_init_finished(ssl_ctx->ssl);
	int err = SSL_ERROR_NONE;
	if (ret != 1) {
		ret = SSL_do_handshake(ssl_ctx->ssl);
		err = SSL_get_error(ssl_ctx->ssl, ret);
	}
	bctbx_message("bctbx_ssl_handshake(): state='%s', err='%s'", ssl_state_to_string(SSL_get_state(ssl_ctx->ssl)),
	              err == SSL_ERROR_SSL ? ERR_error_string(ERR_get_error(), NULL) : "");
	switch (err) {
		case SSL_ERROR_NONE:
			return 0;
		case SSL_ERROR_WANT_READ:
			return BCTBX_ERROR_NET_WANT_READ;
		case SSL_ERROR_WANT_WRITE:
			return BCTBX_ERROR_NET_WANT_WRITE;
		case SSL_ERROR_SYSCALL:
		case SSL_ERROR_SSL:
		default:
			return BCTBX_ERROR_NET_CONN_RESET;
	}
}

int32_t
bctbx_ssl_set_hs_own_cert(bctbx_ssl_context_t *ssl_ctx, bctbx_x509_certificate_t *cert, bctbx_signing_key_t *key) {
	if (cert == NULL || sk_X509_num((STACK_OF(X509) *)cert) != 1) {
		return BCTBX_ERROR_INVALID_CERTIFICATE;
	}
	X509 *certificate = sk_X509_value((STACK_OF(X509) *)cert, 0);
	return SSL_use_certificate(ssl_ctx->ssl, certificate) && SSL_use_PrivateKey(ssl_ctx->ssl, key->evp_pkey)
	           ? 0
	           : ERR_get_error();
}

static int bctbx_ssl_send_callback(BIO *bio, const char *buffer, int buffer_length) {
	/* data is the ssl_context which contains the actual callback and data */
	bctbx_ssl_context_t *ssl_ctx = (bctbx_ssl_context_t *)BIO_get_data(bio);

	int ret =
	    ssl_ctx->callback_send_function(ssl_ctx->callback_sendrecv_data, (const unsigned char *)buffer, buffer_length);

	switch (ret) {
		case BCTBX_ERROR_NET_WANT_READ:
			BIO_set_retry_read(bio);
			break;
		case BCTBX_ERROR_NET_WANT_WRITE:
			BIO_set_retry_write(bio);
			break;
		default:
			BIO_clear_retry_flags(bio);
	}

	return ret;
}

static int bctbx_ssl_recv_callback(BIO *bio, char *buffer, int buffer_length) {
	/* data is the ssl_context which contains the actual callback and data */
	bctbx_ssl_context_t *ssl_ctx = (bctbx_ssl_context_t *)BIO_get_data(bio);

	int ret = ssl_ctx->callback_recv_function(ssl_ctx->callback_sendrecv_data, (unsigned char *)buffer, buffer_length);

	switch (ret) {
		case BCTBX_ERROR_NET_WANT_READ:
			BIO_set_retry_read(bio);
			break;
		case BCTBX_ERROR_NET_WANT_WRITE:
			BIO_set_retry_write(bio);
			break;
		default:
			BIO_clear_retry_flags(bio);
	}

	return ret;
}

static int create_bio(BIO *bio) {
	BIO_set_init(bio, 1);
	return 1;
}

static int destroy_bio(BCTBX_UNUSED(BIO *bio)) {
	return 1;
}

static long bio_ctrl(BCTBX_UNUSED(BIO *bio), int cmd, BCTBX_UNUSED(long larg), BCTBX_UNUSED(void *parg)) {
	return cmd == BIO_CTRL_FLUSH;
}

static BIO_METHOD *create_bio_meth(void) {
	BIO_METHOD *m = BIO_meth_new(BIO_get_new_index(), "bctoolbox_bio_meth");
	BIO_meth_set_write(m, &bctbx_ssl_send_callback);
	BIO_meth_set_read(m, &bctbx_ssl_recv_callback);
	BIO_meth_set_create(m, &create_bio);
	BIO_meth_set_destroy(m, &destroy_bio);
	BIO_meth_set_ctrl(m, &bio_ctrl);
	return m;
}

static BIO_METHOD *get_bio_meth(void) {
	static BIO_METHOD *m = NULL;
	if (m == NULL) m = create_bio_meth();
	return m;
}

void bctbx_ssl_set_io_callbacks(
    bctbx_ssl_context_t *ssl_ctx,
    void *callback_data,
    int (*callback_send_function)(void *, const unsigned char *, size_t), /* callbacks args are: callback data, data
                                                                             buffer to be send, size of data buffer */
    int (*callback_recv_function)(void *,
                                  unsigned char *,
                                  size_t)) { /* args: callback data, data buffer to be read, size of data buffer */

	if (ssl_ctx == NULL) {
		return;
	}

	ssl_ctx->callback_send_function = callback_send_function;
	ssl_ctx->callback_recv_function = callback_recv_function;
	ssl_ctx->callback_sendrecv_data = callback_data;

	BIO *bio_read = BIO_new(get_bio_meth());
	BIO *bio_write = BIO_new(get_bio_meth());

	BIO_set_data(bio_read, ssl_ctx);
	BIO_set_data(bio_write, ssl_ctx);

	SSL_set0_rbio(ssl_ctx->ssl, bio_read);
	SSL_set0_wbio(ssl_ctx->ssl, bio_write);
}

const bctbx_x509_certificate_t *bctbx_ssl_get_peer_certificate(bctbx_ssl_context_t *ssl_ctx) {
	if (ssl_ctx->peer_cert == NULL) {
		X509 *peer_cert = SSL_get_peer_certificate(ssl_ctx->ssl);
		ssl_ctx->peer_cert = sk_X509_new_null();
		if (peer_cert == NULL) {
			bctbx_error("Could not read peer certificate.");
		} else {
			sk_X509_push(ssl_ctx->peer_cert, peer_cert);
		}
	}
	return (bctbx_x509_certificate_t *)ssl_ctx->peer_cert;
}

const char *bctbx_ssl_get_ciphersuite(bctbx_ssl_context_t *ssl_ctx) {
	const SSL_CIPHER *cipher = SSL_SESSION_get0_cipher(SSL_get0_session(ssl_ctx->ssl));
	return SSL_CIPHER_get_name(cipher);
}

int bctbx_ssl_get_ciphersuite_id(const char *ciphersuite) {
	const SSL_CIPHER *cipher = SSL_CIPHER_find(NULL, (const unsigned char *)ciphersuite);
	return cipher == NULL ? 0 : SSL_CIPHER_get_id(cipher);
}

const char *bctbx_ssl_get_version(bctbx_ssl_context_t *ssl_ctx) {
	const SSL_SESSION *session = SSL_get0_session(ssl_ctx->ssl);
	if (session == NULL) return "No session";
	switch (SSL_SESSION_get_protocol_version(session)) {
		/* Matches mbedtls return naming */
		case TLS1_VERSION:
			return "TLSv1.0";
		case TLS1_1_VERSION:
			return "TLSv1.1";
		case TLS1_2_VERSION:
			return "TLSv1.2";
		case TLS1_3_VERSION:
			return "TLSv1.3";
		case DTLS1_VERSION:
			return "DTLSv1.0";
		case DTLS1_2_VERSION:
			return "DTLSv1.2";
		default:
			return "Unknown";
	}
}

int32_t bctbx_ssl_set_hostname(bctbx_ssl_context_t *ssl_ctx, const char *hostname) {
	return SSL_set_tlsext_host_name(ssl_ctx->ssl, hostname) == 1 ? 0 : ERR_get_error();
}

/** DTLS SRTP functions **/
uint8_t bctbx_dtls_srtp_supported(void) {
	return 1;
}

void bctbx_ssl_set_mtu(bctbx_ssl_context_t *ssl_ctx, uint16_t mtu) {
	SSL_set_options(ssl_ctx->ssl, SSL_OP_NO_QUERY_MTU);
	int ret = DTLS_set_link_mtu(ssl_ctx->ssl, mtu);
	// ret = SSL_set_mtu(ssl_ctx->ssl, mtu);
	(void)ret;
}

static bctbx_dtls_srtp_profile_t bctbx_srtp_profile_openssl2bctoolbox(unsigned long profile) {
	switch (profile) {
		case SRTP_AES128_CM_SHA1_80:
			return BCTBX_SRTP_AES128_CM_HMAC_SHA1_80;
		case SRTP_AES128_CM_SHA1_32:
			return BCTBX_SRTP_AES128_CM_HMAC_SHA1_32;
		case SRTP_NULL_SHA1_80:
			return BCTBX_SRTP_NULL_HMAC_SHA1_80;
		case SRTP_NULL_SHA1_32:
			return BCTBX_SRTP_NULL_HMAC_SHA1_32;
		default:
			return BCTBX_SRTP_UNDEFINED;
	}
}

static const char *bctbx_srtp_profile_bctoolbox2openssl(bctbx_dtls_srtp_profile_t bctbx_profile) {
	switch (bctbx_profile) {
		case BCTBX_SRTP_AES128_CM_HMAC_SHA1_80:
			return "SRTP_AES128_CM_SHA1_80";
		case BCTBX_SRTP_AES128_CM_HMAC_SHA1_32:
			return "SRTP_AES128_CM_SHA1_32";
		case BCTBX_SRTP_NULL_HMAC_SHA1_80:
			return "SRTP_NULL_SHA1_80";
		case BCTBX_SRTP_NULL_HMAC_SHA1_32:
			return "SRTP_NULL_SHA1_32";
		default:
			return "";
	}
}

bctbx_dtls_srtp_profile_t bctbx_ssl_get_dtls_srtp_protection_profile(bctbx_ssl_context_t *ssl_ctx) {
	if (ssl_ctx == NULL) {
		return BCTBX_SRTP_UNDEFINED;
	}
	SRTP_PROTECTION_PROFILE *profile = SSL_get_selected_srtp_profile(ssl_ctx->ssl);
	if (profile == NULL) {
		return BCTBX_SRTP_UNDEFINED;
	}
	return bctbx_srtp_profile_openssl2bctoolbox(profile->id);
};

/** DTLS SRTP functions **/

/** config **/
struct bctbx_ssl_config_struct {
	const SSL_METHOD *ssl_method; /**< current tls method used by ssl_ctx (cannot retrieve it from the SSL_CTX object
	                                 before OpenSSL 3.0 so have to store it here..)*/
	SSL_CTX *ssl_ctx;             /**< actual config structure */
	int ssl_verification_mode;    /**< BCTBX_SSL_VERIFY_NONE, BCTBX_SSL_VERIFY_OPTIONAL, BCTBX_SSL_VERIFY_REQUIRED */
	uint8_t ssl_config_externally_provided; /**< a flag, on when the ssl_config was provided by callers and not created
	                                           through bctbx_ssl_config_new() function */
};

bctbx_ssl_config_t *bctbx_ssl_config_new(void) {
	bctbx_ssl_config_t *ssl_config = bctbx_malloc0(sizeof(bctbx_ssl_config_t));
	ssl_config->ssl_method = DTLS_method();
	ssl_config->ssl_ctx = SSL_CTX_new(ssl_config->ssl_method);
	ssl_config->ssl_config_externally_provided = 0;
	bctbx_ssl_config_set_authmode(ssl_config, BCTBX_SSL_VERIFY_REQUIRED);
	return ssl_config;
}

bctbx_type_implementation_t bctbx_ssl_get_implementation_type(void) {
	return BCTBX_OPENSSL;
}

int32_t bctbx_ssl_config_set_crypto_library_config(bctbx_ssl_config_t *ssl_config, void *internal_config) {
	if (ssl_config == NULL) {
		return BCTBX_ERROR_INVALID_SSL_CONFIG;
	}

	/* free already existing structure */
	if (ssl_config->ssl_ctx != NULL && ssl_config->ssl_config_externally_provided == 0) {
		SSL_CTX_free(ssl_config->ssl_ctx);
	}

	/* set the given pointer as the ssl_config context */
	ssl_config->ssl_ctx = (SSL_CTX *)internal_config;

	/* set the flag in order to not free the openssl config when freeing bctbx_ssl_config */
	ssl_config->ssl_config_externally_provided = 1;

	return 0;
}

void bctbx_ssl_config_free(bctbx_ssl_config_t *ssl_config) {
	if (ssl_config == NULL) {
		return;
	}

	/* free mbedtls_ssl_config only when created internally */
	if (ssl_config->ssl_config_externally_provided == 0) {
		SSL_CTX_free(ssl_config->ssl_ctx);
	}

	bctbx_free(ssl_config);
}

const char *endpointToString(int endpoint) {
	switch (endpoint) {
		CASE_OF_RET(BCTBX_SSL_IS_CLIENT);
		CASE_OF_RET(BCTBX_SSL_IS_SERVER);
		default:
			return "";
	}
}

const char *transportToString(int endpoint) {
	switch (endpoint) {
		CASE_OF_RET(BCTBX_SSL_TRANSPORT_STREAM);
		CASE_OF_RET(BCTBX_SSL_TRANSPORT_DATAGRAM);
		default:
			return "";
	}
}

static int bctbx_SSL_CTX_set_ssl_version(bctbx_ssl_config_t *ssl_config, int endpoint, int transport) {
	if (endpoint == BCTBX_SSL_IS_CLIENT && transport == BCTBX_SSL_TRANSPORT_STREAM) {
		ssl_config->ssl_method = TLS_client_method();
	} else if (endpoint == BCTBX_SSL_IS_SERVER && transport == BCTBX_SSL_TRANSPORT_STREAM) {
		ssl_config->ssl_method = TLS_server_method();
	} else if (endpoint == BCTBX_SSL_IS_CLIENT && transport == BCTBX_SSL_TRANSPORT_DATAGRAM) {
		ssl_config->ssl_method = DTLS_client_method();
	} else if (endpoint == BCTBX_SSL_IS_SERVER && transport == BCTBX_SSL_TRANSPORT_DATAGRAM) {
		ssl_config->ssl_method = DTLS_server_method();
	} else {
		return BCTBX_ERROR_INVALID_INPUT_DATA;
	}
	return SSL_CTX_set_ssl_version(ssl_config->ssl_ctx, ssl_config->ssl_method);
}

int32_t bctbx_ssl_config_defaults(bctbx_ssl_config_t *ssl_config, int endpoint, int transport) {
	if (ssl_config == NULL) {
		return BCTBX_ERROR_INVALID_SSL_CONFIG;
	}

	int ret = bctbx_SSL_CTX_set_ssl_version(ssl_config, endpoint, transport);

	// Note: no openssl equivalent to mbedtls_ssl_conf_handshake_timeout()

	// Note: no openssl equivalent to mbedtls_ssl_conf_cert_profile();

	return ret == 1 ? 0 : BCTBX_ERROR_INVALID_SSL_CONFIG;
}

int32_t bctbx_ssl_config_set_endpoint(bctbx_ssl_config_t *ssl_config, int endpoint) {
	if (ssl_config == NULL) {
		return BCTBX_ERROR_INVALID_SSL_CONFIG;
	}

	const int transport =
	    ssl_config->ssl_method == DTLS_server_method() || ssl_config->ssl_method == DTLS_client_method()
	        ? BCTBX_SSL_TRANSPORT_DATAGRAM
	        : BCTBX_SSL_TRANSPORT_STREAM;

	return bctbx_SSL_CTX_set_ssl_version(ssl_config, endpoint, transport);
}

int32_t bctbx_ssl_config_set_transport(bctbx_ssl_config_t *ssl_config, int transport) {
	if (ssl_config == NULL) {
		return BCTBX_ERROR_INVALID_SSL_CONFIG;
	}

	const int endpoint = ssl_config->ssl_method == TLS_client_method() || ssl_config->ssl_method == DTLS_client_method()
	                         ? BCTBX_SSL_IS_CLIENT
	                         : BCTBX_SSL_IS_SERVER;

	return bctbx_SSL_CTX_set_ssl_version(ssl_config, endpoint, transport);
}

/* returns an allocated string that must be freed by caller. */
static char *join(const bctbx_list_t *strings, const char *separator) {
	const bctbx_list_t *string_element = NULL;

	size_t output_string_length = 1;
	for (string_element = strings; string_element != NULL; string_element = bctbx_list_next(string_element)) {
		char *string_data = bctbx_list_get_data(string_element);
		if (string_data != NULL) {
			output_string_length += strlen(string_data) + 1;
		}
	}

	char *output_string = bctbx_malloc0(output_string_length);
	size_t offset = 0;
	for (string_element = strings; string_element != NULL; string_element = bctbx_list_next(string_element)) {
		const char *string_data = bctbx_list_get_data(string_element);
		if (string_data != NULL && strlen(string_data) > 0) {
			offset += snprintf(output_string + offset, output_string_length - offset, "%s%s",
			                   offset > 0 ? separator : "", string_data);
		}
	}

	return output_string;
}

int32_t bctbx_ssl_config_set_ciphersuites(bctbx_ssl_config_t *ssl_config, const bctbx_list_t *ciphersuites) {
	if (ssl_config == NULL) {
		return BCTBX_ERROR_INVALID_SSL_CONFIG;
	}
	/* remap input arguments */
	if (ciphersuites == NULL) {
		return BCTBX_ERROR_INVALID_INPUT_DATA;
	}

	char *ciphername_string = join(ciphersuites, ":");

	// SSL_CTX_set_cipher_list() only configures TLS v1.2 so this api is consistent with the mbedtls backend
	// implementation We should consider an additional api for setting TLS 1.3 ciphers
	SSL_CTX_set_cipher_list(ssl_config->ssl_ctx, ciphername_string);

	bctbx_free(ciphername_string);
	return 0;
}

void *bctbx_ssl_config_get_private_config(bctbx_ssl_config_t *ssl_config) {
	return (void *)ssl_config->ssl_ctx;
}

int getSslVerifyMode(int authmode) {
	switch (authmode) {
		case BCTBX_SSL_VERIFY_NONE:
			return SSL_VERIFY_NONE;
		case BCTBX_SSL_VERIFY_OPTIONAL:
		case BCTBX_SSL_VERIFY_REQUIRED:
			return SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
		default:
			return BCTBX_ERROR_INVALID_SSL_AUTHMODE;
	}
}

int skip_certificate_verification(BCTBX_UNUSED(int preverify_ok), BCTBX_UNUSED(X509_STORE_CTX *x509_ctx)) {
	return 1;
}

const char *authModeToString(int authmode) {
	switch (authmode) {
		CASE_OF_RET(BCTBX_SSL_VERIFY_NONE);
		CASE_OF_RET(BCTBX_SSL_VERIFY_OPTIONAL);
		CASE_OF_RET(BCTBX_SSL_VERIFY_REQUIRED);
		default:
			return "";
	}
}

const char *sslVerifyModeToString(int mode) {
	switch (mode) {
		CASE_OF_RET(SSL_VERIFY_NONE);
		CASE_OF_RET(SSL_VERIFY_PEER);
		CASE_OF_RET(BCTBX_ERROR_INVALID_SSL_AUTHMODE);
		default:
			return "";
	}
}

int32_t bctbx_ssl_config_set_authmode(bctbx_ssl_config_t *ssl_config, int authmode) {
	if (ssl_config == NULL) {
		return BCTBX_ERROR_INVALID_SSL_CONFIG;
	}
	/* remap input arguments */
	int mode = getSslVerifyMode(authmode);
	if (mode < 0) {
		return mode;
	}

	ssl_config->ssl_verification_mode = mode;

	if (authmode == BCTBX_SSL_VERIFY_OPTIONAL) {
		SSL_CTX_set_verify(ssl_config->ssl_ctx, mode, skip_certificate_verification);
	} else {
		SSL_CTX_set_verify(ssl_config->ssl_ctx, mode, NULL);
	}

	return 0;
}

int32_t bctbx_ssl_config_set_rng(BCTBX_UNUSED(bctbx_ssl_config_t *ssl_config),
                                 BCTBX_UNUSED(int (*rng_function)(void *, unsigned char *, size_t)),
                                 BCTBX_UNUSED(void *rng_context)) {
	return BCTBX_ERROR_UNAVAILABLE_FUNCTION;
}

int32_t bctbx_ssl_config_set_callback_verify(
    bctbx_ssl_config_t *ssl_config,
    BCTBX_UNUSED(int (*callback_function)(void *, bctbx_x509_certificate_t *, int, uint32_t *)),
    BCTBX_UNUSED(void *callback_data)) {
	if (ssl_config == NULL) {
		return BCTBX_ERROR_INVALID_SSL_CONFIG;
	}

	// Cannot be  supported as is because of lack of support from openssl to pass data to the verification callback
	SSL_CTX_set_verify(ssl_config->ssl_ctx, ssl_config->ssl_verification_mode, NULL);

	return 0;
}

typedef struct client_cert_callback_data {
	int (*function)(void *, bctbx_ssl_context_t *, const bctbx_list_t *);
	void *data;
} client_cert_callback_ctx_t;

static int client_cert_callback(SSL *ssl, X509 **client_cert, EVP_PKEY **client_key) {
	client_cert_callback_ctx_t *cb_ctx = SSL_get_ex_data(ssl, OPENSSL_CLIENT_CERT_CALLBACK_EX_DATA_IDX);
	bctbx_ssl_context_t *ssl_ctx = SSL_get_ex_data(ssl, OPENSSL_BCTBX_SSL_CTX_EX_DATA_IDX);
	if (cb_ctx != NULL) {
		bctbx_list_t *subjects = bctbx_x509_certificate_get_subjects(bctbx_ssl_get_peer_certificate(ssl_ctx));
		cb_ctx->function(cb_ctx->data, ssl_ctx, subjects);
		bctbx_list_free_with_data(subjects, bctbx_free);
	}
	*client_cert = SSL_get_certificate(ssl);
	*client_key = SSL_get_privatekey(ssl);
	EVP_PKEY_up_ref(*client_key);
	X509_up_ref(*client_cert);
	return 1;
}

int32_t
bctbx_ssl_config_set_callback_cli_cert(bctbx_ssl_config_t *ssl_config,
                                       int (*callback_function)(void *, bctbx_ssl_context_t *, const bctbx_list_t *),
                                       void *callback_data) {
	if (ssl_config == NULL) {
		return BCTBX_ERROR_INVALID_SSL_CONFIG;
	}
	client_cert_callback_ctx_t *data = bctbx_malloc0(sizeof(client_cert_callback_ctx_t));
	data->data = callback_data;
	data->function = callback_function;
	SSL_CTX_set_ex_data(ssl_config->ssl_ctx, OPENSSL_CLIENT_CERT_CALLBACK_EX_DATA_IDX, data);
	SSL_CTX_set_client_cert_cb(ssl_config->ssl_ctx, client_cert_callback);
	return 0;
}

int32_t bctbx_ssl_config_set_ca_chain(bctbx_ssl_config_t *ssl_config, bctbx_x509_certificate_t *ca_chain) {
	if (ssl_config == NULL) {
		return BCTBX_ERROR_INVALID_SSL_CONFIG;
	}
	X509_STORE *cert_store = SSL_CTX_get_cert_store(ssl_config->ssl_ctx);
	for (int i = 0; i < sk_X509_num((STACK_OF(X509) *)ca_chain); i++) {
		X509 *certificate = sk_X509_value((STACK_OF(X509) *)ca_chain, i);
		if (certificate != NULL && !X509_STORE_add_cert(cert_store, certificate)) {
			return ERR_get_error();
		}
	}
	return 0;
}

int32_t bctbx_ssl_config_set_own_cert(bctbx_ssl_config_t *ssl_config,
                                      bctbx_x509_certificate_t *cert,
                                      bctbx_signing_key_t *key) {
	if (ssl_config == NULL) {
		return BCTBX_ERROR_INVALID_SSL_CONFIG;
	}

	if (cert == NULL || sk_X509_num((STACK_OF(X509) *)cert) != 1) {
		return BCTBX_ERROR_INVALID_CERTIFICATE;
	}
	if (key == NULL) {
		return BCTBX_ERROR_UNABLE_TO_PARSE_KEY;
	}

	X509 *certificate = sk_X509_value((STACK_OF(X509) *)cert, 0);
	return SSL_CTX_use_certificate(ssl_config->ssl_ctx, certificate) &&
	               SSL_CTX_use_PrivateKey(ssl_config->ssl_ctx, key->evp_pkey)
	           ? 0
	           : ERR_get_error();
}

int32_t bctbx_ssl_config_set_groups(bctbx_ssl_config_t *ssl_config, const bctbx_list_t *groups) {
	char *string_groups = join(groups, ":");
	int ret = SSL_CTX_set1_groups_list(ssl_config->ssl_ctx, string_groups);
	bctbx_free(string_groups);
	return ret == 1 ? 0 : ERR_get_error();
}

/** DTLS SRTP functions **/
int32_t bctbx_ssl_get_dtls_srtp_key_material(bctbx_ssl_context_t *ssl_ctx, uint8_t *output, size_t *output_length) {
	int ret = 0;
	if (ssl_ctx == NULL) {
		return BCTBX_ERROR_INVALID_SSL_CONTEXT;
	}
	const char *label = "EXTRACTOR-dtls_srtp";
	ret = SSL_export_keying_material(ssl_ctx->ssl, output, *output_length, label, strlen(label), NULL, 0, 0);

	return ret == 1 ? 0 : ERR_get_error();
}

int32_t bctbx_ssl_config_set_dtls_srtp_protection_profiles(bctbx_ssl_config_t *ssl_config,
                                                           const bctbx_dtls_srtp_profile_t *profiles,
                                                           size_t profiles_number) {
	if (ssl_config == NULL) {
		return BCTBX_ERROR_INVALID_SSL_CONFIG;
	}

	bctbx_list_t *openssl_protection_profiles = NULL;
	for (size_t i = 0; i < profiles_number; i++) {
		openssl_protection_profiles =
		    bctbx_list_append(openssl_protection_profiles, (char *)bctbx_srtp_profile_bctoolbox2openssl(profiles[i]));
	}

	char *srtp_protection_profiles_string = join(openssl_protection_profiles, ":");
	bctbx_list_free(openssl_protection_profiles);

	int ret = SSL_CTX_set_tlsext_use_srtp(ssl_config->ssl_ctx, srtp_protection_profiles_string);

	bctbx_free(srtp_protection_profiles_string);

	return ret == 1 ? 0 : ERR_get_error();
}

/** DTLS SRTP functions **/

int32_t bctbx_ssl_context_setup(bctbx_ssl_context_t *ssl_ctx, bctbx_ssl_config_t *ssl_config) {
	/* Check validity of context and config */
	if (ssl_config == NULL) {
		return BCTBX_ERROR_INVALID_SSL_CONFIG;
	}

	if (ssl_ctx == NULL) {
		return BCTBX_ERROR_INVALID_SSL_CONTEXT;
	}

	if (ssl_ctx->ssl) {
		SSL_free(ssl_ctx->ssl);
		void *ex_data = SSL_get_ex_data(ssl_ctx->ssl, OPENSSL_CLIENT_CERT_CALLBACK_EX_DATA_IDX);
		if (ex_data) {
			bctbx_free(ex_data);
		}
	}
	ssl_ctx->ssl = SSL_new(ssl_config->ssl_ctx);

	int ret = SSL_set_mode(ssl_ctx->ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
	if (!(ret & SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER)) {
		bctbx_warning("SSL_set_mode() did not set SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER");
	}

	void *client_cert_cb = SSL_CTX_get_ex_data(ssl_config->ssl_ctx, OPENSSL_CLIENT_CERT_CALLBACK_EX_DATA_IDX);
	SSL_set_ex_data(ssl_ctx->ssl, OPENSSL_BCTBX_SSL_CTX_EX_DATA_IDX, ssl_ctx);
	SSL_set_ex_data(ssl_ctx->ssl, OPENSSL_CLIENT_CERT_CALLBACK_EX_DATA_IDX, client_cert_cb);

	// has to specifically call either before doing the handshake even though we have selected specific client or server
	// methods...
	if (ssl_config->ssl_method == DTLS_server_method() || ssl_config->ssl_method == TLS_server_method()) {
		SSL_set_accept_state(ssl_ctx->ssl);
	} else {
		SSL_set_connect_state(ssl_ctx->ssl);
	}

	return ssl_ctx->ssl == NULL ? BCTBX_ERROR_INVALID_SSL_CONFIG : 0;
}

/*****************************************************************************/
/***** Hashing                                                           *****/
/*****************************************************************************/

static void bctbx_hmac(const EVP_MD *md,
                       const uint8_t *key,
                       size_t key_length,
                       const uint8_t *input,
                       size_t input_length,
                       uint8_t output_length,
                       uint8_t *output) {
	const size_t md_output_length = EVP_MD_size(md);
	uint8_t *md_output = bctbx_malloc0(md_output_length);
	unsigned int md_written = 0;

	if (key == NULL) {
		EVP_Digest(input, input_length, md_output, &md_written, md, NULL);
	} else {
		HMAC(md, key, key_length, input, input_length, md_output, &md_written);
	}

	size_t nCopyBytes = output_length > md_written ? md_written : output_length;
	memcpy(output, md_output, nCopyBytes);

	bctbx_clean(md_output, md_output_length);
	bctbx_free(md_output);
}

void bctbx_digest(const EVP_MD *md, const uint8_t *input, size_t input_length, uint8_t output_length, uint8_t *output) {
	bctbx_hmac(md, NULL, 0, input, input_length, output_length, output);
}

void bctbx_hmacSha512(const uint8_t *key,
                      size_t keyLength,
                      const uint8_t *input,
                      size_t inputLength,
                      uint8_t hmacLength,
                      uint8_t *output) {
	bctbx_hmac(EVP_sha512(), key, keyLength, input, inputLength, hmacLength, output);
}

void bctbx_hmacSha384(const uint8_t *key,
                      size_t keyLength,
                      const uint8_t *input,
                      size_t inputLength,
                      uint8_t hmacLength,
                      uint8_t *output) {
	bctbx_hmac(EVP_sha384(), key, keyLength, input, inputLength, hmacLength, output);
}

void bctbx_hmacSha256(const uint8_t *key,
                      size_t keyLength,
                      const uint8_t *input,
                      size_t inputLength,
                      uint8_t hmacLength,
                      uint8_t *output) {
	bctbx_hmac(EVP_sha256(), key, keyLength, input, inputLength, hmacLength, output);
}

void bctbx_sha512(const uint8_t *input, size_t inputLength, uint8_t hashLength, uint8_t *output) {
	bctbx_digest(EVP_sha512(), input, inputLength, hashLength, output);
}

void bctbx_sha384(const uint8_t *input, size_t inputLength, uint8_t hashLength, uint8_t *output) {
	bctbx_digest(EVP_sha384(), input, inputLength, hashLength, output);
}

void bctbx_sha256(const uint8_t *input, size_t inputLength, uint8_t hashLength, uint8_t *output) {
	bctbx_digest(EVP_sha256(), input, inputLength, hashLength, output);
}

void bctbx_hmacSha1(const uint8_t *key,
                    size_t keyLength,
                    const uint8_t *input,
                    size_t inputLength,
                    uint8_t hmacLength,
                    uint8_t *output) {
	bctbx_hmac(EVP_sha1(), key, keyLength, input, inputLength, hmacLength, output);
}

void bctbx_md5(const uint8_t *input, size_t inputLength, uint8_t output[16]) {
	bctbx_digest(EVP_md5(), input, inputLength, 16, output);
}

/*****************************************************************************/
/***** Encryption/Decryption                                             *****/
/*****************************************************************************/
/***** GCM *****/

static const EVP_CIPHER *get_evp_aes_gcm(size_t key_byte_length) {
	switch (key_byte_length) {
		case 16:
			return EVP_aes_128_gcm();
		case 24:
			return EVP_aes_192_gcm();
		case 32:
			return EVP_aes_256_gcm();
		default:
			return NULL;
	}
}

int32_t bctbx_aes_gcm_encrypt_and_tag(const uint8_t *key,
                                      size_t keyLength,
                                      const uint8_t *plainText,
                                      size_t plainTextLength,
                                      const uint8_t *authenticatedData,
                                      size_t authenticatedDataLength,
                                      const uint8_t *initializationVector,
                                      size_t initializationVectorLength,
                                      uint8_t *tag,
                                      size_t tagLength,
                                      uint8_t *output) {
	/* Influenced by https://wiki.openssl.org/index.php/EVP_Authenticated_Encryption_and_Decryption */
	bctbx_aes_gcm_context_t *ctx =
	    bctbx_aes_gcm_context_new(key, keyLength, authenticatedData, authenticatedDataLength, initializationVector,
	                              initializationVectorLength, BCTBX_GCM_ENCRYPT);
	if (ctx == NULL) return BCTBX_ERROR_INVALID_INPUT_DATA;

	int ret = bctbx_aes_gcm_process_chunk(ctx, plainText, plainTextLength, output);

	if (ret == 0) {
		ret = bctbx_aes_gcm_finish(ctx, tag, tagLength);
	}

	return ret;
}

int32_t bctbx_aes_gcm_decrypt_and_auth(const uint8_t *key,
                                       size_t keyLength,
                                       const uint8_t *cipherText,
                                       size_t cipherTextLength,
                                       const uint8_t *authenticatedData,
                                       size_t authenticatedDataLength,
                                       const uint8_t *initializationVector,
                                       size_t initializationVectorLength,
                                       const uint8_t *tag,
                                       size_t tagLength,
                                       uint8_t *output) {
	int ret;

	bctbx_aes_gcm_context_t *ctx =
	    bctbx_aes_gcm_context_new(key, keyLength, authenticatedData, authenticatedDataLength, initializationVector,
	                              initializationVectorLength, BCTBX_GCM_DECRYPT);
	if (ctx == NULL) {
		return BCTBX_ERROR_INVALID_INPUT_DATA;
	}

	ret = bctbx_aes_gcm_process_chunk(ctx, cipherText, cipherTextLength, output);

	if (ret == 0) {
		ret = bctbx_aes_gcm_finish(ctx, (uint8_t *)tag, tagLength);
	}

	return ret;
}

bctbx_aes_gcm_context_t *bctbx_aes_gcm_context_new(const uint8_t *key,
                                                   size_t keyLength,
                                                   const uint8_t *authenticatedData,
                                                   size_t authenticatedDataLength,
                                                   const uint8_t *initializationVector,
                                                   size_t initializationVectorLength,
                                                   uint8_t mode) {
	int len;

	const EVP_CIPHER *cipher = get_evp_aes_gcm(keyLength);
	if (cipher == NULL) {
		return NULL;
	}

	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL) {
		return NULL;
	}

	if (1 == EVP_CipherInit_ex(ctx, cipher, NULL, NULL, NULL, mode == BCTBX_GCM_ENCRYPT) &&
	    1 == EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, initializationVectorLength, NULL) &&
	    1 == EVP_CipherInit_ex(ctx, NULL, NULL, key, initializationVector, mode == BCTBX_GCM_ENCRYPT)
	    /* NULL passed as out is AEAD specific */
	    /* See "AEAD INTERFACE": https://www.openssl.org/docs/man3.1/man3/EVP_EncryptInit.html */
	    && 1 == EVP_CipherUpdate(ctx, NULL, &len, authenticatedData, authenticatedDataLength)) {
		return (bctbx_aes_gcm_context_t *)ctx;
	} else {
		EVP_CIPHER_CTX_free(ctx);
		return NULL;
	}
}

int32_t bctbx_aes_gcm_process_chunk(bctbx_aes_gcm_context_t *context,
                                    const uint8_t *input,
                                    size_t inputLength,
                                    uint8_t *output) {
	int len;
	return 1 == EVP_CipherUpdate((EVP_CIPHER_CTX *)context, output, &len, input, inputLength) ? 0 : -1;
}

int32_t bctbx_aes_gcm_finish(bctbx_aes_gcm_context_t *context, uint8_t *tag, size_t tagLength) {
	int ret = 1;
	int len;
	EVP_CIPHER_CTX *ctx = (EVP_CIPHER_CTX *)context;

	if (0 == EVP_CIPHER_CTX_encrypting(ctx)) {
		/* Set expected tag value. Works in OpenSSL 1.0.1d and later */
		ret = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tagLength, (void *)tag);
	}

	if (ret == 1) ret = EVP_CipherFinal_ex(ctx, NULL, &len);

	if (ret == 1 && 1 == EVP_CIPHER_CTX_encrypting(ctx)) {
		ret = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tagLength, tag);
	}

	EVP_CIPHER_CTX_cleanup(ctx);
	EVP_CIPHER_CTX_free(ctx);

	return ret == 1 ? 0 : BCTBX_ERROR_UNSPECIFIED_ERROR;
}

static void bctbx_evp_cipher_init_update_final(const EVP_CIPHER *cipher,
                                               int mode,
                                               const uint8_t *key,
                                               const uint8_t *IV,
                                               const uint8_t *input,
                                               size_t inputLength,
                                               uint8_t *output) {
	int len = 0;

	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	if (ctx == NULL) {
		return;
	}
	EVP_CipherInit_ex(ctx, cipher, NULL, NULL, NULL, mode == BCTBX_GCM_ENCRYPT);
	EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 16, NULL);
	EVP_CipherInit_ex(ctx, NULL, NULL, key, IV, mode == BCTBX_GCM_ENCRYPT);
	EVP_CipherUpdate(ctx, output, &len, input, inputLength);
	EVP_CipherFinal_ex(ctx, output + len, &len);
	EVP_CIPHER_CTX_cleanup(ctx);
	EVP_CIPHER_CTX_free(ctx);
}

void bctbx_aes128CfbEncrypt(
    const uint8_t *key, const uint8_t *IV, const uint8_t *input, size_t inputLength, uint8_t *output) {

	bctbx_evp_cipher_init_update_final(EVP_aes_128_cfb(), BCTBX_GCM_ENCRYPT, key, IV, input, inputLength, output);
}

void bctbx_aes128CfbDecrypt(
    const uint8_t *key, const uint8_t *IV, const uint8_t *input, size_t inputLength, uint8_t *output) {
	bctbx_evp_cipher_init_update_final(EVP_aes_128_cfb(), BCTBX_GCM_DECRYPT, key, IV, input, inputLength, output);
}

void bctbx_aes256CfbEncrypt(
    const uint8_t *key, const uint8_t *IV, const uint8_t *input, size_t inputLength, uint8_t *output) {
	bctbx_evp_cipher_init_update_final(EVP_aes_256_cfb(), BCTBX_GCM_ENCRYPT, key, IV, input, inputLength, output);
}

void bctbx_aes256CfbDecrypt(
    const uint8_t *key, const uint8_t *IV, const uint8_t *input, size_t inputLength, uint8_t *output) {
	bctbx_evp_cipher_init_update_final(EVP_aes_256_cfb(), BCTBX_GCM_DECRYPT, key, IV, input, inputLength, output);
}
