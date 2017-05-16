/*
	bctoolbox
    Copyright (C) 2017  Belledonne Communications SARL


    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <bctoolbox/crypto.h>

#ifdef HAVE_DECAF

#include "decaf.h"
#include "decaf/ed255.h"
#include "decaf/ed448.h"

int bctbx_crypto_have_ecc(void) {
 return TRUE;
}

/*****************************************************************************/
/*** Elliptic Curve Diffie-Hellman - ECDH                                  ***/
/*****************************************************************************/

/* Create and initialise the ECDH Context */
bctbx_ECDHContext_t *bctbx_CreateECDHContext(uint8_t ECDHAlgo) {
	/* create the context */
	bctbx_ECDHContext_t *context = (bctbx_ECDHContext_t *)bctbx_malloc(sizeof(bctbx_ECDHContext_t));
	memset (context, 0, sizeof(bctbx_ECDHContext_t));

	/* initialise pointer to NULL to ensure safe call to free() when destroying context */
	context->secret = NULL;
	context->sharedSecret = NULL;
	context->selfPublic = NULL;
	context->peerPublic = NULL;
	context->cryptoModuleData = NULL; /* decaf do not use any context for these operations */

	/* set parameters in the context */
	context->algo=ECDHAlgo;

	switch (ECDHAlgo) {
		case BCTBX_ECDH_X25519:
			context->pointCoordinateLength = DECAF_X25519_PUBLIC_BYTES;
			context->secretLength = DECAF_X25519_PRIVATE_BYTES;
			break;
		case BCTBX_ECDH_X448:
			context->pointCoordinateLength = DECAF_X448_PUBLIC_BYTES;
			context->secretLength = DECAF_X448_PRIVATE_BYTES;
			break;
		default:
			bctbx_free(context);
			return NULL;
			break;
	}
	return context;
}

/**
 *
 * @brief	Set the given secret key in the ECDH context
 *
 * @param[in/out]	context		ECDH context, will store the given secret key if length is matching the pre-setted algo for this context
 * @param[in]		secret		The buffer holding the secret, is duplicated in the ECDH context
 * @param[in]		secretLength	Length of previous buffer, must match the algo type setted at context creation
 */
void bctbx_ECDHSetSecretKey(bctbx_ECDHContext_t *context, uint8_t *secret, size_t secretLength) {
	if (context!=NULL && context->secretLength==secretLength) {
		context->secret = (uint8_t *)bctbx_malloc(context->secretLength);
		memcpy(context->secret, secret, secretLength);
	}
}

/**
 *
 * @brief	Derive the public key from the secret setted in context and using preselected algo, following RFC7748
 *
 * @param[in/out]	context		The context holding algo setting and secret, used to store public key
 */
void bctbx_ECDHDerivePublicKey(bctbx_ECDHContext_t *context) {
	if (context!=NULL && context->secret!=NULL) {
		/* allocate public key buffer */
		context->selfPublic = (uint8_t *)bctbx_malloc(context->pointCoordinateLength);

		/* then generate the public value */
		switch (context->algo) {
			case BCTBX_ECDH_X25519:
				decaf_x25519_derive_public_key(context->selfPublic, context->secret);
				break;
			case BCTBX_ECDH_X448:
				decaf_x448_derive_public_key(context->selfPublic, context->secret);
				break;
			default:
				break;
		}
	}
}

/* generate the random secret and compute the public value */
void bctbx_ECDHCreateKeyPair(bctbx_ECDHContext_t *context, int (*rngFunction)(void *, uint8_t *, size_t), void *rngContext) {
	if (context!=NULL) {
		/* first generate the random bytes of self secret and store it in context(do it directly instead of creating a temp buffer and calling SetSecretKey) */
		context->secret = (uint8_t *)bctbx_malloc(context->secretLength);
		rngFunction(rngContext, context->secret, context->secretLength);

		/* Then derive the public key */
		bctbx_ECDHDerivePublicKey(context);
	}
}

