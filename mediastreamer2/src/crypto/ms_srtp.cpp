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

#include <map>
#include <mutex>
#include <vector>

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif
#include "ortp/ortp.h"

#include "bctoolbox/crypto.h"
#include "bctoolbox/crypto.hh"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/ms_srtp.h"

#ifdef HAVE_SRTP

/*srtp defines all this stuff*/
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION

#include "srtp_prefix.h"

#define NULL_SSRC 0

static size_t ms_srtp_get_master_key_size(MSCryptoSuite suite);
static size_t ms_srtp_get_master_salt_size(MSCryptoSuite suite);

struct MSSrtpStreamStats {
	MSSrtpKeySource mSource; /**< who provided the key (SDES, ZRTP, DTLS-SRTP) */
	MSCryptoSuite mSuite;    /**< what crypto suite was set. Is set to MS_CRYPTO_SUITE_INVALID if setting fails */
	MSSrtpStreamStats() : mSource{MSSrtpKeySourceUnavailable}, mSuite{MS_CRYPTO_SUITE_INVALID} {};
};

/**
 * Class to store the encrypted EKT Tag
 */
class EktTagCipherText {
public:
	uint32_t mRoc;                    /**< The ROC used in the plain EKT tag */
	std::vector<uint8_t> mCipherText; /**< The cipher text itself */
	EktTagCipherText(uint32_t roc, std::vector<uint8_t> cipherText) : mRoc{roc}, mCipherText{cipherText} {};
};

/**
 * Class to store all context informations needed by the EKT
 */
class Ekt {
public:
	bctoolbox::AesId mCipherType;   /**< AESKW128 or AESKW256 */
	MSCryptoSuite mSrtpCryptoSuite; /**< The SRTP crypto suite to be used to protect the RTP packets with the key
	                                    encrypted with this EKT */
	std::vector<uint8_t> mKey; /**< The EKTKey that the recipient should use when generating EKTCiphertext values, size
	                             depends on ekt_cipher_type */
	std::vector<uint8_t> mSrtpMasterKey;  /**< The SRTP master key currently in use: note we use the same key when
	                                          bundling sessions when sending */
	std::vector<uint8_t> mSrtpMasterSalt; /**< The SRTP master salt to be used with any master key encrypted with this
	                                          EKT Key, size depends on mSrtpCryptoSuite */
	uint16_t mSpi;                        /**< reference this EKTKey and SRTP master salt */
	uint32_t mTtl;   /**< The maximum amount of time, in seconds, that this EKTKey can be used.(on 24 bits) */
	uint16_t mEpoch; /**< how many SRTP keys have been sent for this SSRC under the current EKTKey, prior to the current
	                   key */
	std::map<uint32_t, std::shared_ptr<EktTagCipherText>>
	    tagCache; /**< maps of ROC and current cipher text in use indexed by SSRC - as a session can bundle several SSRC
	               */

	Ekt(){};
	Ekt(const MSEKTParametersSet *params)
	    : mCipherType{bctoolbox::AesId::AES128}, mSrtpCryptoSuite{params->ekt_srtp_crypto_suite},
	      mKey{std::vector<uint8_t>(ms_srtp_get_master_key_size(mSrtpCryptoSuite))},
	      mSrtpMasterSalt{std::vector<uint8_t>(ms_srtp_get_master_salt_size(mSrtpCryptoSuite))}, mSpi{params->ekt_spi},
	      mTtl{params->ekt_ttl}, mEpoch{0} {
		memcpy(mKey.data(), params->ekt_key_value, mKey.size());
		memcpy(mSrtpMasterSalt.data(), params->ekt_master_salt, mSrtpMasterSalt.size());
		if (params->ekt_cipher_type == MS_EKT_CIPHERTYPE_AESKW256) {
			mCipherType = bctoolbox::AesId::AES256;
		}
	}
	~Ekt() {
		if (!mKey.empty()) {
			bctbx_clean(mKey.data(), mKey.size());
		}
		if (!mSrtpMasterKey.empty()) {
			bctbx_clean(mSrtpMasterKey.data(), mSrtpMasterKey.size());
		}
	}
};

class MSSrtpStreamContext {
public:
	srtp_t mSrtp;
	/* store modifiers in the context just to be able to not append then again if the context is modified */
	RtpTransportModifier *mModifierRtp;
	RtpTransportModifier *mModifierRtcp;
	bool mSecured;
	bool mMandatoryEnabled;
	std::mutex mMutex;
	/* for stats purpose */
	MSSrtpStreamStats mStats;

	/* For double encryption */
	srtp_t mInnerSrtp;
	MSSrtpStreamStats mInnerStats;

	/* For EKT */
	MSEKTMode mEktMode; /**< EKT operation mode: disabled, enabled, transfer */

	MSSrtpStreamContext()
	    : mSrtp{nullptr}, mModifierRtp{nullptr}, mModifierRtcp{nullptr}, mSecured{false}, mMandatoryEnabled{false},
	      mInnerSrtp{nullptr}, mEktMode{MS_EKT_DISABLED} {};
};

class MSSrtpSendStreamContext : public MSSrtpStreamContext {
public:
	std::shared_ptr<Ekt> ektSender; /**< the EKT used by sender */
	MSSrtpSendStreamContext() : ektSender{nullptr} {};
};
class MSSrtpRecvStreamContext : public MSSrtpStreamContext {
public:
	std::map<uint16_t, std::shared_ptr<Ekt>>
	    ektsReceiverPool; /**< a map of EKT used to decrypt incoming EKT tag if needed, indexed by SPI */
	MSSrtpRecvStreamContext(){};
};

struct _MSSrtpCtx {
	bctoolbox::RNG mRNG;           /**< EKT needs a RNG to be able to generate srtp master key */
	MSSrtpSendStreamContext mSend; /**< The context used to protect outgoing packets, is always using any_outbound mode
	                                 so we don't manage SSRC */
	MSSrtpRecvStreamContext mRecv; /**< The contex used to unprotect incoming packets. Outer encryption always use
	                                 any_inbound mode, inner - if present - uses ssrc specific */
};

static int ms_add_srtp_stream(MSSrtpStreamContext *streamCtx,
                              MSCryptoSuite suite,
                              const uint8_t *key,
                              size_t key_length,
                              bool is_send,
                              bool is_inner,
                              uint32_t ssrc);

/***********************************************/
/***** LOCAL FUNCTIONS                     *****/
/***********************************************/
static MSSrtpCtx *ms_srtp_context_new(void) {
	MSSrtpCtx *ctx = new _MSSrtpCtx();

	return ctx;
}

/**** Encrypted Key Transport related functions ****/
static size_t ms_srtp_ekt_get_tag_size(std::shared_ptr<Ekt> ekt) {
	// TODO: implement a mecanism to tell if we must send a long or short tag, for now always long
	// RFC 8870 section 4.1: tag is EKTCipherText + 7 bytes trailer(SPI, Epoch, Length, terminal byte)
	// EKTPlain is SRTPMasterKeyLength(1 byte) SRTPMasterKey(depends on crypto suite) SSRC(4 bytes) ROC(4 bytes)
	size_t EKTPlain_size = 1 + ms_srtp_get_master_key_size(ekt->mSrtpCryptoSuite) + 4 + 4;
	// EKTCipher size, using AESKeyWrap 128 or 256 is : round up the plaintext size to a multiple of 8 + 8
	size_t EKTCipher_size = EKTPlain_size + ((EKTPlain_size % 8 == 0) ? 0 : (8 - (EKTPlain_size % 8))) + 8;
	return EKTCipher_size + 7;
}

/**
 * manage EKT tag on reception
 * Parse and decrypt EKT tag, if it holds a new key, create the srtp session for it
 * @param[in]		t	the transport modifier, get access to the SRTP context from user data and transport session
 * @param[in/out]	m	the incoming message expected to hold an EKT tag
 * @param[in/out]	slen	size of the incoming message(is ajusted by this function to prune the EKT tag)
 * @return true on success, false otherwise
 */
