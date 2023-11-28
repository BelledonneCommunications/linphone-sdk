/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ms_srtp_h
#define ms_srtp_h

#include "mediastreamer2/mscommon.h"
#include <ortp/rtpsession.h>

#ifdef __cplusplus
extern "C" {
#endif
/* defined in mediastream.h */
#ifndef MS_MEDIA_STREAM_SESSIONS_DEFINED
typedef struct _MSMediaStreamSessions MSMediaStreamSessions;
#define MS_MEDIA_STREAM_SESSIONS_DEFINED 1
#endif

/**
 * @brief Enum describing algorithm used to exchange srtp keys.
 **/
typedef enum _MSSrtpKeySource {
	MSSrtpKeySourceUnknown = 0,       /**< Source of this key is unknown, for retrocompatibility */
	MSSrtpKeySourceSDES = 1,          /**< The Srtp keys were exchanged using SDES */
	MSSrtpKeySourceZRTP = 2,          /**< The Srtp keys were exchanged using ZRTP */
	MSSrtpKeySourceDTLS = 3,          /**< The Srtp keys were exchanged using DTLS-SRTP */
	MSSrtpKeySourceEKT = 4,           /**< The Srtp keys were exchanged using EKT */
	MSSrtpKeySourceUnavailable = 0xFF /**< The Srtp keys are not set for all sessions yet (stream is not secure)*/
} MSSrtpKeySource;

/*
 * Crypto suite used configure encrypted stream*/
typedef enum _MSCryptoSuite {
	MS_CRYPTO_SUITE_INVALID = 0,
	MS_AES_128_SHA1_80,
	MS_AES_128_SHA1_80_NO_AUTH,
	MS_AES_128_SHA1_80_SRTP_NO_CIPHER,
	MS_AES_128_SHA1_80_SRTCP_NO_CIPHER,
	MS_AES_128_SHA1_80_NO_CIPHER,
	MS_AES_256_SHA1_80,
	MS_AES_CM_256_SHA1_80,
	MS_AES_128_SHA1_32,
	MS_AES_128_SHA1_32_NO_AUTH,
	MS_AES_256_SHA1_32,
	MS_AEAD_AES_128_GCM,
	MS_AEAD_AES_256_GCM
} MSCryptoSuite;

typedef struct _MSCryptoSuiteNameParams {
	const char *name;
	const char *params;
} MSCryptoSuiteNameParams;

/* EKT as described in RFC 8870 */
typedef enum _MSEKTCipherType {
	MS_EKT_CIPHERTYPE_AESKW128 = 0x00,
	MS_EKT_CIPHERTYPE_AESKW256 = 0x01,
} MSEKTCipherType;

typedef enum _MSEKTMode {
	MS_EKT_DISABLED = 0, /**< EKT is not in operation */
	MS_EKT_ENABLED,  /**< EKT is used, we should be given an EKT and use it to produce EKT tag on sending and expect EKT
	                    tag on reception */
	MS_EKT_TRANSFER, /**< We are in transfer mode: we expect EKT tag at the end of the packet but cannot decrypt, just
	                   pass them */
	MS_EKT_DISABLED_WITH_TRANSFER /**< We are in transfer but wihtout EKT tag, only double encryption, so manage the OHB
	                               */
} MSEKTMode;

typedef struct _MSEKTParametersSet {
	MSEKTCipherType ekt_cipher_type;     /**< AESKW128 or AESKW256 */
	MSCryptoSuite ekt_srtp_crypto_suite; /**< The SRTP crypto suite to be used to protect the RTP packets with the key
	                                        encrypted with this EKT */
	uint8_t ekt_key_value[32];   /**< The EKTKey that the recipient should use when generating EKTCiphertext values,
	                                actual size depends on ekt_cipher_type */
	uint8_t ekt_master_salt[14]; /**< The SRTP master salt to be used with any master key encrypted with this EKT Key,
	                                actual size depends on ekt_srtp_crypto_suite */
	uint16_t ekt_spi;            /**< reference this EKTKey and SRTP master salt */
	uint32_t ekt_ttl; /**< The maximum amount of time, in seconds, that this EKTKey can be used.(on 24 bits) */
} MSEKTParametersSet;

MS2_PUBLIC MSCryptoSuite ms_crypto_suite_build_from_name_params(const MSCryptoSuiteNameParams *nameparams);
MS2_PUBLIC int ms_crypto_suite_to_name_params(MSCryptoSuite cs, MSCryptoSuiteNameParams *nameparams);
MS2_PUBLIC bool_t ms_crypto_suite_is_unencrypted(MSCryptoSuite cs);
MS2_PUBLIC bool_t ms_crypto_suite_is_unauthenticated(MSCryptoSuite cs);

/* defined in srtp.h*/
typedef struct _MSSrtpCtx MSSrtpCtx;

/**
 * Check if SRTP is supported
 * @return true if SRTP is supported
 */
MS2_PUBLIC bool_t ms_srtp_supported(void);

/**
 * Set encryption requirements.
 * srtp session might be created/deleted depending on requirement parameter and already set keys
 * @param[in/out]	sessions	The sessions associated to the current media stream
 * @param[in]		yesno		If yes, any incoming/outgoing rtp packets are silently discarded.
 * until keys are provided using functions #media_stream_set_srtp_recv_key_b64 or #media_stream_set_srtp_recv_key
 * @return	0 on success, error code otherwise
 */

MS2_PUBLIC int ms_media_stream_sessions_set_encryption_mandatory(MSMediaStreamSessions *sessions, bool_t yesno);

/**
 * Get encryption requirements.
 * @param[in/out]	sessions	The sessions associated to the current media stream
 * @return	TRUE if only encrypted rtp packet shall be sent/received
 */

MS2_PUBLIC bool_t ms_media_stream_sessions_get_encryption_mandatory(const MSMediaStreamSessions *sessions);

/**
 * Set srtp receiver key for the given media stream.
 * If no srtp session exists on the stream it is created, if it already exists srtp policy is created/modified for the
 * receiver side of the stream.
 *
 * @param[in/out]	sessions	The sessions associated to the current media stream
 * @param[in]		suite		The srtp crypto suite to use
 * @param[in]		key		Srtp master key and master salt in a base 64 NULL terminated string
 * @param[in]		source	algorithm used to exchange this key
 * @return	0 on success, error code otherwise
 */
MS2_PUBLIC int ms_media_stream_sessions_set_srtp_recv_key_b64(MSMediaStreamSessions *sessions,
                                                              MSCryptoSuite suite,
                                                              const char *key,
                                                              MSSrtpKeySource source);

/**
 * Set srtp receiver key for the given media stream.
 * If no srtp session exists on the stream it is created, if it already exists srtp policy is created/modified for the
 * receiver side of the stream.
 *
 * @param[in/out]	sessions	The sessions associated to the current media stream
 * @param[in]		suite		The srtp crypto suite to use
 * @param[in]		key		Srtp master key and master salt
 * @param[in]		key_length	key buffer length
 * @param[in]		source		algorithm used to exchange this key
 * @return	0 on success, error code otherwise
 */
MS2_PUBLIC int ms_media_stream_sessions_set_srtp_recv_key(MSMediaStreamSessions *sessions,
                                                          MSCryptoSuite suite,
                                                          const uint8_t *key,
                                                          size_t key_length,
                                                          MSSrtpKeySource source);

/**
 * Set srtp inner receiver key for the given media stream.
 * This is used for double encryption only (RFC8723)
 * If no outer srtp session exists on the stream returns an error
 *
 * @param[in/out]	sessions	The sessions associated to the current media stream
 * @param[in]		suite		The srtp crypto suite to use
 * @param[in]		key		Srtp master key and master salt
 * @param[in]		key_length	key buffer length
 * @param[in]		source		algorithm used to exchange this key
 * @return	0 on success, error code otherwise
 */
MS2_PUBLIC int ms_media_stream_sessions_set_srtp_inner_recv_key(MSMediaStreamSessions *sessions,
                                                                MSCryptoSuite suite,
                                                                const uint8_t *key,
                                                                size_t key_length,
                                                                MSSrtpKeySource source,
                                                                uint32_t ssrc);

/**
 * Set srtp inner receiver key for the given media stream.
 * This is used for double encryption only (RFC8723)
 * If no outer srtp session exists on the stream returns an error
 *
 * @param[in/out]	sessions	The sessions associated to the current media stream
 * @param[in]		suite		The srtp crypto suite to use
 * @param[in]		key		Srtp master key and master salt in a base 64 NULL terminated string
 * @param[in]		source		algorithm used to exchange this key
 * @return	0 on success, error code otherwise
 */
MS2_PUBLIC int ms_media_stream_sessions_set_srtp_inner_recv_key_b64(
    MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char *key, MSSrtpKeySource source, uint32_t ssrc);

/**
 * Set srtp sender key for the given media stream.
 * If no srtp session exists on the stream it is created, if it already exists srtp policy is created/modified for the
 * sender side of the stream.
 *
 * @param[in/out]	sessions	The sessions associated to the current media stream
 * @param[in]		suite		The srtp crypto suite to use
 * @param[in]		key		Srtp master key and master salt in a base 64 NULL terminated string
 * @param[in]		source		algorithm used to exchange this key
 * @return	0 on success, error code otherwise
 */
MS2_PUBLIC int ms_media_stream_sessions_set_srtp_send_key_b64(MSMediaStreamSessions *sessions,
                                                              MSCryptoSuite suite,
                                                              const char *key,
                                                              MSSrtpKeySource source);

/**
 * Set srtp sender key for the given media stream.
 * If no srtp session exists on the stream it is created, if it already exists srtp policy is created/modified for the
 * sender side of the stream.
 *
 * @param[in/out]	stream		The mediastream to operate on
 * @param[in]		suite		The srtp crypto suite to use
 * @param[in]		key		Srtp master key and master salt
 * @param[in]		key_length	key buffer length
 * @param[in]		stream_type	Srtp suite is applied to RTP stream, RTCP stream or both
 * @param[in]		source		algorithm used to exchange this key
 * @return	0 on success, error code otherwise
 */
MS2_PUBLIC int ms_media_stream_sessions_set_srtp_send_key(MSMediaStreamSessions *sessions,
                                                          MSCryptoSuite suite,
                                                          const uint8_t *key,
                                                          size_t key_length,
                                                          MSSrtpKeySource source);

/**
 * Set srtp inner sender key for the given media stream.
 * This is used for double encryption only (RFC8723)
 * If no outer srtp session exists on the stream returns an error
 *
 * @param[in/out]	stream		The mediastream to operate on
 * @param[in]		suite		The srtp crypto suite to use
 * @param[in]		key		Srtp master key and master salt
 * @param[in]		key_length	key buffer length
 * @param[in]		stream_type	Srtp suite is applied to RTP stream, RTCP stream or both
 * @param[in]		source		algorithm used to exchange this key
 * @return	0 on success, error code otherwise
 */
MS2_PUBLIC int ms_media_stream_sessions_set_srtp_inner_send_key(MSMediaStreamSessions *sessions,
                                                                MSCryptoSuite suite,
                                                                const uint8_t *key,
                                                                size_t key_length,
                                                                MSSrtpKeySource source);

/**
 * Set srtp inner sender key for the given media stream.
 * This is used for double encryption only (RFC8723)
 * If no outer srtp session exists on the stream returns an error
 *
 * @param[in/out]	stream		The mediastream to operate on
 * @param[in]		suite		The srtp crypto suite to use
 * @param[in]		key		Srtp master key and master salt in a base 64 NULL terminated string
 * @param[in]		stream_type	Srtp suite is applied to RTP stream, RTCP stream or both
 * @param[in]		source		algorithm used to exchange this key
 * @return	0 on success, error code otherwise
 */
MS2_PUBLIC int ms_media_stream_sessions_set_srtp_inner_send_key_b64(MSMediaStreamSessions *sessions,
                                                                    MSCryptoSuite suite,
                                                                    const char *key,
                                                                    MSSrtpKeySource source);

/**
 * Set the session EKT operation mode:
 *   - MS_EKT_DISABLED: No EKT, this is the default mode.
 *   - MS_EKT_ENABLED: We are expecting to get a EKT and use it to produce EKT tag on outgoing packet and parse EKT tag
 * on incoming ones
 *   - MS_EKT_TRANSFER: We are a relay unable to decrypt the EKT tag but it will be present at the end of the packet and
 * we need to relay it
 *   - MS_EKT_DISABLED_WITH_TRANSFER: We are a relay and double encryption is on. EKT is not enabled(so no EKT tag at
 * the end of RPT packets) but we must manage the OHB - this mode is used mostly for testing.
 *
 * @param[in/out]	stream		The mediastream to operate on
 * @param[in]		mode		One of disabled, enabled or transfer
 * @return	0 on success, error code otherwise
 */
MS2_PUBLIC int ms_media_stream_sessions_set_ekt_mode(MSMediaStreamSessions *sessions, MSEKTMode mode);

/**
 * Set Encrypted Key transport
 * Once set, sending stream on this session will regenerate a SRTP master key and dispatch it using the given EKT
 * EKT is stored in reception context to decrypt incoming ekt tag (more than one can be used in reception as peers mays
 * not update all together)
 *
 * @param[in/out]	stream		The mediastream to operate on
 * @param[in]		ekt		The parameter set holding all information needed to generate, dispatch and decrypt incoming
 * Srtp master key Data is copied internally and caller can dispose of it at anytime after this call
 * @return	0 on success, error code otherwise
 */
MS2_PUBLIC int ms_media_stream_sessions_set_ekt(MSMediaStreamSessions *sessions, const MSEKTParametersSet *ekt);

/**
 * Convert MSCryptoSuite enum to a string.
 *
 * @param[in]		suite		The srtp crypto suite to use
 * @return	the string corresponding the crypto suite
 */
MS2_PUBLIC const char *ms_crypto_suite_to_string(MSCryptoSuite suite);

/**
 * Free ressources used by SRTP context
 * @param[in/out]	context		the DTLS-SRTP context
 */
MS2_PUBLIC void ms_srtp_context_delete(MSSrtpCtx *session);

#ifdef __cplusplus
}
#endif

#endif /* ms_srtp_h */