/* compute secret - the ->peerPublic field of context must have been set before calling this function */
void bctbx_ECDHComputeSecret(bctbx_ECDHContext_t *context, int (*rngFunction)(void *, uint8_t *, size_t), void *rngContext) {
	if (context != NULL && context->secret!=NULL && context->peerPublic!=NULL) {
		/* allocate shared secret buffer */
		context->sharedSecret = (uint8_t *)bctbx_malloc(context->pointCoordinateLength);

		switch (context->algo) {
			case BCTBX_ECDH_X25519:
				if (decaf_x25519(context->sharedSecret, context->peerPublic, context->secret)==DECAF_FAILURE) {
					bctbx_free(context->sharedSecret);
					context->sharedSecret=NULL;
				}
				break;
			case BCTBX_ECDH_X448:
				if (decaf_x448(context->sharedSecret, context->peerPublic, context->secret)==DECAF_FAILURE) {
					bctbx_free(context->sharedSecret);
					context->sharedSecret=NULL;
				}
				break;
			default:
				break;
		}
	}
}

/* clean DHM context */
void bctbx_DestroyECDHContext(bctbx_ECDHContext_t *context) {
	if (context!= NULL) {
		/* key and secret must be erased from memory and not just freed */
		if (context->secret != NULL) {
			memset(context->secret, 0, context->secretLength);
			free(context->secret);
		}
		free(context->selfPublic);
		if (context->sharedSecret != NULL) {
			memset(context->sharedSecret, 0, context->pointCoordinateLength);
			free(context->sharedSecret);
		}
		free(context->peerPublic);
		free(context->cryptoModuleData);
		free(context);
	}
}


/*****************************************************************************/
/*** Edwards Curve Digital Signature Algorithm - EdDSA                     ***/
/*****************************************************************************/

/* Create and initialise the EDDSA Context */
bctbx_EDDSAContext_t *bctbx_CreateEDDSAContext(uint8_t EDDSAAlgo) {
	/* create the context */
	bctbx_EDDSAContext_t *context = (bctbx_EDDSAContext_t *)bctbx_malloc(sizeof(bctbx_EDDSAContext_t));
	memset (context, 0, sizeof(bctbx_EDDSAContext_t));

	/* initialise pointer to NULL to ensure safe call to free() when destroying context */
	context->secretKey = NULL;
	context->publicKey = NULL;
	context->cryptoModuleData = NULL; /* decaf do not use any context for these operations */

	/* set parameters in the context */
	context->algo=EDDSAAlgo;

	switch (EDDSAAlgo) {
		case BCTBX_EDDSA_25519:
			context->pointCoordinateLength = DECAF_EDDSA_25519_PUBLIC_BYTES;
			context->secretLength = DECAF_EDDSA_25519_PRIVATE_BYTES;
			break;
		case BCTBX_EDDSA_448:
			context->pointCoordinateLength = DECAF_EDDSA_448_PUBLIC_BYTES;
			context->secretLength = DECAF_EDDSA_448_PRIVATE_BYTES;
			break;
		default:
			bctbx_free(context);
			return NULL;
			break;
	}
	return context;
}

/* generate the random secret and compute the public value */
void bctbx_EDDSACreateKeyPair(bctbx_EDDSAContext_t *context, int (*rngFunction)(void *, uint8_t *, size_t), void *rngContext) {
	/* first generate the random bytes of self secret and store it in context */
	context->secretKey = (uint8_t *)bctbx_malloc(context->secretLength);
	rngFunction(rngContext, context->secretKey, context->secretLength);

	/* then generate the public value */
	bctbx_EDDSADerivePublicKey(context);
}

/* using the secret key present in context, derive the public one */
void bctbx_EDDSADerivePublicKey(bctbx_EDDSAContext_t *context) {
	/* check we have a context and it was set to get a public key matching the length of the given one */
	if (context != NULL) {
		if (context->secretKey != NULL) { /* don't go further if we have no secret key in context */
			if (context->publicKey != NULL) { /* delete existing key if any */
				bctbx_free(context->publicKey);
			}
			context->publicKey = bctbx_malloc(context->pointCoordinateLength);

			/* then generate the public value */
			switch (context->algo) {
				case BCTBX_EDDSA_25519:
					decaf_ed25519_derive_public_key(context->publicKey, context->secretKey);
					break;
				case BCTBX_EDDSA_448:
					decaf_ed448_derive_public_key(context->publicKey, context->secretKey);
					break;
				default:
					break;
			}
		}
	}
}