static bool ms_srtp_process_ekt_on_receive(RtpTransportModifier *t, mblk_t *m, int *slen) {
	MSSrtpRecvStreamContext *ctx = (MSSrtpRecvStreamContext *)t->data;
	if (ctx->ektsReceiverPool.empty()) {
		ms_warning("EKT enabled but we were given no keys, drop packet");
		return false;
	}

	// Short EKT tag, just remove it
	if (m->b_rptr[*slen - 1] == 0x00) {
		*slen -= 1;
		return true;
	}

	// Check it is a Full EKT Tag
	if (m->b_rptr[*slen - 1] != 0x02) {
		ms_error("SRTP is expecting an EKT tag but message type is invalid : 0x%x", m->b_rptr[*slen - 1]);
		return false;
	}

	// parse the Full EKT Tag
	*slen -= 3;
	size_t ekt_tag_size = (((uint16_t)m->b_rptr[*slen]) << 8) | ((uint16_t)m->b_rptr[*slen + 1]);
	// skip epoch: not supported for now
	*slen -= 4;
	uint16_t spi = (((uint16_t)m->b_rptr[*slen]) << 8) | ((uint16_t)m->b_rptr[*slen + 1]);

	*slen -= (int)(ekt_tag_size)-7; // b_ptr+slen points at the begining of the EKTtag

	// Do we have this EKT?
	auto search = ctx->ektsReceiverPool.find(spi);
	if (search != ctx->ektsReceiverPool.end()) {
		auto ekt = search->second;
		uint32_t ssrc = rtp_header_get_ssrc((rtp_header_t *)m->b_rptr);
		// If we do not have any cipher text matching this SSRC or the received EKTtag different than
		// one we already have
		if ((ekt->tagCache.count(ssrc) == 0) || (ekt->tagCache[ssrc]->mCipherText.size() != ekt_tag_size) ||
		    (memcmp(ekt->tagCache[ssrc]->mCipherText.data(), m->b_rptr + *slen, ekt_tag_size) != 0)) {
			// This is a new EKTtag
			std::vector<uint8_t> cipherText{m->b_rptr + *slen, m->b_rptr + *slen + ekt_tag_size};

			// Decrypt it
			std::vector<uint8_t> plainText{};
			plainText.reserve(cipherText.size() - 8);
			if (AES_key_unwrap(cipherText, ekt->mKey, plainText, ekt->mCipherType) != 0) {
				ms_error("SRTP stream [%p] unable to decryt EKT tag with SPI %02x. Drop the packet", ctx, spi);
				return false;
			}

			// Parse the EKT tag: Key Length(1 byte), key, SSRC(4 bytes), ROC(4 bytes)
			size_t index = 0;
			size_t srtp_master_key_size = plainText[index++];
			std::vector<uint8_t> srtp_master_key(plainText.cbegin() + 1, plainText.cbegin() + 1 + srtp_master_key_size);
			index += srtp_master_key_size;
			uint32_t ekt_ssrc = ((uint32_t)plainText[index]) << 24 | ((uint32_t)plainText[index + 1]) << 16 |
			                    ((uint32_t)plainText[index + 2]) << 8 | ((uint32_t)plainText[index + 3]);
			index += 4;
			uint32_t roc = ((uint32_t)plainText[index]) << 24 | ((uint32_t)plainText[index + 1]) << 16 |
			               ((uint32_t)plainText[index + 2]) << 8 | ((uint32_t)plainText[index + 3]);
			bctbx_clean(plainText.data(), plainText.size());

			// Check SSRC
			if (ssrc != ekt_ssrc) {
				ms_error("EKT incoming: EKTTag with SPI %02x  get an SSRC(%04x) and Packet SSRC(%04x) "
				         "differs. Drop the packet",
				         spi, ekt_ssrc, ssrc);
				return false;
			}

			// Insert the key and ROC in the srtp context
			srtp_master_key.insert(srtp_master_key.end(), ekt->mSrtpMasterSalt.cbegin(), ekt->mSrtpMasterSalt.cend());
			if (ctx->mInnerSrtp == NULL) {
				std::unique_lock<std::mutex> lock(ctx->mMutex);
				auto err = srtp_create(&ctx->mInnerSrtp, NULL);
				if (err != srtp_err_status_ok) {
					ms_error("Failed to create inner srtp session (%d) for srtp stream [%p] upon "
					         "reception of a new EKT tag with spi %02x, drop the packet",
					         err, ctx, spi);
					return false;
				}
			}

			if (ms_add_srtp_stream(ctx, ekt->mSrtpCryptoSuite, srtp_master_key.data(), srtp_master_key.size(), false,
			                       true, ssrc) != 0) {
				ms_error("SRTP stream [%p] unable to add EKT tag retrieved SRTP master key in "
				         "reception on SSRC %04x. Drop the packet",
				         ctx, ssrc);
				return false;
			} else {
				ms_message("media_stream_set_srtp_inner_recv_key on EKT Tag reception: key %02x..%02x (ssrc %x, ROC "
				           "%d) stream sessions is [%p]",
				           srtp_master_key.front(), srtp_master_key.back(), ssrc, roc, t->session);
			}

			auto ret = srtp_set_stream_roc(ctx->mInnerSrtp, ssrc, roc);
			if (ret != err_status_ok) {
				ms_error("SRTP stream [%p] unable to set ROC from EKT tag in reception on SSRC %04x. "
				         "Drop the packet",
				         ctx, ssrc);
				return false;
			}
			if (ctx->mInnerStats.mSource != MSSrtpKeySourceEKT || ctx->mInnerStats.mSuite != ekt->mSrtpCryptoSuite) {
				ctx->mInnerStats.mSource = MSSrtpKeySourceEKT;
				ctx->mInnerStats.mSuite = ekt->mSrtpCryptoSuite;
				/* Srtp encryption has changed, notify to get it in call stats */
				OrtpEvent *ev = ortp_event_new(ORTP_EVENT_SRTP_ENCRYPTION_CHANGED);
				OrtpEventData *eventData = ortp_event_get_data(ev);
				eventData->info.srtp_info.is_send = FALSE;
				eventData->info.srtp_info.is_inner = TRUE;
				eventData->info.srtp_info.source = MSSrtpKeySourceEKT;
				eventData->info.srtp_info.suite = ekt->mSrtpCryptoSuite;
				rtp_session_dispatch_event(t->session, ev);
			}

			// Store the tag in cache
			ekt->tagCache.emplace(ssrc, std::make_shared<EktTagCipherText>(roc, cipherText));
		}
		return true;
	} else {
		ms_warning("Receive EKT tag but we do not have the key to decrypt it(spi %02x), drop packet", spi);
		return false;
	}
}

/**
 * set EKT tag on outgoing packets
 * Generate, encrypt and append the EKT tag to the outgoing packets if needed
 * Storage for the EKT tag is already allocated at the end of the packet, just write it
 * @param[in]		ctx	the SRTP context for outgoing packets
 * @param[in/out]	m	the outgoing message to append the EKT tag
 * @param[in/out]	slen	size of the outgoign message(is ajusted by this function to add the EKT tag)
 * @param[in]		ekt_tag_size	the size of the ekt tag to be produced
 * @return true on success, false otherwise
 */
static bool ms_srtp_set_ekt_tag(MSSrtpSendStreamContext *ctx, mblk_t *m, int *slen, size_t ekt_tag_size) {
	if (ctx->ektSender != nullptr) {
		// mecanism to decide if we use short or long EKT tag is implemented in ms_srtp_ekt_get_tag_size.
		// When this function is called, based on the given ekt_tag_size we can determine what to do:
		// - 0 this is not supposed to happend, return an error: if we have a ektSender context
		//   we must append an EKT tag
		// - 1 add a short EKT tag
		// - more than 1, add a long EKT tag
		if (ekt_tag_size == 0) {
			ms_error("SRTP stream [%p] sending packet asked to set an ekt tag of length 0", ctx);
			return false;
		}

		if (ekt_tag_size == 1) { // Short EKT tag
			m->b_rptr[*slen] = 0;
			*slen += 1;
			return true;
		}

		// Long EKT Tag
		// Get SSRC from the packet header and not the session as we can bundle several sessions
		uint32_t ssrc = rtp_header_get_ssrc((rtp_header_t *)m->b_rptr);
		uint32_t roc = 0;
		auto ret = srtp_get_stream_roc(ctx->mInnerSrtp, ssrc, &roc);
		if (ret != err_status_ok) {
			ms_error("Unable to retrieve ROC when creating EKT plain text");
			return false;
		}
		// Shall we compute the EKT Tag?
		bool createTag = false;
		bool replaceTag = false;
		if (ctx->ektSender->tagCache.count(ssrc) == 0) {
			// We do not have any cipher text for this SSRC
			createTag = true;
		} else {
			// Check the current ROC is still the one we used to generate the tag
			if (roc != ctx->ektSender->tagCache[ssrc]->mRoc) {
				replaceTag = true;
			}
		}

		if (createTag || replaceTag) {
			auto ekt = ctx->ektSender;
			// We must create the cipher text
			// Plain text is: SRTPMasterKeyLength(1 byte) SRTPMasterKey SSRC ROC
			std::vector<uint8_t> plainText{static_cast<uint8_t>(ms_srtp_get_master_key_size(ekt->mSrtpCryptoSuite))};
			plainText.reserve(ms_srtp_get_master_key_size(ekt->mSrtpCryptoSuite) + 9);
			plainText.insert(plainText.end(), ekt->mSrtpMasterKey.cbegin(), ekt->mSrtpMasterKey.cend());

			plainText.push_back(static_cast<uint8_t>((ssrc >> 24) & 0xFF));
			plainText.push_back(static_cast<uint8_t>((ssrc >> 16) & 0xFF));
			plainText.push_back(static_cast<uint8_t>((ssrc >> 8) & 0xFF));
			plainText.push_back(static_cast<uint8_t>((ssrc)&0xFF));

			plainText.push_back(static_cast<uint8_t>((roc >> 24) & 0xFF));
			plainText.push_back(static_cast<uint8_t>((roc >> 16) & 0xFF));
			plainText.push_back(static_cast<uint8_t>((roc >> 8) & 0xFF));
			plainText.push_back(static_cast<uint8_t>((roc)&0xFF));

			// encrypt it
			std::vector<uint8_t> cipherText{};
			cipherText.reserve(ekt_tag_size);
			AES_key_wrap(plainText, ekt->mKey, cipherText, ekt->mCipherType);
			bctbx_clean(plainText.data(), plainText.size());

			// append SPI
			cipherText.push_back(static_cast<uint8_t>((ekt->mSpi >> 8) & 0xFF));
			cipherText.push_back(static_cast<uint8_t>((ekt->mSpi) & 0xFF));

			// append epoch
			cipherText.push_back(static_cast<uint8_t>((ekt->mEpoch >> 8) & 0xFF));
			cipherText.push_back(static_cast<uint8_t>((ekt->mEpoch) & 0xFF));

			// append Length: in bytes, including length and message type byte
			cipherText.push_back(static_cast<uint8_t>((ekt_tag_size >> 8) & 0xFF));
			cipherText.push_back(static_cast<uint8_t>((ekt_tag_size)&0xFF));

			// Full EKT tag message type : 0x02
			cipherText.push_back(0x02);
			if (createTag) {
				ekt->tagCache.emplace(ssrc, std::make_shared<EktTagCipherText>(roc, cipherText));
			} else {
				ekt->tagCache[ssrc] = std::make_shared<EktTagCipherText>(roc, cipherText);
			}
		}

		memcpy(m->b_rptr + *slen, ctx->ektSender->tagCache[ssrc]->mCipherText.data(), ekt_tag_size);
		*slen += (int)(ekt_tag_size);
	} else {                          // We were expecting an EKT but don't have it to create the EKT tag
		if (ctx->mMandatoryEnabled) { // drop the packet if encryption is mandatory
			return 0;                 /* drop the packet */
		}
	}
	return true;
}

/**** Enf of Encrypted Key Transport related functions ****/

static void check_and_create_srtp_context(MSMediaStreamSessions *sessions) {
	if (!sessions->srtp_context) {
		sessions->srtp_context = ms_srtp_context_new();
	}
}
/**** Sender functions ****/
static int ms_srtp_process_on_send(RtpTransportModifier *t, mblk_t *m) {
	int slen;
	MSSrtpSendStreamContext *ctx = (MSSrtpSendStreamContext *)t->data;
	err_status_t err;
	rtp_header_t *rtp_header = (rtp_header_t *)m->b_rptr;
	slen = (int)msgdsize(m);

	if (rtp_header && (slen > RTP_FIXED_HEADER_SIZE && rtp_header->version == 2)) {
		size_t ekt_tag_size = 0;
		std::vector<uint8_t> ekt_tag{};
		std::unique_lock<std::mutex> lock(ctx->mMutex);
		if (ctx->mStats.mSuite == MS_CRYPTO_SUITE_INVALID) { // No srtp is set up
			if (ctx->mMandatoryEnabled) {
				return 0; /* drop the packet */
			} else {
				return slen; /* pass it uncrypted */
			}
		}

		// EKT preparation (possible tag append at the end of encryption processing)
		if (ctx->mEktMode == MS_EKT_ENABLED && ctx->ektSender != nullptr) {
			ekt_tag_size = ms_srtp_ekt_get_tag_size(ctx->ektSender);
		} else if (ctx->mEktMode == MS_EKT_TRANSFER) {
			// We are in transfer mode: the EktTag shall already be at the end of the packet
			// copy it in a buffer to be able to append it again after the srtp_protect call
			msgpullup(m, -1); // This should be useless(and thus harmless) as in transfer mode the message shall not be
			                  // fragmented, but just in case
			if (m->b_rptr[slen - 1] == 0x00) { // Short EKT tag
				ekt_tag_size = 1;
				ekt_tag.assign({0x00});
				slen--;
			} else if (m->b_rptr[slen - 1] == 0x02) { // Full EKT tag
				ekt_tag_size = ((uint16_t)(m->b_rptr[slen - 3])) << 8 | (uint16_t)(m->b_rptr[slen - 2]);
				ekt_tag.assign(m->b_rptr + slen - ekt_tag_size, m->b_rptr + slen);
				slen -= (int)(ekt_tag_size); // Hide the trailing ekt tag from the SRTP engine
			} else {
				ms_error("SRTP stream [%p] sending packet in transfer mode expecting EKT tag but none were found, type "
				         "is %x",
				         ctx, m->b_rptr[slen - 1]);
			}
		}

		/* We do have an inner srtp context, double encryption is on.
		 */
		if (ctx->mInnerSrtp) {
			/* RFC 8723:
			 * 1 - get the header without extension + payload and encrypt that with the inner encryption
			 * 2 - put back the orginal header with extensions
			 * 3 - append the OHB byte to 0x00 after the auth tag
			 * 4 - pass it to outer encryption */
			/* defragment message and enlarge the buffer for srtp to write its data */
			msgpullup(m, slen + 2 * SRTP_MAX_TRAILER_LEN + 5 +
			                 ekt_tag_size); // +4 for 32 bits alignment + 1 byte for OHB set to 0x00

			if (rtp_get_extbit(m) != 0) { /* There is an extension header */
				uint8_t *payload = NULL;
				uint16_t cc = rtp_get_cc(m);
				int payload_size = rtp_get_payload(m, &payload);
				if (payload_size < 0) {
					ms_warning("srtp_protect inner encryption failed (unable to get payload) for stream ctx [%p]", ctx);
					return -1;
				}
				/* create a synthetic packet, it holds:
				 * RTP header + size of CSRC if any - No extensions
				 * payload
				 * reserve space for SRTP trailer */
				size_t synthetic_header_size = RTP_FIXED_HEADER_SIZE + 4 * cc;
				size_t synthetic_size = synthetic_header_size + (size_t)payload_size + SRTP_MAX_TRAILER_LEN + 4;
				synthetic_size += 4 - synthetic_size % 4; /* make sure the memory is 32 bits aligned for srtp */
				uint8_t *synthetic = (uint8_t *)ms_malloc0(synthetic_size);
				memcpy(synthetic, m->b_rptr, synthetic_header_size);              /* copy header */
				((rtp_header_t *)(synthetic))->extbit = 0;                        /* force the ext bit to 0 */
				memcpy(synthetic + synthetic_header_size, payload, payload_size); /* append payload */

				/* encrypt the synthetic packet */
				int synthetic_len = (int)(synthetic_header_size + payload_size);
				err = srtp_protect(ctx->mInnerSrtp, synthetic, &synthetic_len);
				if (err != err_status_ok) {
					ms_warning("srtp_protect inner encryption failed (%d) for stream ctx [%p]", err, ctx);
					ms_free(synthetic);
					return -1;
				}

				/* put it back in the original one */
				memcpy(payload, synthetic + synthetic_header_size, synthetic_len - synthetic_header_size);
				ms_free(synthetic);
				slen += (int)(synthetic_len - synthetic_header_size -
				              payload_size); /* slen is header + payload -> set it with new payload size: (synthetic_len
				                                - synthetic_header_size) instead of the original payload_size  */

			} else { /* no extension header, we can directly proceed to inner encryption */
				err = srtp_protect(ctx->mInnerSrtp, m->b_rptr, &slen);
				if (err != err_status_ok) {
					ms_warning("srtp_protect inner encryption failed (%d) for stream ctx [%p]", err, ctx);
					return -1;
				}
			}
			m->b_rptr[slen] = 0x00; /* the emtpy OHB */
			slen++;
		} else {
			/* defragment message and enlarge the buffer for srtp to write its data */
			msgpullup(m, slen + SRTP_MAX_TRAILER_LEN + ekt_tag_size + 4); /*+4 for 32 bits alignment*/
			;
		}

		err = srtp_protect(ctx->mSrtp, m->b_rptr, &slen);

		// Append the EKT tag
		if (ctx->mEktMode == MS_EKT_ENABLED) {
			if (!ms_srtp_set_ekt_tag(ctx, m, &slen, ekt_tag_size)) {
				// Problem when trying to set the EKT tag, drop the packet
				return 0;
			}
		} else if (ctx->mEktMode ==
		           MS_EKT_TRANSFER) { // We are in transfer mode: put back the EKT tag at the end of the packet
			if (ekt_tag_size > 0) {
				memcpy(m->b_rptr + slen, ekt_tag.data(), ekt_tag_size);
				slen += (int)(ekt_tag_size);
			}
		}
	} else {
		/*ignoring non rtp/rtcp packets*/
		return slen;
	}

	/* check return code from srtp_protect */
	if (err == err_status_ok) {
		return slen;
	}
	ms_warning("srtp_protect failed (%d) for stream ctx [%p]", err, ctx);
	return -1;
}