/* clean EDDSA context */
void bctbx_DestroyEDDSAContext(bctbx_EDDSAContext_t *context) {
	if (context!= NULL) {
		/* secretKey must be erased from memory and not just freed */
		if (context->secretKey != NULL) {
			memset(context->secretKey, 0, context->secretLength);
			free(context->secretKey);
		}
		free(context->publicKey);
		free(context->cryptoModuleData);
		free(context);
	}
}

/**
 *
 * @brief Sign the message given using private key and EdDSA algo set in context
 *
 * @param[in]		context			EDDSA context storing the algorithm to use(ed448 or ed25519) and the private key to use
 * @param[in]		message			The message to be signed
 * @param[in]		messageLength		Length of the message buffer
 * @param [in]		associatedData		A "context" for this signature of up to 255 bytes.
 * @param [in]		associatedDataLength	Length of the context.
 * @param[out]		signature		The signature
 * @param[in/out]	signatureLength		The size of the signature buffer as input, the size of the actual signature as output
 *
 */
void bctbx_EDDSA_sign(bctbx_EDDSAContext_t *context, const uint8_t *message, const size_t messageLength, const uint8_t *associatedData, const size_t associatedDataLength, uint8_t *signature, size_t *signatureLength) {
	if (context!=NULL) {
		switch (context->algo) {
			case BCTBX_EDDSA_25519:
				if (*signatureLength>=DECAF_EDDSA_25519_SIGNATURE_BYTES) { /* check the buffer is large enough to hold the signature */
					decaf_ed25519_sign ( signature, context->secretKey, context->publicKey, message, messageLength, 0, associatedData, associatedDataLength);
					*signatureLength=DECAF_EDDSA_25519_SIGNATURE_BYTES;
					return;
				}
				break;
			case BCTBX_EDDSA_448:
				if (*signatureLength>=DECAF_EDDSA_448_SIGNATURE_BYTES) { /* check the buffer is large enough to hold the signature */
					decaf_ed448_sign ( signature, context->secretKey, context->publicKey, message, messageLength, 0, associatedData, associatedDataLength); 
					*signatureLength=DECAF_EDDSA_448_SIGNATURE_BYTES;
					return;
				}
				break;
		default:
			break;
		}
	}
	*signatureLength=0;
}

/**
 *
 * @brief Set a public key in a EDDSA context to be used to verify messages signature
 *
 * @param[in/out]	context		EDDSA context storing the algorithm to use(ed448 or ed25519)
 * @param[in]		publicKey	The public to store in context
 * @param[in]		publicKeyLength	The length of previous buffer
 */
void bctbx_EDDSA_setPublicKey(bctbx_EDDSAContext_t *context, uint8_t *publicKey, size_t publicKeyLength) {
	/* check we have a context and it was set to get a public key matching the length of the given one */
	if (context != NULL) {
		if (context->pointCoordinateLength == publicKeyLength) {
			if (context->publicKey != NULL) { /* delete existing key if any */
				bctbx_free(context->publicKey);
			}
			context->publicKey = bctbx_malloc(publicKeyLength);
			memcpy(context->publicKey, publicKey, publicKeyLength);
		}
	}
}
/**
 *
 * @brief Set a private key in a EDDSA context to be used to sign message
 *
 * @param[in/out]	context		EDDSA context storing the algorithm to use(ed448 or ed25519)
 * @param[in]		secretKey	The secret to store in context
 * @param[in]		secretKeyLength	The length of previous buffer
 */
void bctbx_EDDSA_setSecretKey(bctbx_EDDSAContext_t *context, uint8_t *secretKey, size_t secretKeyLength) {
	/* check we have a context and it was set to get a public key matching the length of the given one */
	if (context != NULL) {
		if (context->secretLength == secretKeyLength) {
			if (context->secretKey != NULL) { /* delete existing key if any */
				bctbx_free(context->secretKey);
			}
			context->secretKey = bctbx_malloc(secretKeyLength);
			memcpy(context->secretKey, secretKey, secretKeyLength);
		}
	}
}