static int ms_srtcp_process_on_send(RtpTransportModifier *t, mblk_t *m) {
	int slen;
	err_status_t err;
	rtcp_common_header_t *rtcp_header = (rtcp_common_header_t *)m->b_rptr;
	slen = (int)msgdsize(m);
	MSSrtpSendStreamContext *ctx = (MSSrtpSendStreamContext *)t->data;

	// ignore non rtcp packets
	if (rtcp_header && (slen > RTP_FIXED_HEADER_SIZE && rtcp_header->version == 2)) {
		std::unique_lock<std::mutex> lock(ctx->mMutex);
		if (ctx->mStats.mSuite == MS_CRYPTO_SUITE_INVALID) { // No srtp is set up
			err = err_status_ok;
			if (ctx->mMandatoryEnabled) {
				return 0; /*droping packets*/
			}
		} else {
			/* defragment incoming message and enlarge the buffer for srtp to write its data */
			msgpullup(m,
			          slen + SRTP_MAX_TRAILER_LEN + 4 /*for 32 bits alignment*/ + 4 /*required by srtp_protect_rtcp*/);
			err = srtp_protect_rtcp(ctx->mSrtp, m->b_rptr, &slen);
			if (err != err_status_ok) {
				ms_warning("srtp_protect_rtcp failed (%d) for stream ctx [%p]", err, ctx);
				return -1;
			}
		}
	}

	return slen;
}

static int ms_srtp_process_dummy(RtpTransportModifier *t, mblk_t *m) {
	return (int)msgdsize(m);
}

static int ms_srtp_process_on_receive(RtpTransportModifier *t, mblk_t *m) {
	int slen = (int)msgdsize(m);
	err_status_t srtp_err = err_status_ok;

	/* Check incoming message seems to be a valid RTP */
	rtp_header_t *rtp = (rtp_header_t *)m->b_rptr;
	if (slen < RTP_FIXED_HEADER_SIZE || rtp->version != 2) {
		return slen;
	}

	MSSrtpRecvStreamContext *ctx = (MSSrtpRecvStreamContext *)t->data;
	/* Shall we check the EKT ? */
	std::vector<uint8_t> ekt_tag{};
	if (ctx->mEktMode == MS_EKT_ENABLED) {
		if (!ms_srtp_process_ekt_on_receive(t, m, &slen)) {
			return 0; // Error during ekt tag processing, drop the packet
		}
	} else if (ctx->mEktMode == MS_EKT_TRANSFER) {
		// In transfer mode, we shall save the EktTag in a temp buffer to restore it after the srtp unprotect
		if (m->b_rptr[slen - 1] == 0x00) { // Short EKT tag
			ekt_tag.assign({0x00});
			slen--;
		} else if (m->b_rptr[slen - 1] == 0x02) { // Full EKT tag
			size_t ekt_tag_size = ((uint16_t)(m->b_rptr[slen - 3])) << 8 | (uint16_t)(m->b_rptr[slen - 2]);
			ekt_tag.assign(m->b_rptr + slen - ekt_tag_size, m->b_rptr + slen);
			slen -= (int)(ekt_tag_size); // Hide the trailing ekt tag from the SRTP engine
		} else {
			ms_error("SRTP stream [%p] receiving packet in transfer mode expecting EKT tag but none were found, "
			         "type is %x",
			         ctx, m->b_rptr[slen - 1]);
		}
	}

	if (ctx->mStats.mSuite == MS_CRYPTO_SUITE_INVALID) {
		if (ctx->mMandatoryEnabled) {
			return 0; /* drop message: we cannot decrypt but encryption is mandatory */
		} else {
			return slen; /* just pass it */
		}
	}

	if ((srtp_err = srtp_unprotect(ctx->mSrtp, m->b_rptr, &slen)) != err_status_ok) {
		ms_warning("srtp_unprotect_rtp failed (%d) on stream ctx [%p]", srtp_err, ctx);
		return -1;
	}

	/* Do we have double encryption */
	if (ctx->mInnerSrtp != NULL) {
		/* RFC8723: now that we applied outer crypto algo, we must
		 * 1 - get the OHB: if it is not 0 replace the headers
		 * 2 - if we have extensions remove them
		 * 3 - decrypt and put the extensions back */
		if (m->b_rptr[slen - 1] != 0) { /* there is some OHB bits sets, restore it */
			// For now we have no reason to support the PT, M or SeqNum modification
			// So this is not implemented but leads to an error message and packet discarded
			// Note: this may happend in case of error in the outer layer decryption - the auth tag should prevent
			// it but...
			ms_error("A double encrypted packet seem to have a non null OHB - see RFC8723 section 4 - section. "
			         "This is not supported yet - discard the packet.");
			return 0;
		}
		slen--;                       /* drop the OHB Config byte */
		if (rtp_get_extbit(m) != 0) { /* There is an extension header */
			uint8_t *payload = NULL;
			uint16_t cc = rtp_get_cc(m);
			int extsize = rtp_get_extheader(m, NULL, NULL) +
			              4; // get_extheader returns the size of the ext header itself, add 4 bytes for the size
			                 // itself as we need the actual size occupied by ext header
			int payload_size = rtp_get_payload(m, &payload); /* The payload size returned is incorrect as it
			                                                    includes the outer encryption auth tag and OHB */
			payload_size =
			    slen - (RTP_FIXED_HEADER_SIZE + 4 * cc + extsize); /* slen is the size of header(with ext) + payload */
			/* create a synthetic packet, it holds:
			 * RTP header + size of CSRC if any - No extensions
			 * payload (includes inner SRTP auth tag) */
			size_t synthetic_header_size = RTP_FIXED_HEADER_SIZE + 4 * cc;
			int synthetic_len = (int)synthetic_header_size + payload_size;
			uint8_t *synthetic = (uint8_t *)ms_malloc0(synthetic_len);
			memcpy(synthetic, m->b_rptr, synthetic_header_size);              /* copy header */
			((rtp_header_t *)(synthetic))->extbit = 0;                        /* force the ext bit to 0 */
			memcpy(synthetic + synthetic_header_size, payload, payload_size); /* append payload */

			/* decrypt the synthetic packet */
			srtp_err = srtp_unprotect(ctx->mInnerSrtp, synthetic, &synthetic_len);
			if (srtp_err != err_status_ok) {
				ms_warning("srtp_unprotect_rtp inner encryption failed (%d) for stream ctx [%p]", srtp_err, ctx);
				ms_free(synthetic);
				return -1;
			}

			/* put it back in the original packet */
			memcpy(payload, synthetic + synthetic_header_size, synthetic_len - synthetic_header_size);
			ms_free(synthetic);
			slen = (int)(RTP_FIXED_HEADER_SIZE + 4 * cc + extsize  /* original header size */
			             + synthetic_len - synthetic_header_size); /* current payload size (after decrypt) */
		} else {
			/* no extension header, decrypt directly */
			srtp_err = srtp_unprotect(ctx->mInnerSrtp, m->b_rptr, &slen);
		}
	} else if (ctx->mEktMode == MS_EKT_TRANSFER) { // We shall not be in transfer mode and have inner srtp context
		// Restore the Ekt tag after the decrypted packet
		if (!ekt_tag.empty()) {
			memcpy(m->b_rptr + slen, ekt_tag.data(),
			       ekt_tag.size()); // No need to worry with pullups as we are just putting back what we removed,
			                        // buffer is already allocated for that
			slen += (static_cast<int>(ekt_tag.size()));
		}
	}

	if (srtp_err == err_status_ok) {
		return slen;
	} else {
		ms_warning("srtp_unprotect failed (%d) on stream ctx [%p]", srtp_err, ctx);
		return -1;
	}
}

static int ms_srtcp_process_on_receive(RtpTransportModifier *t, mblk_t *m) {
	int slen = (int)msgdsize(m);
	err_status_t err = err_status_ok;

	/* Check incoming message seems to be a valid RTCP */
	rtcp_common_header_t *rtcp = (rtcp_common_header_t *)m->b_rptr;
	if (slen < (int)(sizeof(rtcp_common_header_t) + 4) || rtcp->version != 2) {
		return slen;
	}

	MSSrtpRecvStreamContext *ctx = (MSSrtpRecvStreamContext *)t->data;
	if (ctx->mStats.mSuite == MS_CRYPTO_SUITE_INVALID) {
		if (ctx->mMandatoryEnabled) {
			return 0; /* drop message: we cannot decrypt but encryption is mandatory */
		} else {
			return slen; /* just pass it */
		}
	}

	err = srtp_unprotect_rtcp(ctx->mSrtp, m->b_rptr, &slen);
	if (err != err_status_ok) {
		ms_warning("srtp_unprotect_rtcp failed (%d) on stream ctx [%p]", err, ctx);
		return -1;
	}

	return slen;
}

static size_t ms_srtp_get_master_key_size(MSCryptoSuite suite) {
	switch (suite) {
		case MS_AES_128_SHA1_80:
		case MS_AES_128_SHA1_80_NO_AUTH:
		case MS_AES_128_SHA1_80_SRTP_NO_CIPHER:
		case MS_AES_128_SHA1_80_SRTCP_NO_CIPHER:
		case MS_AES_128_SHA1_80_NO_CIPHER:
		case MS_AES_128_SHA1_32:
		case MS_AES_128_SHA1_32_NO_AUTH:
		case MS_AEAD_AES_128_GCM:
			return SRTP_AES_128_KEY_LEN;
		case MS_AES_256_SHA1_80:
		case MS_AES_CM_256_SHA1_80:
		case MS_AES_256_SHA1_32:
		case MS_AEAD_AES_256_GCM:
			return SRTP_AES_256_KEY_LEN;
		case MS_CRYPTO_SUITE_INVALID:
		default:
			return 0;
			break;
	}
}

static size_t ms_srtp_get_master_salt_size(MSCryptoSuite suite) {
	switch (suite) {
		case MS_AES_128_SHA1_80:
		case MS_AES_128_SHA1_80_NO_AUTH:
		case MS_AES_128_SHA1_80_SRTP_NO_CIPHER:
		case MS_AES_128_SHA1_80_SRTCP_NO_CIPHER:
		case MS_AES_128_SHA1_80_NO_CIPHER:
		case MS_AES_128_SHA1_32:
		case MS_AES_128_SHA1_32_NO_AUTH:
		case MS_AES_256_SHA1_80:
		case MS_AES_CM_256_SHA1_80:
		case MS_AES_256_SHA1_32:
			return SRTP_SALT_LEN;
		case MS_AEAD_AES_128_GCM:
		case MS_AEAD_AES_256_GCM:
			return SRTP_AEAD_SALT_LEN;
		case MS_CRYPTO_SUITE_INVALID:
		default:
			return 0;
			break;
	}
}

/**** Session management functions ****/

/**
 * deallocate transport modifier ressources
 * @param[in/out] tp	The transport modifier to be deallocated
 */
static void ms_srtp_transport_modifier_destroy(RtpTransportModifier *tp) {
	ms_free(tp);
}

static MSSrtpStreamContext *get_stream_context(MSMediaStreamSessions *sessions, bool is_send) {
	if (is_send) {
		return &sessions->srtp_context->mSend;
	} else {
		return &sessions->srtp_context->mRecv;
	}
}

static int ms_media_stream_session_fill_srtp_context(MSMediaStreamSessions *sessions, bool is_send, bool is_inner) {
	err_status_t err = srtp_err_status_ok;
	RtpTransport *transport_rtp = NULL, *transport_rtcp = NULL;
	MSSrtpStreamContext *streamCtx = get_stream_context(sessions, is_send);

	rtp_session_get_transports(sessions->rtp_session, &transport_rtp, &transport_rtcp);

	std::unique_lock<std::mutex> lock(streamCtx->mMutex);

	if (is_inner) { /* inner srtp context just need to setup itself, not the modifier*/
		if (streamCtx->mInnerSrtp && is_send) {
			/*we cannot reuse inner srtp context in output as it is in ssrc_any_outbound mode, so freeing first*/
			/* for incoming inner context, we use specific ssrc mode so we have one context */
			srtp_dealloc(streamCtx->mInnerSrtp);
			streamCtx->mInnerSrtp = NULL;
		}

		if (streamCtx->mInnerSrtp == NULL) {
			err = srtp_create(&streamCtx->mInnerSrtp, NULL);
			if (err != srtp_err_status_ok) {
				ms_error("Failed to create inner srtp session (%d) for stream sessions [%p]", err, sessions);
				goto end;
			}
		}
	} else { /* this is outer srtp context, setup srtp and modifier */
		if (streamCtx->mSrtp && streamCtx->mSecured) {
			/*we cannot reuse srtp context, so freeing first*/
			srtp_dealloc(streamCtx->mSrtp);
			streamCtx->mSrtp = NULL;
		}

		if (!streamCtx->mSrtp) {
			err = srtp_create(&streamCtx->mSrtp, NULL);
			if (err != srtp_err_status_ok) {
				ms_error("Failed to create srtp session (%d) for stream sessions [%p]", err, sessions);
				goto end;
			}
		}

		if (!streamCtx->mModifierRtp) {
			streamCtx->mModifierRtp = ms_new0(RtpTransportModifier, 1);
			streamCtx->mModifierRtp->data = streamCtx;
			streamCtx->mModifierRtp->t_process_on_send = is_send ? ms_srtp_process_on_send : ms_srtp_process_dummy;
			streamCtx->mModifierRtp->t_process_on_receive =
			    is_send ? ms_srtp_process_dummy : ms_srtp_process_on_receive;
			streamCtx->mModifierRtp->t_destroy = ms_srtp_transport_modifier_destroy;
			meta_rtp_transport_append_modifier(transport_rtp, streamCtx->mModifierRtp);
		}

		if (!streamCtx->mModifierRtcp) {
			streamCtx->mModifierRtcp = ms_new0(RtpTransportModifier, 1);
			streamCtx->mModifierRtcp->data = streamCtx;
			streamCtx->mModifierRtcp->t_process_on_send = is_send ? ms_srtcp_process_on_send : ms_srtp_process_dummy;
			streamCtx->mModifierRtcp->t_process_on_receive =
			    is_send ? ms_srtp_process_dummy : ms_srtcp_process_on_receive;
			streamCtx->mModifierRtcp->t_destroy = ms_srtp_transport_modifier_destroy;
			meta_rtp_transport_append_modifier(transport_rtcp, streamCtx->mModifierRtcp);
		}
	}
end:
	return err;
}

static int ms_media_stream_sessions_fill_srtp_context_all_stream(struct _MSMediaStreamSessions *sessions) {
	int err = -1;
	/*check if exist before filling*/

	if (!(get_stream_context(sessions, true)->mSrtp) &&
	    (err = ms_media_stream_session_fill_srtp_context(sessions, true, false)))
		return err;

	if (!get_stream_context(sessions, false)->mSrtp)
		err = ms_media_stream_session_fill_srtp_context(sessions, false, false);

	return err;
}

static int ms_set_srtp_crypto_policy(MSCryptoSuite suite, crypto_policy_t *policy, bool is_rtp) {
	switch (suite) {
		case MS_AES_128_SHA1_32:
			// srtp doc says: not adapted to rtcp...
			crypto_policy_set_aes_cm_128_hmac_sha1_32(policy);
			break;
		case MS_AES_128_SHA1_80_NO_AUTH:
		case MS_AES_128_SHA1_32_NO_AUTH:
			// srtp doc says: not adapted to rtcp...
			crypto_policy_set_aes_cm_128_null_auth(policy);
			break;
		case MS_AES_128_SHA1_80_SRTP_NO_CIPHER:
			// SRTP is not encrypted
			// SRTCP is encrypted
			(is_rtp) ? crypto_policy_set_null_cipher_hmac_sha1_80(policy)
			         : crypto_policy_set_aes_cm_128_hmac_sha1_80(policy);
			break;
		case MS_AES_128_SHA1_80_SRTCP_NO_CIPHER:
			// SRTP is encrypted
			// SRTCP is not encrypted
			(is_rtp) ? crypto_policy_set_aes_cm_128_hmac_sha1_80(policy)
			         : crypto_policy_set_null_cipher_hmac_sha1_80(policy);
			break;
		case MS_AES_128_SHA1_80_NO_CIPHER:
			// SRTP is not encrypted
			// SRTCP is not encrypted
			crypto_policy_set_null_cipher_hmac_sha1_80(policy);
			break;
		case MS_AES_128_SHA1_80: /*default mode*/
			crypto_policy_set_aes_cm_128_hmac_sha1_80(policy);
			break;
		case MS_AES_256_SHA1_80: // For backward compatibility
		case MS_AES_CM_256_SHA1_80:
			crypto_policy_set_aes_cm_256_hmac_sha1_80(policy);
			break;
		case MS_AES_256_SHA1_32:
			crypto_policy_set_aes_cm_256_hmac_sha1_32(policy);
			break;
		case MS_AEAD_AES_128_GCM:
			srtp_crypto_policy_set_aes_gcm_128_16_auth(policy);
			break;
		case MS_AEAD_AES_256_GCM:
			srtp_crypto_policy_set_aes_gcm_256_16_auth(policy);
			break;
		case MS_CRYPTO_SUITE_INVALID:
		default:
			return -1;
			break;
	}
	return 0;
}

static bool ms_srtp_is_crypto_policy_secure(MSCryptoSuite suite) {
	switch (suite) {
		case MS_AES_128_SHA1_32:
		case MS_AES_128_SHA1_80_NO_AUTH:
		case MS_AES_128_SHA1_32_NO_AUTH:
		case MS_AES_128_SHA1_80:
		case MS_AES_256_SHA1_80:
		case MS_AES_CM_256_SHA1_80:
		case MS_AES_256_SHA1_32:
		case MS_AEAD_AES_128_GCM:
		case MS_AEAD_AES_256_GCM:
			return true;
		case MS_AES_128_SHA1_80_SRTP_NO_CIPHER:
		case MS_AES_128_SHA1_80_SRTCP_NO_CIPHER:
		case MS_AES_128_SHA1_80_NO_CIPHER:
		case MS_CRYPTO_SUITE_INVALID:
		default:
			return false;
	}
}