/**
 *
 * @brief Use the public key set in context to verify the given signature and message
 *
 * @param[in/out]	context			EDDSA context storing the algorithm to use(ed448 or ed25519) and public key
 * @param[in]		message			Message to verify
 * @param[in]		messageLength		Length of the message buffer
 * @param [in]		associatedData		A "context" for this signature of up to 255 bytes.
 * @param [in]		associatedDataLength	Length of the context.
 * @param[in]		signature		The signature
 * @param[in]		signatureLength		The size of the signature buffer
 *
 * @return BCTBX_VERIFY_SUCCESS or BCTBX_VERIFY_FAILED
 */
int bctbx_EDDSA_verify(bctbx_EDDSAContext_t *context, const uint8_t *message, size_t messageLength, const uint8_t *associatedData, const size_t associatedDataLength, uint8_t *signature, size_t signatureLength) {
	int ret = BCTBX_VERIFY_FAILED;
	if (context!=NULL) {
		decaf_error_t retDecaf = DECAF_FAILURE;
		switch (context->algo) {
			case BCTBX_EDDSA_25519:
				if (signatureLength==DECAF_EDDSA_25519_SIGNATURE_BYTES) { /* check length of given signature */
					retDecaf = decaf_ed25519_verify (signature, context->publicKey, message, messageLength, 0, associatedData, associatedDataLength);
				}
				break;
			case BCTBX_EDDSA_448:
				if (signatureLength==DECAF_EDDSA_448_SIGNATURE_BYTES) { /* check lenght of given signature */
					retDecaf = decaf_ed448_verify (signature, context->publicKey, message, messageLength, 0, associatedData, associatedDataLength);
				}
				break;
			default:
				break;
		}
		if (retDecaf == DECAF_SUCCESS) {
			ret = BCTBX_VERIFY_SUCCESS;
		}
	}
	return ret;
}

/**
 *
 * @brief Convert a EDDSA private key to a ECDH private key
 *      pass the EDDSA private key through the hash function used in EdDSA
 *
 * @param[in]	ed	Context holding the current private key to convert
 * @param[out]	x	Context to store the private key for x25519 key exchange
*/
void bctbx_EDDSA_ECDH_privateKeyConversion(const bctbx_EDDSAContext_t *ed, bctbx_ECDHContext_t *x) {
	if (ed!=NULL && x!=NULL && ed->secretKey!=NULL) {
		if (ed->algo == BCTBX_EDDSA_25519 && x->algo == BCTBX_ECDH_X25519) {
			if (x->secret!=NULL) {
				bctbx_free(x->secret);
			}
			x->secret = bctbx_malloc(x->secretLength);
			decaf_ed25519_convert_private_key_to_x25519(x->secret, ed->secretKey);
		} else if (ed->algo == BCTBX_EDDSA_448 && x->algo == BCTBX_ECDH_X448) {
			if (x->secret!=NULL) {
				bctbx_free(x->secret);
			}
			x->secret = bctbx_malloc(x->secretLength);
			decaf_ed448_convert_private_key_to_x448(x->secret, ed->secretKey);
		}
	}
}