const char *ms_crypto_suite_to_string(MSCryptoSuite suite) {
	switch (suite) {
		case MS_CRYPTO_SUITE_INVALID:
			return "<invalid-or-unsupported-suite>";
			break;
		case MS_AES_128_SHA1_80:
			return "AES_CM_128_HMAC_SHA1_80";
			break;
		case MS_AES_128_SHA1_32:
			return "AES_CM_128_HMAC_SHA1_32";
			break;
		case MS_AES_128_SHA1_80_NO_AUTH:
			return "AES_CM_128_HMAC_SHA1_80 UNAUTHENTICATED_SRTP";
			break;
		case MS_AES_128_SHA1_32_NO_AUTH:
			return "AES_CM_128_HMAC_SHA1_32 UNAUTHENTICATED_SRTP";
			break;
		case MS_AES_128_SHA1_80_SRTP_NO_CIPHER:
			return "AES_CM_128_HMAC_SHA1_80 UNENCRYPTED_SRTP";
			break;
		case MS_AES_128_SHA1_80_SRTCP_NO_CIPHER:
			return "AES_CM_128_HMAC_SHA1_80 UNENCRYPTED_SRTCP";
			break;
		case MS_AES_128_SHA1_80_NO_CIPHER:
			return "AES_CM_128_HMAC_SHA1_80 UNENCRYPTED_SRTP UNENCRYPTED_SRTCP";
			break;
		case MS_AES_256_SHA1_80:
			return "AES_256_CM_HMAC_SHA1_80";
			break;
		case MS_AES_CM_256_SHA1_80:
			return "AES_CM_256_HMAC_SHA1_80";
			break;
		case MS_AES_256_SHA1_32:
			return "AES_256_CM_HMAC_SHA1_32";
			break;
		case MS_AEAD_AES_128_GCM:
			return "AEAD_AES_128_GCM";
			break;
		case MS_AEAD_AES_256_GCM:
			return "AEAD_AES_256_GCM";
			break;
	}
	return "<invalid-or-unsupported-suite>";
}

static err_status_t ms_srtp_add_or_update_stream(srtp_t session, const srtp_policy_t *policy) {
	err_status_t status = srtp_update_stream(session, policy);
	if (status != srtp_err_status_ok) {
		status = srtp_add_stream(session, policy);
	}

	return status;
}

static int ms_add_srtp_stream(MSSrtpStreamContext *streamCtx,
                              MSCryptoSuite suite,
                              const uint8_t *key,
                              size_t key_length,
                              bool is_send,
                              bool is_inner,
                              uint32_t ssrc) {
	srtp_policy_t policy;
	err_status_t err;
	ssrc_t ssrc_conf;
	srtp_t srtp = is_inner ? streamCtx->mInnerSrtp : streamCtx->mSrtp;

	memset(&policy, 0, sizeof(policy));

	/* Init both RTP and RTCP policies, even if this srtp_t is used for one of them.
	 * Indeed the key derivation algorithm that computes the SRTCP auth key depends on parameters
	 * of the SRTP stream.*/
	if (ms_set_srtp_crypto_policy(suite, &policy.rtp, true) != 0) {
		return -1;
	}

	// and RTCP stream
	if (ms_set_srtp_crypto_policy(suite, &policy.rtcp, false) != 0) {
		return -1;
	}

	// Check key size,
	switch (suite) {
		case MS_AES_128_SHA1_80_SRTP_NO_CIPHER:
			if ((int)key_length != policy.rtcp.cipher_key_len) {
				ms_error(
				    "Key size (%i) doesn't match the selected srtcp profile (required %d) - srtp profile unencrypted",
				    (int)key_length, policy.rtcp.cipher_key_len);
				return -1;
			}
			break;
		case MS_AES_128_SHA1_80_SRTCP_NO_CIPHER:
			if ((int)key_length != policy.rtp.cipher_key_len) {
				ms_error(
				    "Key size (%i) doesn't match the selected srtp profile (required %d) - srtcp profile unencrypted",
				    (int)key_length, policy.rtp.cipher_key_len);
				return -1;
			}
			break;
		default: // both rtp and rtcp policies should match the given one
			if (((int)key_length != policy.rtp.cipher_key_len) || ((int)key_length != policy.rtcp.cipher_key_len)) {
				ms_error("Key size (%i) doesn't match the selected srtp profile (required %d) or srtcp profile "
				         "(required %d)",
				         (int)key_length, policy.rtp.cipher_key_len, policy.rtcp.cipher_key_len);
				return -1;
			}
			break;
	}

	if (is_send) policy.allow_repeat_tx = 1; /*necessary for telephone-events*/

	/* When RTP bundle mode is used, the srtp_t is used to encrypt or decrypt several RTP streams (SSRC) at the same
	 * time. This is why we use the "template" mode of libsrtp, using ssrc_any_inbound and ssrc_any_outbound For inner
	 * encryption we must specify a SSRC when in reception as it is how we will be able to sort streams - they will not
	 * use the same inner key in sending mode, we have one key anyway */
	ssrc_conf.type = is_send ? ssrc_any_outbound : (is_inner ? ssrc_specific : ssrc_any_inbound);
	ssrc_conf.value = ssrc;

	policy.ssrc = ssrc_conf;
	policy.key = (unsigned char *)key;
	policy.next = NULL;

	err = ms_srtp_add_or_update_stream(srtp, &policy);
	if (err != err_status_ok) {
		ms_error("Failed to add stream to srtp session (%d)", err);
		return -1;
	}

	return 0;
}

/***********************************************/
/***** EXPORTED FUNCTIONS                  *****/
/***********************************************/
/**** Private to mediastreamer2 functions ****/
/* header declared in voip/private.h */
static int srtp_init_done = 0;

extern "C" int ms_srtp_init(void) {

	err_status_t st = srtp_err_status_ok;
	ms_message("srtp init");
	if (!srtp_init_done) {
		st = srtp_init();
		if (st == srtp_err_status_ok) {
			srtp_init_done++;
		} else {
			ms_fatal("Couldn't initialize SRTP library: %d.", (int)st);
		}
	} else srtp_init_done++;
	return (int)st;
}

extern "C" void ms_srtp_shutdown(void) {
	srtp_init_done--;
	if (srtp_init_done == 0) {
		srtp_shutdown();
	}
}

static int ms_media_stream_sessions_set_srtp_key(MSMediaStreamSessions *sessions,
                                                 MSCryptoSuite suite,
                                                 const uint8_t *key,
                                                 size_t key_length,
                                                 bool is_send,
                                                 bool is_inner,
                                                 MSSrtpKeySource source,
                                                 uint32_t ssrc) {
	int error = -1;
	int ret = 0;
	check_and_create_srtp_context(sessions);

	if (key) {
		ms_message("media_stream_set_srtp_%s%s_key(): key %02x..%02x (ssrc %x) stream sessions is [%p]",
		           (is_inner ? "inner_" : ""), (is_send ? "send" : "recv"), (uint8_t)key[0],
		           (uint8_t)key[key_length - 1], (!is_send && is_inner) ? ssrc : 0, sessions);
	} else {
		ms_message("media_stream_set_srtp_%s%s_key(): key none stream sessions is [%p]", (is_inner ? "inner_" : ""),
		           (is_send ? "send" : "recv"), sessions);
	}

	MSSrtpStreamContext *streamCtx = get_stream_context(sessions, is_send);

	/* When the key is NULL or suite set to INVALID, juste deactivate SRTP */
	if (key == NULL || suite == MS_CRYPTO_SUITE_INVALID) {
		if (is_inner) {
			streamCtx->mInnerStats.mSource = MSSrtpKeySourceUnavailable;
			streamCtx->mInnerStats.mSuite = MS_CRYPTO_SUITE_INVALID;
		} else {
			if (streamCtx->mSrtp) {
				srtp_dealloc(streamCtx->mSrtp);
				streamCtx->mSrtp = NULL;
			}
			streamCtx->mSecured = false;
			streamCtx->mStats.mSource = MSSrtpKeySourceUnavailable;
			streamCtx->mStats.mSuite = MS_CRYPTO_SUITE_INVALID;
		}
	} else if ((error = ms_media_stream_session_fill_srtp_context(sessions, is_send, is_inner))) {
		streamCtx->mSecured = false;
		if (is_inner) {
			streamCtx->mInnerStats.mSource = MSSrtpKeySourceUnavailable;
			streamCtx->mInnerStats.mSuite = MS_CRYPTO_SUITE_INVALID;
		} else {
			streamCtx->mStats.mSource = MSSrtpKeySourceUnavailable;
			streamCtx->mStats.mSuite = MS_CRYPTO_SUITE_INVALID;
		}
		ret = error;
	} else if ((error = ms_add_srtp_stream(streamCtx, suite, key, key_length, is_send, is_inner, ssrc))) {
		streamCtx->mSecured = false;
		if (is_inner) {
			streamCtx->mInnerStats.mSource = MSSrtpKeySourceUnavailable;
			streamCtx->mInnerStats.mSuite = MS_CRYPTO_SUITE_INVALID;
		} else {
			streamCtx->mStats.mSource = MSSrtpKeySourceUnavailable;
			streamCtx->mStats.mSuite = MS_CRYPTO_SUITE_INVALID;
		}
		ret = error;
	} else {
		if (is_inner) {
			streamCtx->mInnerStats.mSource = source;
			streamCtx->mInnerStats.mSuite = suite;
		} else {
			streamCtx->mSecured = ms_srtp_is_crypto_policy_secure(suite);
			streamCtx->mStats.mSource = source;
			streamCtx->mStats.mSuite = suite;
		}
	}

	/* Srtp encryption has changed, notify to get it in call stats */
	OrtpEvent *ev = ortp_event_new(ORTP_EVENT_SRTP_ENCRYPTION_CHANGED);
	OrtpEventData *eventData = ortp_event_get_data(ev);
	eventData->info.srtp_info.is_send = is_send;
	eventData->info.srtp_info.is_inner = is_inner;
	eventData->info.srtp_info.source = source;
	eventData->info.srtp_info.suite = suite;
	rtp_session_dispatch_event(sessions->rtp_session, ev);

	return ret;
}

/**** Public Functions ****/
/* header declared in include/mediastreamer2/ms_srtp.h */
extern "C" bool_t ms_srtp_supported(void) {
	return TRUE;
}

extern "C" void ms_srtp_context_delete(MSSrtpCtx *session) {
	if (session->mSend.mSrtp) srtp_dealloc(session->mSend.mSrtp);
	if (session->mRecv.mSrtp) srtp_dealloc(session->mRecv.mSrtp);
	if (session->mSend.mInnerSrtp) srtp_dealloc(session->mSend.mInnerSrtp);
	if (session->mRecv.mInnerSrtp) srtp_dealloc(session->mRecv.mInnerSrtp);

	delete (session);
}

extern "C" bool_t ms_media_stream_sessions_secured(const MSMediaStreamSessions *sessions, MediaStreamDir dir) {
	if (!sessions->srtp_context) return FALSE;

	switch (dir) {
		case MediaStreamSendRecv:
			return (sessions->srtp_context->mSend.mSecured && sessions->srtp_context->mRecv.mSecured);
		case MediaStreamSendOnly:
			return sessions->srtp_context->mSend.mSecured;
		case MediaStreamRecvOnly:
			return sessions->srtp_context->mRecv.mSecured;
	}

	return FALSE;
}

extern "C" MSSrtpKeySource ms_media_stream_sessions_get_srtp_key_source(const MSMediaStreamSessions *sessions,
                                                                        MediaStreamDir dir,
                                                                        bool_t is_inner) {
	if (sessions->srtp_context == NULL) {
		return MSSrtpKeySourceUnavailable;
	}
	switch (dir) {
		case MediaStreamSendRecv:
			// Check sender and receiver keys have the same source
			if (is_inner == TRUE) {
				if (sessions->srtp_context->mSend.mInnerStats.mSource ==
				    sessions->srtp_context->mRecv.mInnerStats.mSource) {
					return sessions->srtp_context->mSend.mInnerStats.mSource;
				} else {
					return MSSrtpKeySourceUnavailable;
				}
			} else {
				if (sessions->srtp_context->mSend.mStats.mSource == sessions->srtp_context->mRecv.mStats.mSource) {
					return sessions->srtp_context->mSend.mStats.mSource;
				} else {
					return MSSrtpKeySourceUnavailable;
				}
			}
			break;
		case MediaStreamSendOnly:
			if (is_inner == TRUE) {
				return sessions->srtp_context->mSend.mInnerStats.mSource;
			} else {
				return sessions->srtp_context->mSend.mStats.mSource;
			}
		case MediaStreamRecvOnly:
			if (is_inner == TRUE) {
				return sessions->srtp_context->mRecv.mInnerStats.mSource;
			} else {
				return sessions->srtp_context->mRecv.mStats.mSource;
			}
	}
	return MSSrtpKeySourceUnavailable;
}

extern "C" MSCryptoSuite ms_media_stream_sessions_get_srtp_crypto_suite(const MSMediaStreamSessions *sessions,
                                                                        MediaStreamDir dir,
                                                                        bool_t is_inner) {
	if (sessions->srtp_context == NULL) {
		return MS_CRYPTO_SUITE_INVALID;
	}
	switch (dir) {
		case MediaStreamSendRecv:
			if (is_inner == TRUE) {
				// Check sender and receiver keys have the suite
				if (sessions->srtp_context->mSend.mInnerStats.mSuite ==
				    sessions->srtp_context->mRecv.mInnerStats.mSuite) {
					return sessions->srtp_context->mSend.mInnerStats.mSuite;
				} else {
					return MS_CRYPTO_SUITE_INVALID;
				}
			} else {
				// Check sender and receiver keys have the suite
				if (sessions->srtp_context->mSend.mStats.mSuite == sessions->srtp_context->mRecv.mStats.mSuite) {
					return sessions->srtp_context->mSend.mStats.mSuite;
				} else {
					return MS_CRYPTO_SUITE_INVALID;
				}
			}
			break;
		case MediaStreamSendOnly:
			if (is_inner == TRUE) {
				return sessions->srtp_context->mSend.mInnerStats.mSuite;
			} else {
				return sessions->srtp_context->mSend.mStats.mSuite;
			}
		case MediaStreamRecvOnly:
			if (is_inner == TRUE) {
				return sessions->srtp_context->mRecv.mInnerStats.mSuite;
			} else {
				return sessions->srtp_context->mRecv.mStats.mSuite;
			}
	}
	return MS_CRYPTO_SUITE_INVALID;
}

static int ms_media_stream_sessions_set_srtp_key_b64_base(MSMediaStreamSessions *sessions,
                                                          MSCryptoSuite suite,
                                                          const char *b64_key,
                                                          MSSrtpKeySource source,
                                                          bool is_send,
                                                          bool is_inner,
                                                          uint32_t ssrc) {
	int retval;

	size_t key_length = 0;
	uint8_t *key = NULL;

	if (b64_key != NULL) {
		/* decode b64 key */
		size_t b64_key_length = strlen(b64_key);
		// size_t max_key_length = b64_decode(b64_key, b64_key_length, 0, 0);
		bctbx_base64_decode(nullptr, &key_length, (const unsigned char *)b64_key, b64_key_length);
		key = (uint8_t *)ms_malloc0(key_length);
		// if ((key_length = b64_decode(b64_key, b64_key_length, key, max_key_length)) == 0) {
		if ((retval = bctbx_base64_decode(key, &key_length, (const unsigned char *)b64_key, b64_key_length)) != 0) {
			ms_error("Error decoding b64 srtp (%s) key : error -%x", b64_key, -retval);
			ms_free(key);
			return -1;
		}
	}

	/* pass decoded key to set_recv_key function */
	retval = ms_media_stream_sessions_set_srtp_key(sessions, suite, key, key_length, is_send, is_inner, source, ssrc);

	ms_free(key);

	return retval;
}

extern "C" int ms_media_stream_sessions_set_srtp_recv_key_b64(MSMediaStreamSessions *sessions,
                                                              MSCryptoSuite suite,
                                                              const char *b64_key,
                                                              MSSrtpKeySource source) {
	return ms_media_stream_sessions_set_srtp_key_b64_base(sessions, suite, b64_key, source, false, false, NULL_SSRC);
}
extern "C" int ms_media_stream_sessions_set_srtp_inner_recv_key_b64(
    MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char *b64_key, MSSrtpKeySource source, uint32_t ssrc) {
	return ms_media_stream_sessions_set_srtp_key_b64_base(sessions, suite, b64_key, source, false, true, ssrc);
}

extern "C" int ms_media_stream_sessions_set_srtp_send_key_b64(MSMediaStreamSessions *sessions,
                                                              MSCryptoSuite suite,
                                                              const char *b64_key,
                                                              MSSrtpKeySource source) {
	return ms_media_stream_sessions_set_srtp_key_b64_base(sessions, suite, b64_key, source, true, false, NULL_SSRC);
}

extern "C" int ms_media_stream_sessions_set_srtp_inner_send_key_b64(MSMediaStreamSessions *sessions,
                                                                    MSCryptoSuite suite,
                                                                    const char *b64_key,
                                                                    MSSrtpKeySource source) {
	return ms_media_stream_sessions_set_srtp_key_b64_base(sessions, suite, b64_key, source, true, true, NULL_SSRC);
}

extern "C" int ms_media_stream_sessions_set_srtp_recv_key(MSMediaStreamSessions *sessions,
                                                          MSCryptoSuite suite,
                                                          const uint8_t *key,
                                                          size_t key_length,
                                                          MSSrtpKeySource source) {
	return ms_media_stream_sessions_set_srtp_key(sessions, suite, key, key_length, false, false, source, NULL_SSRC);
}

extern "C" int ms_media_stream_sessions_set_srtp_inner_recv_key(MSMediaStreamSessions *sessions,
                                                                MSCryptoSuite suite,
                                                                const uint8_t *key,
                                                                size_t key_length,
                                                                MSSrtpKeySource source,
                                                                uint32_t ssrc) {
	return ms_media_stream_sessions_set_srtp_key(sessions, suite, key, key_length, false, true, source, ssrc);
}

extern "C" int ms_media_stream_sessions_set_srtp_send_key(MSMediaStreamSessions *sessions,
                                                          MSCryptoSuite suite,
                                                          const uint8_t *key,
                                                          size_t key_length,
                                                          MSSrtpKeySource source) {
	return ms_media_stream_sessions_set_srtp_key(sessions, suite, key, key_length, true, false, source, NULL_SSRC);
}