/**
 *
 * @brief Convert a EDDSA public key to a ECDH public key
 * 	point conversion : montgomeryX = (edwardsY + 1)*inverse(1 - edwardsY) mod p
 *
 * @param[in]	ed	Context holding the current public key to convert
 * @param[out]	x	Context to store the public key for x25519 key exchange
 * @param[in]	isSelf	Flag to decide where to store the public key in context: BCTBX_ECDH_ISPEER or BCTBX_ECDH_ISPEER
*/
void bctbx_EDDSA_ECDH_publicKeyConversion(const bctbx_EDDSAContext_t *ed, bctbx_ECDHContext_t *x, uint8_t isSelf) {
	if (ed!=NULL && x!=NULL && ed->publicKey!=NULL) {
		if (ed->algo == BCTBX_EDDSA_25519 && x->algo == BCTBX_ECDH_X25519) {
			if (isSelf==BCTBX_ECDH_ISPEER) {
				if (x->peerPublic!=NULL) {
					bctbx_free(x->peerPublic);
				}
				x->peerPublic = bctbx_malloc(x->pointCoordinateLength);
				decaf_ed25519_convert_public_key_to_x25519(x->peerPublic, ed->publicKey);
			} else {
				if (x->selfPublic!=NULL) {
					bctbx_free(x->selfPublic);
				}
				x->selfPublic = bctbx_malloc(x->pointCoordinateLength);
				decaf_ed25519_convert_public_key_to_x25519(x->selfPublic, ed->publicKey);
			}
		} else if (ed->algo == BCTBX_EDDSA_448 && x->algo == BCTBX_ECDH_X448) {
			if (isSelf==BCTBX_ECDH_ISPEER) {
				if (x->peerPublic!=NULL) {
					bctbx_free(x->peerPublic);
				}
				x->peerPublic = bctbx_malloc(x->pointCoordinateLength);
				decaf_ed448_convert_public_key_to_x448(x->peerPublic, ed->publicKey);
			} else {
				if (x->selfPublic!=NULL) {
					bctbx_free(x->selfPublic);
				}
				x->selfPublic = bctbx_malloc(x->pointCoordinateLength);
				decaf_ed448_convert_public_key_to_x448(x->selfPublic, ed->publicKey);
			}
		}
	}

}

#else /* HAVE_DECAF */
/* We do not have lib decaf, implement empty stubs */
int bctbx_crypto_have_ecc(void) { return FALSE;}
bctbx_ECDHContext_t *bctbx_CreateECDHContext(uint8_t ECDHAlgo) {return NULL;}
void bctbx_ECDHCreateKeyPair(bctbx_ECDHContext_t *context, int (*rngFunction)(void *, uint8_t *, size_t), void *rngContext) {return;}
void bctbx_ECDHSetSecretKey(bctbx_ECDHContext_t *context, uint8_t *secret, size_t secretLength){return;}
void bctbx_ECDHDerivePublicKey(bctbx_ECDHContext_t *context){return;}
void bctbx_ECDHComputeSecret(bctbx_ECDHContext_t *context, int (*rngFunction)(void *, uint8_t *, size_t), void *rngContext){return;}
void bctbx_DestroyECDHContext(bctbx_ECDHContext_t *context){return;}

bctbx_EDDSAContext_t *bctbx_CreateEDDSAContext(uint8_t EDDSAAlgo) {return NULL;}
void bctbx_EDDSACreateKeyPair(bctbx_EDDSAContext_t *context, int (*rngFunction)(void *, uint8_t *, size_t), void *rngContext) {return;}
void bctbx_EDDSADerivePublicKey(bctbx_EDDSAContext_t *context) {return;}
void bctbx_DestroyEDDSAContext(bctbx_EDDSAContext_t *context) {return;}
void bctbx_EDDSA_sign(bctbx_EDDSAContext_t *context, const uint8_t *message, const size_t messageLength, const uint8_t *associatedData, const size_t associatedDataLength, uint8_t *signature, size_t *signatureLength) {return;}
void bctbx_EDDSA_setPublicKey(bctbx_EDDSAContext_t *context, uint8_t *publicKey, size_t publicKeyLength) {return;}
void bctbx_EDDSA_setSecretKey(bctbx_EDDSAContext_t *context, uint8_t *secretKey, size_t secretKeyLength) {return;}
int bctbx_EDDSA_verify(bctbx_EDDSAContext_t *context, const uint8_t *message, size_t messageLength, const uint8_t *associatedData, const size_t associatedDataLength, uint8_t *signature, size_t signatureLength) {return BCTBX_VERIFY_FAILED;}
void bctbx_EDDSA_ECDH_privateKeyConversion(const bctbx_EDDSAContext_t *ed, bctbx_ECDHContext_t *x) {return;}
void bctbx_EDDSA_ECDH_publicKeyConversion(const bctbx_EDDSAContext_t *ed, bctbx_ECDHContext_t *x, uint8_t isSelf) {return;}
#endif