extern "C" int ms_media_stream_sessions_set_srtp_inner_send_key(MSMediaStreamSessions *sessions,
                                                                MSCryptoSuite suite,
                                                                const uint8_t *key,
                                                                size_t key_length,
                                                                MSSrtpKeySource source) {
	return ms_media_stream_sessions_set_srtp_key(sessions, suite, key, key_length, true, true, source, NULL_SSRC);
}

extern "C" int ms_media_stream_sessions_set_encryption_mandatory(MSMediaStreamSessions *sessions, bool_t yesno) {
	/*for now, managing all streams in one time*/
	int err;
	check_and_create_srtp_context(sessions);
	if (yesno) {
		if ((err = ms_media_stream_sessions_fill_srtp_context_all_stream(sessions))) {
			return err;
		}
	}
	sessions->srtp_context->mSend.mMandatoryEnabled = (yesno == TRUE);
	sessions->srtp_context->mRecv.mMandatoryEnabled = (yesno == TRUE);
	return 0;
}

extern "C" bool_t ms_media_stream_sessions_get_encryption_mandatory(const MSMediaStreamSessions *sessions) {

	if (!sessions->srtp_context) return FALSE;

	return sessions->srtp_context->mSend.mMandatoryEnabled && sessions->srtp_context->mRecv.mMandatoryEnabled;
}

extern "C" int ms_media_stream_sessions_set_ekt_mode(MSMediaStreamSessions *sessions, MSEKTMode mode) {
	check_and_create_srtp_context(sessions);

	switch (mode) {
		case MS_EKT_DISABLED:
		case MS_EKT_ENABLED:
		case MS_EKT_TRANSFER:
			sessions->srtp_context->mSend.mEktMode = mode;
			sessions->srtp_context->mRecv.mEktMode = mode;
			break;
		default:
			sessions->srtp_context->mSend.mEktMode = MS_EKT_DISABLED;
			sessions->srtp_context->mRecv.mEktMode = MS_EKT_DISABLED;
			ms_error("Invalid EKT operation mode %d", (int)mode);
			return -1;
			break;
	}

	return 0;
}

static void ms_media_stream_generate_and_set_srtp_keys_for_ekt(MSMediaStreamSessions *sessions,
                                                               std::shared_ptr<Ekt> ekt) {
	size_t master_key_size = ms_srtp_get_master_key_size(ekt->mSrtpCryptoSuite);
	uint8_t salted_key[SRTP_MAX_KEY_LEN]; // local buffer to temporary store key||salt

	// Generate new Master Key for this sending context
	ekt->mSrtpMasterKey = sessions->srtp_context->mRNG.randomize(master_key_size);
	memcpy(salted_key, ekt->mSrtpMasterKey.data(), master_key_size); // copy the freshly generated master key
	memcpy(salted_key + master_key_size, ekt->mSrtpMasterSalt.data(),
	       ekt->mSrtpMasterSalt.size()); // append the master salt after the key

	// Set these keys in the current srtp context
	ms_media_stream_sessions_set_srtp_inner_send_key(sessions, ekt->mSrtpCryptoSuite, salted_key,
	                                                 master_key_size + ekt->mSrtpMasterSalt.size(), MSSrtpKeySourceEKT);

	// Cleaning
	bctbx_clean(salted_key, master_key_size);
}

extern "C" int ms_media_stream_sessions_set_ekt(MSMediaStreamSessions *sessions, const MSEKTParametersSet *ekt_params) {
	ms_message("set EKT with SPI %04x on session %p", ekt_params->ekt_spi, sessions);
	check_and_create_srtp_context(sessions);
	// Force the operating mode to enable as we are given a key
	sessions->srtp_context->mRecv.mEktMode = MS_EKT_ENABLED;
	sessions->srtp_context->mSend.mEktMode = MS_EKT_ENABLED;

	std::shared_ptr<Ekt> ekt = nullptr;

	// Check we do not have it yet in the receiver map
	if (sessions->srtp_context->mRecv.ektsReceiverPool.count(ekt_params->ekt_spi) != 0) {
		// Is this the one used in send context?
		if (sessions->srtp_context->mSend.ektSender != nullptr &&
		    sessions->srtp_context->mSend.ektSender->mSpi == ekt_params->ekt_spi) {
			ms_warning("EKT with SPI %04x already present and used for outgoing ekttags, keep using it, no SRTP master "
			           "key generation",
			           ekt_params->ekt_spi);
			return 0;
		} else {
			ms_warning("EKT with SPI %04x already present, switch back to it for outgoing ekttags and regenerate srtp "
			           "master key",
			           ekt_params->ekt_spi);
			ekt = sessions->srtp_context->mRecv.ektsReceiverPool[ekt_params->ekt_spi];
			ekt->mEpoch++;
		}
	}

	// create the ekt object from given params, insert it in the recv context map and set is as the current sending one
	ekt = std::make_shared<Ekt>(ekt_params);
	sessions->srtp_context->mRecv.ektsReceiverPool.emplace(ekt_params->ekt_spi, ekt);
	sessions->srtp_context->mSend.ektSender = ekt;
	// SRTP master key retrieval is performed upon reception of full EKT tag matching the SPI.
	// Generate a master key for sending stream
	ms_media_stream_generate_and_set_srtp_keys_for_ekt(sessions, ekt);
	return 0;
}

#else /* HAVE_SRTP */

typedef void *srtp_t;
typedef int err_status_t;

extern "C" bool_t ms_srtp_supported(void) {
	return FALSE;
}

extern "C" int ms_srtp_init(void) {
	return -1;
}

extern "C" void ms_srtp_shutdown(void) {
}

extern "C" int ms_media_stream_sessions_set_srtp_recv_key_b64(struct _MSMediaStreamSessions *sessions,
                                                              MSCryptoSuite suite,
                                                              const char *b64_key,
                                                              MSSrtpKeySource source) {
	ms_error("Unable to set srtp recv key b64: srtp support disabled in mediastreamer2");
	return -1;
}

extern "C" int ms_media_stream_sessions_set_srtp_recv_key(
    MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char *key, size_t key_length, MSSrtpKeySource source) {
	ms_error("Unable to set srtp recv key: srtp support disabled in mediastreamer2");
	return -1;
}

extern "C" int ms_media_stream_sessions_set_srtp_send_key_b64(MSMediaStreamSessions *sessions,
                                                              MSCryptoSuite suite,
                                                              const char *b64_key,
                                                              MSSrtpKeySource source) {
	ms_error("Unable to set srtp send key b64: srtp support disabled in mediastreamer2");
	return -1;
}

extern "C" int ms_media_stream_sessions_set_srtp_send_key(
    MSMediaStreamSessions *sessions, MSCryptoSuite suite, const char *key, size_t key_length, MSSrtpKeySource source) {
	ms_error("Unable to set srtp send key: srtp support disabled in mediastreamer2");
	return -1;
}

extern "C" int ms_media_stream_session_set_encryption_mandatory(MSMediaStreamSessions *sessions, bool_t yesno) {
	ms_error("Unable to set srtp encryption mandatory mode: srtp support disabled in mediastreamer2");
	return -1;
}

extern "C" bool_t ms_media_stream_sessions_get_encryption_mandatory(const MSMediaStreamSessions *sessions) {
	ms_error("Unable to get srtp encryption mandatory mode: srtp support disabled in mediastreamer2");
	return -1;
}

extern "C" bool_t ms_media_stream_sessions_secured(const MSMediaStreamSessions *sessions, MediaStreamDir dir) {
	return FALSE;
}

extern "C" MSSrtpKeySource ms_media_stream_sessions_get_srtp_key_source(const MSMediaStreamSessions *sessions,
                                                                        MediaStreamDir dir,
                                                                        bool_t is_inner) {
	return MSSrtpKeySourceUnavailable;
}

extern "C" MSCryptoSuite ms_media_stream_sessions_get_srtp_crypto_suite(const MSMediaStreamSessions *sessions,
                                                                        MediaStreamDir dir,
                                                                        bool_t is_inner) {
	return MS_CRYPTO_SUITE_INVALID;
}

extern "C" void ms_srtp_context_delete(MSSrtpCtx *session) {
	ms_error("Unable to delete srtp context [%p]: srtp support disabled in mediastreamer2", session);
}

extern "C" int ms_media_stream_sessions_set_encryption_mandatory(MSMediaStreamSessions *sessions, bool_t yesno) {
	ms_error("Unable to set encryption_mandatory [%p]: srtp support disabled in mediastreamer2", sessions);
	return -1;
}
extern "C" int ms_media_stream_sessions_set_ekt_mode(MSMediaStreamSessions *sessions, MSEKTMode mode) {
	ms_error("Unable to set EKT operation mode: srtp support disabled in mediastreamer2");
	return -1;
}
extern "C" int ms_media_stream_sessions_set_ekt(MSMediaStreamSessions *sessions, MSEKTParametersSet *ekt) {
	ms_error("Unable to set EKT key: srtp support disabled in mediastreamer2");
	return -1;
}
#endif
