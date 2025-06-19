/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
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

#include "bctoolbox/vfs_standard.h"
#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/mstonedetector.h"
#include "mediastreamer2/msutils.h"
#include "mediastreamer2/msvolume.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

#define HELLO_8K_1S_FILE "sounds/hello8000-1s.wav"

/* Payload types: define several payload types as the relay would change it */
#define MARIELLE_PAYLOAD_TYPE 121
#define PAULINE_PAYLOAD_TYPE 123
#define RELAY_PAYLOAD_TYPE 123

static MSFactory *_factory = NULL;
static RtpProfile *profile = NULL;
static int tester_before_all(void) {
	// ms_init();
	_factory = ms_tester_factory_new();

	ms_factory_enable_statistics(_factory, TRUE);
	ortp_init();

	profile = rtp_profile_new("default profile");
	rtp_profile_set_payload(profile, MARIELLE_PAYLOAD_TYPE, &payload_type_pcmu8000);
	rtp_profile_set_payload(profile, PAULINE_PAYLOAD_TYPE, &payload_type_pcmu8000);
	rtp_profile_set_payload(profile, RELAY_PAYLOAD_TYPE, &payload_type_pcmu8000);

	return 0;
}

static int tester_after_all(void) {
	// ms_exit();
	rtp_profile_destroy(profile);

	ms_factory_destroy(_factory);
	return 0;
}

#define MARIELLE_RTP_PORT (base_port + 11)
#define MARIELLE_RTCP_PORT (base_port + 12)

#define MARGAUX_RTP_PORT (base_port + 13)
#define MARGAUX_RTCP_PORT (base_port + 14)

#define PAULINE_RTP_PORT (base_port + 15)
#define PAULINE_RTCP_PORT (base_port + 16)

/* Relay gets three pairs of port one for each correspondant */
#define RELAY_MARIELLE_RTP_PORT (base_port + 17)
#define RELAY_MARIELLE_RTCP_PORT (base_port + 18)
#define RELAY_MARGAUX_RTP_PORT (base_port + 19)
#define RELAY_MARGAUX_RTCP_PORT (base_port + 20)
#define RELAY_PAULINE_RTP_PORT (base_port + 21)
#define RELAY_PAULINE_RTCP_PORT (base_port + 22)

/* identify streams in bundle */
/* short ID header extension fit in 2 bytes, which means they can be added in the padding space left by audio level
 * extension */
#define SHORT_MID_MARIELLE_SESSION "m"
#define SHORT_MID_MARIELLE_SESSION_BIS "n"
#define SHORT_MID_PAULINE_SESSION "p"
#define SHORT_MID_MARIELLE_SOURCE_SESSION "a"
#define SHORT_MID_MARIELLE_SOURCE_SESSION_BIS "b"
/* long ID header extension fit in 8 or 9 bytes, which means they cannot fit in any padding and request the allocation
 * of more space */
#define LONG_MID_MARIELLE_SESSION "marielle"
#define LONG_MID_MARIELLE_SESSION_BIS "marielle_bis"
#define LONG_MID_PAULINE_SESSION "pauline"
#define LONG_MID_MARIELLE_SOURCE_SESSION "Marielle"
#define LONG_MID_MARIELLE_SOURCE_SESSION_BIS "MarielleBis"

static void push_rtp_session_into_list(bctbx_list_t *l, RtpSession *s) {
	bctbx_list_t *elem;
	for (elem = l; elem != nullptr; elem = elem->next) {
		RtpSession **pps = (RtpSession **)elem->data;
		if (*pps == nullptr) {
			*pps = s;
			return;
		}
	}
	BC_FAIL("push_rtp_session_into_list(): Unexpected RtpSession");
}

static void
new_ssrc_incoming_in_bundle(BCTBX_UNUSED(RtpSession *session), BCTBX_UNUSED(void *mp), void *s, void *userData) {
	RtpSession **newSession = (RtpSession **)s;
	bctbx_list_t *margaux_sessions_list = (bctbx_list_t *)userData;
	*newSession = rtp_session_new(RTP_SESSION_RECVONLY);
	rtp_session_set_profile(*newSession, profile);
	rtp_session_enable_jitter_buffer(*newSession,
	                                 FALSE); // Disable jitter buffer for the final recipient, we want to get data
	                                         // when they arrive, we're assuming no loss
	rtp_session_enable_rtcp(*newSession, FALSE);
	rtp_session_set_payload_type(*newSession, RELAY_PAYLOAD_TYPE);

	push_rtp_session_into_list(margaux_sessions_list, *newSession);
}

static void
new_ssrc_outgoing_in_bundle(BCTBX_UNUSED(RtpSession *session), BCTBX_UNUSED(void *mp), void *s, void *userData) {
	RtpSession **newSession = (RtpSession **)s;
	bctbx_list_t *relay_sessions_list = (bctbx_list_t *)userData;
	*newSession = rtp_session_new(RTP_SESSION_SENDONLY);
	rtp_session_set_profile(*newSession, profile);
	rtp_session_enable_transfer_mode(*newSession, TRUE); // relay rtp session is in transfer mode
	rtp_session_enable_rtcp(*newSession, FALSE);
	rtp_session_set_payload_type(*newSession, RELAY_PAYLOAD_TYPE);

	// the session list holds 2 sessions, use them one after the other to create sessions
	push_rtp_session_into_list(relay_sessions_list, *newSession);
}

namespace {
struct test_params {
	MSCryptoSuite outerSuite;  /**< SRTP crypto suite used for outer encryption */
	bool useInnerEncryption;   /**< When set off do not use inner encryption - default in On */
	MSCryptoSuite innerSuite;  /**< SRTP crypto suite used for inner encryption */
	bool useParticipantVolume; /**< include participant volumes (force usage of other extension header) */
	bool useLongBundleId;      /**< use long strings for bundle Id -> ofrce re-allocation in packet header */
	bool useBundledSource;     /**< Marie streams 2 sessions bundled */
	bool useEkt; /**< inner encryption key SRPT are exchanged using EKT tag - EKT exchange is not part of this test */
	bool skipPaulineBegin; /**< Pauline first 12 packets are lost, so Margaux starts getting packets from Pauline after
	                          the ROC change */
	bool discardPaulinePackets; /**< Relay discards 10 packets from Pauline index 20 to 29 -> test seq num continuity on
	                               final recipient: no packet loss detected */
	bool useSharedMid; /**< relayed bundled sessions are using the same MID: relay will declare the three sessions with
	                      same MID (and must identify the correct one when sending). On the reception side, Margaux has
	                      three sessions, SSRC are pre-assigned on send and receive are pre-assigned - implies
	                      useLongBundleId and useBundledSource */
	bool autodiscoverBundleSessions; /**< relay to margaux sessions are auto discovered: create only one session (on
	                                    relay side and on margaux side) and set it in a bundle. Bundle callbacks create
	                                    new session on needed basis.Implies shared_mid and useEkt(as we cannot set
	                                    inner keys without knowing first the expected SSRC */

	test_params()
	    : outerSuite{MS_AES_128_SHA1_80}, useInnerEncryption{true}, innerSuite{MS_AEAD_AES_256_GCM},
	      useParticipantVolume{false}, useLongBundleId{false}, useBundledSource{false}, useEkt{false},
	      skipPaulineBegin{false}, discardPaulinePackets{false}, useSharedMid{false},
	      autodiscoverBundleSessions{false} {
	}
};
} // namespace

static bool_t double_encrypted_rtp_relay_data_base(test_params &p) {

	if (!ms_srtp_supported()) {
		ms_warning("srtp not available, skiping...");
		return TRUE;
	}
	// auto discover implies using ekt and shared Mid
	if (p.autodiscoverBundleSessions) {
		p.useSharedMid = true;
		if (p.useInnerEncryption) {
			p.useEkt = true;
		}
	}
	// Share MID implies using bundled source and long bundle id
	if (p.useSharedMid) {
		p.useBundledSource = true;
		p.useLongBundleId = true;
	}

	char *hello_file = bc_tester_res(HELLO_8K_1S_FILE);
	BCTBX_SLOGI << "Double encrypted rtp relay test - Params:" << std::endl
	            << " - Outer suite : " << (ms_crypto_suite_to_string(p.outerSuite)) << std::endl
	            << " - Inner suite : " << (ms_crypto_suite_to_string(p.innerSuite)) << std::endl
	            << " - useInnerEncryption : " << p.useInnerEncryption << std::endl
	            << " - useParticipantVolume : " << p.useParticipantVolume << std::endl
	            << " - useLongBundleId : " << p.useLongBundleId << std::endl
	            << " - useBundledSource : " << p.useBundledSource << std::endl
	            << " - useEkt : " << p.useEkt << std::endl
	            << " - useSharedMid : " << p.useSharedMid << std::endl
	            << " - skipPaulineBegin : " << p.skipPaulineBegin << std::endl
	            << " - discardPaulinePackets : " << p.discardPaulinePackets << std::endl
	            << " - autodiscoverBundleSessions : " << p.autodiscoverBundleSessions << std::endl;

	bctbx_vfs_file_t *fp = bctbx_file_open(&bcStandardVfs, hello_file, "r");
	bc_free(hello_file);
	bctbx_list_t *margaux_sessions_list = NULL;
	bctbx_list_t *relay_sessions_list = NULL;

	MSEKTParametersSet ekt_params;
	if (p.useEkt) {
		p.useInnerEncryption = true;
		uint8_t master_salt[14] = {
		    0x01, 0x10, 0x11, 0x04, 0x40, 0x44, 0x07,
		    0x70, 0x77, 0x0a, 0xa0, 0xaa, 0xf0, 0x0f}; // 14 bytes master salt even if we may end up using only 12 bytes
		// 32 bytes EKT key value even if we may endup using only 16
		uint8_t key_value[32] = {0x23, 0xd4, 0x19, 0x12, 0x7e, 0x85, 0xa3, 0x14, 0xa8, 0x47, 0x71,
		                         0x2d, 0x04, 0x3c, 0x31, 0x50, 0xad, 0x2d, 0x16, 0x97, 0xa1, 0x60,
		                         0x41, 0xe4, 0xc5, 0xec, 0x78, 0xc1, 0xdf, 0x99, 0xb8, 0xd9};
		MSEKTCipherType ekt_cipher = MS_EKT_CIPHERTYPE_AESKW128;
		if (p.innerSuite == MS_AES_256_SHA1_80 || p.innerSuite == MS_AES_256_SHA1_32 ||
		    p.innerSuite == MS_AEAD_AES_256_GCM) {
			ekt_cipher = MS_EKT_CIPHERTYPE_AESKW256;
		}
		ekt_params.ekt_cipher_type = ekt_cipher;
		ekt_params.ekt_srtp_crypto_suite = p.innerSuite;
		memcpy(ekt_params.ekt_key_value, key_value, 32);
		memcpy(ekt_params.ekt_master_salt, master_salt, 14);
		ekt_params.ekt_spi = 0x1234;
		ekt_params.ekt_ttl = 0; // do not use ttl
	}

	const char *aes_128_bits_marielle_outer_key = "d0RmdmcmVCspeEc3QGZiNWpVLFJhQX1cfHAwJSoj";
	const char *aes_128_bits_marielle_inner_key = "eCYF4nYyCvmCpFWjUeDaxI2GWp2BzCRlIPfg52Te";
	const char *aes_128_bits_pauline_outer_key = "6jCLmtRkVW9E/BUuJtYj/R2z6+4iEe06/DWohQ9F";
	const char *aes_128_bits_pauline_inner_key = "CVamr4a05ebeHUhZGuoNcY5PpaxWR59uYFwzu0Am";
	const char *aes_128_bits_margaux_outer_key = "2qgyEDZiYTtaxgY+rKJUemLKMFbCy6LsWfhAuCxG";

	const char *aes_256_bits_marielle_outer_key = "nJNTwiMkyAu8zs0MWUiSQbnBL4M+xkWTYgrVLR2eFwZyO+ca2UqBy2Uh9pVRbA==";
	const char *aes_256_bits_marielle_inner_key = "N3vq6TMfvtyYpqGaEi9vAHMCzgWJvaD1PIfwEYtdEgI2ACezZo2vpOdV2YWEcQ==";
	const char *aes_256_bits_pauline_outer_key = "UKg69sFLbrA7d0hEVKMtT83R3GR3sjhE0XMqNBbQ+axoDWMP5dQNfjNuSQQHbw==";
	const char *aes_256_bits_pauline_inner_key = "ilm37gyQGIV62ISFvFPsKqm2Zma/rcDG4kTp2jsh+nOwMHSZg4SNB/y28Twrvw==";
	const char *aes_256_bits_margaux_outer_key = "EJ1w/9QVGT0TkLdE3CR5ZHMkf7I/j9bORHAFGKo7cIjZ39Yl8ZZfaR4Yg9XL2g==";

	const char *aes_gcm_128_bits_marielle_outer_key = "bkTcxXe9N3/vHKKiqQAqmL0qJ+CSiWRat/Tadg==";
	const char *aes_gcm_128_bits_marielle_inner_key = "MPKEi1/zHMH9osL2FIxUH/r3BiPjgS/LWIiTPA==";
	const char *aes_gcm_128_bits_pauline_outer_key = "Ya+BvAxQUqPer3X/AF4gDJUT4pVjbYc6O+u1pg==";
	const char *aes_gcm_128_bits_pauline_inner_key = "dTgaAhtNHGQa9Zt4WRrcKrfjXt+2tOfUTvSg5Q==";
	const char *aes_gcm_128_bits_margaux_outer_key = "wc2/ctTL3CHjxBf4h35WXCACxKhNxGS7q+t0ww==";

	const char *aes_gcm_256_bits_marielle_outer_key = "WpvA7zUhbhJ2i1ui2nOX43QjrOwCGBkaCPtjnphQKwv/L+GdscAKGQWzG/c=";
	const char *aes_gcm_256_bits_marielle_inner_key = "J74fLdR6tp6EwJVgWjtcGufB7GcR64kAHbIbZyGKVq62acCZmx4mNNLIkus=";
	const char *aes_gcm_256_bits_pauline_outer_key = "PtyD6l92cGR643om/5dEIGirCCxPeL9/LJF7PaFMoMocqMrz73CO0Fz7L20=";
	const char *aes_gcm_256_bits_pauline_inner_key = "sIimmQ8m4PWKl1x1iu+H1uqj3pcVtvg6LDNmFEdPOLxbClt+8ZQ8DmJ/PRg=";
	const char *aes_gcm_256_bits_margaux_outer_key = "ng3FvX7U7GZqZ8gpVioo8mR0qQFrJZF8QxCgMdJ75IKB3ZRRwEWgtYREN50=";

	const char *marielle_outer_key = NULL;
	const char *pauline_outer_key = NULL;
	const char *margaux_outer_key = NULL;
	const char *marielle_inner_key = NULL;
	const char *pauline_inner_key = NULL;

	switch (p.outerSuite) {
		case MS_AES_128_SHA1_32:
		case MS_AES_128_SHA1_80:
			marielle_outer_key = aes_128_bits_marielle_outer_key;
			pauline_outer_key = aes_128_bits_pauline_outer_key;
			margaux_outer_key = aes_128_bits_margaux_outer_key;
			break;
		case MS_AES_256_SHA1_32:
		case MS_AES_256_SHA1_80:
		case MS_AES_CM_256_SHA1_80:
			marielle_outer_key = aes_256_bits_marielle_outer_key;
			pauline_outer_key = aes_256_bits_pauline_outer_key;
			margaux_outer_key = aes_256_bits_margaux_outer_key;
			break;
		case MS_AEAD_AES_128_GCM:
			marielle_outer_key = aes_gcm_128_bits_marielle_outer_key;
			pauline_outer_key = aes_gcm_128_bits_pauline_outer_key;
			margaux_outer_key = aes_gcm_128_bits_margaux_outer_key;
			break;
		case MS_AEAD_AES_256_GCM:
			marielle_outer_key = aes_gcm_256_bits_marielle_outer_key;
			pauline_outer_key = aes_gcm_256_bits_pauline_outer_key;
			margaux_outer_key = aes_gcm_256_bits_margaux_outer_key;
			break;
		default:
			BC_FAIL("Unsupported suite");
			return FALSE;
	}

	// Set inner keys if needed (not Ekt and using inner encryption
	if (p.useInnerEncryption && !p.useEkt) {
		switch (p.innerSuite) {
			case MS_AES_128_SHA1_32:
			case MS_AES_128_SHA1_80:
				marielle_inner_key = aes_128_bits_marielle_inner_key;
				pauline_inner_key = aes_128_bits_pauline_inner_key;
				break;
			case MS_AES_256_SHA1_32:
			case MS_AES_256_SHA1_80:
			case MS_AES_CM_256_SHA1_80:
				marielle_inner_key = aes_256_bits_marielle_inner_key;
				pauline_inner_key = aes_256_bits_pauline_inner_key;
				break;
			case MS_AEAD_AES_128_GCM:
				marielle_inner_key = aes_gcm_128_bits_marielle_inner_key;
				pauline_inner_key = aes_gcm_128_bits_pauline_inner_key;
				break;
			case MS_AEAD_AES_256_GCM:
				marielle_inner_key = aes_gcm_256_bits_marielle_inner_key;
				pauline_inner_key = aes_gcm_256_bits_pauline_inner_key;
				break;
			default:
				BC_FAIL("Unsupported suite");
				return FALSE;
		}
	}

	/* Margaux is the final recipient: build 2 rtpsession, they will be bundled  */
	/* This is the bundle primary session, code mostly copied from ms_create_duplex_rtp_session*/
	RtpSession *rtpSession_margaux_marielle = rtp_session_new(RTP_SESSION_RECVONLY);
	rtp_session_set_recv_buf_size(rtpSession_margaux_marielle, ms_factory_get_mtu(_factory));
	rtp_session_set_scheduling_mode(rtpSession_margaux_marielle, 0);
	rtp_session_set_blocking_mode(rtpSession_margaux_marielle, 0);
	rtp_session_set_symmetric_rtp(rtpSession_margaux_marielle, TRUE);
	rtp_session_set_local_addr(rtpSession_margaux_marielle, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT);
	rtp_session_set_multicast_loopback(rtpSession_margaux_marielle, TRUE);
	rtp_session_set_profile(rtpSession_margaux_marielle, profile);
	rtp_session_enable_jitter_buffer(rtpSession_margaux_marielle,
	                                 FALSE); // Disable jitter buffer for the final recipient, we want to get data when
	                                         // they arrive, we're assuming no loss
	rtp_session_enable_rtcp(rtpSession_margaux_marielle, FALSE);
	rtp_session_set_payload_type(rtpSession_margaux_marielle, RELAY_PAYLOAD_TYPE);

	RtpSession *rtpSession_margaux_pauline = NULL;
	RtpSession *rtpSession_margaux_marielle_bis = NULL;
	if (p.autodiscoverBundleSessions == false) {
		/* Second session, in RECV only, is bundled so we do not need to define local port */
		rtpSession_margaux_pauline = rtp_session_new(RTP_SESSION_RECVONLY);
		rtp_session_set_profile(rtpSession_margaux_pauline, profile);
		rtp_session_set_recv_buf_size(rtpSession_margaux_pauline, MAX(ms_factory_get_mtu(_factory), 1500));
		rtp_session_enable_jitter_buffer(rtpSession_margaux_pauline,
		                                 FALSE); // Disable jitter buffer for the final recipient, we want to get data
		                                         // when they arrive, we're assuming no loss
		rtp_session_enable_rtcp(rtpSession_margaux_pauline, FALSE);
		rtp_session_set_payload_type(rtpSession_margaux_pauline, RELAY_PAYLOAD_TYPE);

		if (p.useBundledSource) { // Marielle source bundles two sessions so margaux receives 3
			// Third session, in RECV only, is bundled so we do not need to define local port
			rtpSession_margaux_marielle_bis = rtp_session_new(RTP_SESSION_RECVONLY);
			rtp_session_set_profile(rtpSession_margaux_marielle_bis, profile);
			rtp_session_set_recv_buf_size(rtpSession_margaux_marielle_bis, MAX(ms_factory_get_mtu(_factory), 1500));
			rtp_session_enable_jitter_buffer(rtpSession_margaux_marielle_bis,
			                                 FALSE); // Disable jitter buffer for the final recipient, we want to get
			                                         // data when they arrive, we're assuming no loss
			rtp_session_enable_rtcp(rtpSession_margaux_marielle_bis, FALSE);
			rtp_session_set_payload_type(rtpSession_margaux_marielle_bis, RELAY_PAYLOAD_TYPE);
		}
	} else {
		// set the additional margaux sessions pointers in a list, so we can get them in the callback in charge of their
		// creation
		margaux_sessions_list = bctbx_list_new(&rtpSession_margaux_marielle_bis);
		margaux_sessions_list = bctbx_list_append(margaux_sessions_list, &rtpSession_margaux_pauline);

		rtp_session_signal_connect(rtpSession_margaux_marielle, "new_incoming_ssrc_found_in_bundle",
		                           (RtpCallback)new_ssrc_incoming_in_bundle, margaux_sessions_list);
	}

	/* create a bundle, margaux_marielle is the main session */
	RtpBundle *rtpBundle_margaux = rtp_bundle_new();
	if (p.useLongBundleId) {
		// when p.useSharedMid is on(then use_long_bundle and p.useBundledSource are also on, to simplify the test
		// code), all the sessions use the same MID
		rtp_bundle_add_session(rtpBundle_margaux, LONG_MID_MARIELLE_SESSION, rtpSession_margaux_marielle);
		if (p.autodiscoverBundleSessions == false) {
			if (p.useSharedMid) {
				rtp_bundle_add_session(rtpBundle_margaux, LONG_MID_MARIELLE_SESSION, rtpSession_margaux_marielle_bis);
				rtp_bundle_add_session(rtpBundle_margaux, LONG_MID_MARIELLE_SESSION, rtpSession_margaux_pauline);
			} else {
				rtp_bundle_add_session(rtpBundle_margaux, LONG_MID_PAULINE_SESSION, rtpSession_margaux_pauline);
				if (p.useBundledSource) { // Marielle source bundles two sessions so margaux receives 3
					rtp_bundle_add_session(rtpBundle_margaux, LONG_MID_MARIELLE_SESSION_BIS,
					                       rtpSession_margaux_marielle_bis);
				}
			}
		}
	} else {
		rtp_bundle_add_session(rtpBundle_margaux, SHORT_MID_MARIELLE_SESSION, rtpSession_margaux_marielle);
		rtp_bundle_add_session(rtpBundle_margaux, SHORT_MID_PAULINE_SESSION, rtpSession_margaux_pauline);
		if (p.useBundledSource) { // Marielle source bundles two sessions so margaux receives 3
			rtp_bundle_add_session(rtpBundle_margaux, SHORT_MID_MARIELLE_SESSION_BIS, rtpSession_margaux_marielle_bis);
		}
	}
	rtp_bundle_set_mid_extension_id(rtpBundle_margaux, RTP_EXTENSION_MID);
	MSMediaStreamSessions margaux = {};
	margaux.rtp_session = rtpSession_margaux_marielle;

	/* the relay needs to open rtp session with all endpoints, 2 bundled sessions for margaux. Relay's RtpSession are
	 * all in transfer mode  */
	// relay_margaux: one main session created as duplex just because it is easier
	RtpSession *rtpSession_relay_margaux_marielle = ms_create_duplex_rtp_session(
	    RELAY_IP, RELAY_MARGAUX_RTP_PORT, RELAY_MARGAUX_RTCP_PORT, ms_factory_get_mtu(_factory));
	rtp_session_set_remote_addr_and_port(rtpSession_relay_margaux_marielle, MARGAUX_IP, MARGAUX_RTP_PORT,
	                                     MARGAUX_RTCP_PORT);
	rtp_session_set_profile(rtpSession_relay_margaux_marielle, profile);
	rtp_session_enable_transfer_mode(rtpSession_relay_margaux_marielle, TRUE); // relay rtp session is in transfer mode
	rtp_session_enable_rtcp(rtpSession_relay_margaux_marielle, FALSE);
	rtp_session_set_payload_type(rtpSession_relay_margaux_marielle, RELAY_PAYLOAD_TYPE);
	RtpSession *rtpSession_relay_margaux_marielle_2 = NULL;
	RtpSession *rtpSession_relay_margaux_marielle_bis = NULL;
	RtpSession *rtpSession_relay_margaux_pauline = NULL;
	if (p.autodiscoverBundleSessions == false) {
		// relay_margaux: secondary session in the bundle, minimal settings
		rtpSession_relay_margaux_pauline = rtp_session_new(RTP_SESSION_SENDONLY);
		rtp_session_set_profile(rtpSession_relay_margaux_pauline, profile);
		rtp_session_enable_transfer_mode(rtpSession_relay_margaux_pauline,
		                                 TRUE); // relay rtp session is in transfer mode
		rtp_session_enable_rtcp(rtpSession_relay_margaux_pauline, FALSE);
		rtp_session_set_payload_type(rtpSession_relay_margaux_pauline, RELAY_PAYLOAD_TYPE);

		if (p.useBundledSource) { // Marielle source bundles two sessions so margaux receives 3
			rtpSession_relay_margaux_marielle_bis = rtp_session_new(RTP_SESSION_SENDONLY);
			rtp_session_set_profile(rtpSession_relay_margaux_marielle_bis, profile);
			rtp_session_enable_transfer_mode(rtpSession_relay_margaux_marielle_bis,
			                                 TRUE); // relay rtp session is in transfer mode
			rtp_session_enable_rtcp(rtpSession_relay_margaux_marielle_bis, FALSE);
			rtp_session_set_payload_type(rtpSession_relay_margaux_marielle_bis, RELAY_PAYLOAD_TYPE);
		}
	} else {
		// set the additional relay sessions pointers in a list, so we can get them in the callback in charge of their
		// creation
		relay_sessions_list = bctbx_list_append(relay_sessions_list, &rtpSession_relay_margaux_marielle_2);
		relay_sessions_list = bctbx_list_append(relay_sessions_list, &rtpSession_relay_margaux_marielle_bis);
		relay_sessions_list = bctbx_list_append(relay_sessions_list, &rtpSession_relay_margaux_pauline);

		rtp_session_signal_connect(rtpSession_relay_margaux_marielle, "new_outgoing_ssrc_found_in_bundle",
		                           (RtpCallback)new_ssrc_outgoing_in_bundle, relay_sessions_list);
	}

	/* create a bundle, relay_margaux_marielle is the main session */
	RtpBundle *rtpBundle_relay = rtp_bundle_new();
	if (p.useLongBundleId) {
		rtp_bundle_add_session(rtpBundle_relay, LONG_MID_MARIELLE_SESSION, rtpSession_relay_margaux_marielle);
		if (p.autodiscoverBundleSessions == false) {
			// when p.useSharedMid is on(then use_long_bundle and p.useBundledSource are also on, to simplify the test
			// code), all the sessions use the same MID
			if (p.useSharedMid) {
				rtp_bundle_add_session(rtpBundle_relay, LONG_MID_MARIELLE_SESSION, rtpSession_relay_margaux_pauline);
				rtp_bundle_add_session(rtpBundle_relay, LONG_MID_MARIELLE_SESSION,
				                       rtpSession_relay_margaux_marielle_bis);
			} else {
				rtp_bundle_add_session(rtpBundle_relay, LONG_MID_PAULINE_SESSION, rtpSession_relay_margaux_pauline);
				if (p.useBundledSource) { // Marielle source bundles two sessions so margaux receives 3
					rtp_bundle_add_session(rtpBundle_relay, LONG_MID_MARIELLE_SESSION_BIS,
					                       rtpSession_relay_margaux_marielle_bis);
				}
			}
		}
	} else {
		rtp_bundle_add_session(rtpBundle_relay, SHORT_MID_MARIELLE_SESSION, rtpSession_relay_margaux_marielle);
		rtp_bundle_add_session(rtpBundle_relay, SHORT_MID_PAULINE_SESSION, rtpSession_relay_margaux_pauline);
		if (p.useBundledSource) { // Marielle source bundles two sessions so margaux receives 3
			rtp_bundle_add_session(rtpBundle_relay, SHORT_MID_MARIELLE_SESSION_BIS,
			                       rtpSession_relay_margaux_marielle_bis);
		}
	}
	rtp_bundle_set_mid_extension_id(rtpBundle_relay, RTP_EXTENSION_MID);
	MSMediaStreamSessions relay_margaux = {};
	relay_margaux.rtp_session = rtpSession_relay_margaux_marielle;
	if (p.useEkt) {
		ms_media_stream_sessions_set_ekt_mode(&relay_margaux, MS_EKT_TRANSFER);
	} else if (p.useInnerEncryption) {
		ms_media_stream_sessions_set_ekt_mode(&relay_margaux, MS_EKT_DISABLED_WITH_TRANSFER);
	}

	// marielle_relay: session used in recv only but created in duplex just because it is easier
	RtpSession *rtpSession_relay_marielle = ms_create_duplex_rtp_session(
	    RELAY_IP, RELAY_MARIELLE_RTP_PORT, RELAY_MARIELLE_RTCP_PORT, ms_factory_get_mtu(_factory));
	rtp_session_set_profile(rtpSession_relay_marielle, profile);
	rtp_session_enable_transfer_mode(rtpSession_relay_marielle, TRUE); // relay rtp session is in transfer mode
	rtp_session_enable_rtcp(rtpSession_relay_marielle, FALSE);
	rtp_session_set_payload_type(rtpSession_relay_marielle, MARIELLE_PAYLOAD_TYPE);
	MSMediaStreamSessions relay_marielle = {};
	relay_marielle.rtp_session = rtpSession_relay_marielle;
	if (p.useEkt) {
		ms_media_stream_sessions_set_ekt_mode(&relay_marielle, MS_EKT_TRANSFER);
	} else if (p.useInnerEncryption) {
		ms_media_stream_sessions_set_ekt_mode(&relay_marielle, MS_EKT_DISABLED_WITH_TRANSFER);
	}
	RtpSession *rtpSession_relay_marielle_bis = NULL;
	RtpBundle *rtpBundle_relay_marielle = NULL;
	if (p.useBundledSource) { // Marielle bundles two sessions so relay must get ready for it
		rtpSession_relay_marielle_bis = rtp_session_new(RTP_SESSION_RECVONLY);
		rtp_session_set_profile(rtpSession_relay_marielle_bis, profile);
		rtp_session_enable_transfer_mode(rtpSession_relay_marielle_bis, TRUE); // relay rtp session is in transfer mode
		rtp_session_enable_rtcp(rtpSession_relay_marielle_bis, FALSE);
		rtp_session_set_payload_type(rtpSession_relay_marielle_bis, MARIELLE_PAYLOAD_TYPE);
		rtpBundle_relay_marielle = rtp_bundle_new();
		if (p.useLongBundleId) {
			rtp_bundle_add_session(rtpBundle_relay_marielle, LONG_MID_MARIELLE_SOURCE_SESSION,
			                       rtpSession_relay_marielle);
			rtp_bundle_add_session(rtpBundle_relay_marielle, LONG_MID_MARIELLE_SOURCE_SESSION_BIS,
			                       rtpSession_relay_marielle_bis);
		} else {
			rtp_bundle_add_session(rtpBundle_relay_marielle, SHORT_MID_MARIELLE_SOURCE_SESSION,
			                       rtpSession_relay_marielle);
			rtp_bundle_add_session(rtpBundle_relay_marielle, SHORT_MID_MARIELLE_SOURCE_SESSION_BIS,
			                       rtpSession_relay_marielle_bis);
		}
		rtp_bundle_set_mid_extension_id(rtpBundle_relay_marielle, RTP_EXTENSION_MID);
	}

	// pauline_relay: session used in recv only but created in duplex just because it is easier
	RtpSession *rtpSession_relay_pauline = ms_create_duplex_rtp_session(
	    RELAY_IP, RELAY_PAULINE_RTP_PORT, RELAY_PAULINE_RTCP_PORT, ms_factory_get_mtu(_factory));
	rtp_session_set_profile(rtpSession_relay_pauline, profile);
	rtp_session_enable_transfer_mode(rtpSession_relay_pauline, TRUE); // relay rtp session is in transfer mode
	rtp_session_enable_rtcp(rtpSession_relay_pauline, FALSE);
	rtp_session_set_payload_type(rtpSession_relay_pauline, PAULINE_PAYLOAD_TYPE);
	MSMediaStreamSessions relay_pauline = {};
	relay_pauline.rtp_session = rtpSession_relay_pauline;
	if (p.useEkt) {
		ms_media_stream_sessions_set_ekt_mode(&relay_pauline, MS_EKT_TRANSFER);
	} else if (p.useInnerEncryption) {
		ms_media_stream_sessions_set_ekt_mode(&relay_pauline, MS_EKT_DISABLED_WITH_TRANSFER);
	}

	/* Marielle is a source */
	RtpSession *rtpSession_marielle =
	    ms_create_duplex_rtp_session(MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT, ms_factory_get_mtu(_factory));
	rtp_session_set_remote_addr_and_port(rtpSession_marielle, RELAY_IP, RELAY_MARIELLE_RTP_PORT,
	                                     RELAY_MARIELLE_RTCP_PORT);
	rtp_session_set_profile(rtpSession_marielle, profile);
	rtp_session_enable_rtcp(rtpSession_marielle, FALSE);
	rtp_session_set_payload_type(rtpSession_marielle, MARIELLE_PAYLOAD_TYPE);
	MSMediaStreamSessions marielle = {};
	marielle.rtp_session = rtpSession_marielle;
	RtpSession *rtpSession_marielle_bis = NULL;
	RtpBundle *rtpBundle_marielle = NULL;
	if (p.useBundledSource) { // Marielle bundles two sessions
		rtpSession_marielle_bis = rtp_session_new(RTP_SESSION_SENDONLY);
		rtp_session_set_profile(rtpSession_marielle_bis, profile);
		rtp_session_enable_rtcp(rtpSession_marielle_bis, FALSE);
		rtp_session_set_payload_type(rtpSession_marielle_bis, MARIELLE_PAYLOAD_TYPE);
		rtpBundle_marielle = rtp_bundle_new();
		if (p.useLongBundleId) {
			rtp_bundle_add_session(rtpBundle_marielle, LONG_MID_MARIELLE_SOURCE_SESSION, rtpSession_marielle);
			rtp_bundle_add_session(rtpBundle_marielle, LONG_MID_MARIELLE_SOURCE_SESSION_BIS, rtpSession_marielle_bis);
		} else {
			rtp_bundle_add_session(rtpBundle_marielle, SHORT_MID_MARIELLE_SOURCE_SESSION, rtpSession_marielle);
			rtp_bundle_add_session(rtpBundle_marielle, SHORT_MID_MARIELLE_SOURCE_SESSION_BIS, rtpSession_marielle_bis);
		}
		rtp_bundle_set_mid_extension_id(rtpBundle_marielle, RTP_EXTENSION_MID);
	}

	/* Pauline is a source */
	RtpSession *rtpSession_pauline =
	    ms_create_duplex_rtp_session(PAULINE_IP, PAULINE_RTP_PORT, PAULINE_RTCP_PORT, ms_factory_get_mtu(_factory));
	rtp_session_set_remote_addr_and_port(rtpSession_pauline, RELAY_IP, RELAY_PAULINE_RTP_PORT, RELAY_PAULINE_RTCP_PORT);
	rtp_session_set_profile(rtpSession_pauline, profile);
	rtp_session_enable_rtcp(rtpSession_pauline, FALSE);
	rtp_session_set_payload_type(rtpSession_pauline, PAULINE_PAYLOAD_TYPE);
	rtp_session_set_seq_number(rtpSession_pauline, 0xFFF8); // Pauline's ROC will change after 8 packets sent as we
	                                                        // start the session with a seqnumber near the max (0xFFFF)
	MSMediaStreamSessions pauline = {};
	pauline.rtp_session = rtpSession_pauline;

	/* set outer keys */
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&marielle, p.outerSuite, marielle_outer_key,
	                                                              MSSrtpKeySourceSDES) == 0);

	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&pauline, p.outerSuite, pauline_outer_key,
	                                                              MSSrtpKeySourceSDES) == 0);

	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&margaux, p.outerSuite, margaux_outer_key,
	                                                              MSSrtpKeySourceSDES) == 0);

	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&relay_margaux, p.outerSuite, margaux_outer_key,
	                                                              MSSrtpKeySourceSDES) == 0);
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&relay_marielle, p.outerSuite, marielle_outer_key,
	                                                              MSSrtpKeySourceSDES) == 0);
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&relay_pauline, p.outerSuite, pauline_outer_key,
	                                                              MSSrtpKeySourceSDES) == 0);

	/* set inner keys */
	if (p.useInnerEncryption) {
		if (p.useEkt) { // Just set the same EKT for all sources and recipient, they will generate keys and decrypt them
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_ekt(&marielle, &ekt_params) == 0);
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_ekt(&pauline, &ekt_params) == 0);
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_ekt(&margaux, &ekt_params) == 0);
			// when skipping pauline begins, we must force the EKT full tag to be present in every packets, not just the
			// first ones
			if (p.skipPaulineBegin) {
				BC_ASSERT_TRUE(ms_media_stream_sessions_set_ekt_full_tag_period(&pauline, 0) == 0);
			}
		} else { // set the inner keys
			// Marielle (even when 2 sessions are bundled from marielle, they will use the same key)
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_send_key_b64(
			                   &marielle, p.innerSuite, marielle_inner_key, MSSrtpKeySourceZRTP) == 0);
			// Pauline
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_send_key_b64(
			                   &pauline, p.innerSuite, pauline_inner_key, MSSrtpKeySourceZRTP) == 0);
			/* margaux inner keys are both set in margaux_marielle(attached to margaux MSMediaSessions)) rtpSession as
			 * it is the main one in the bundle, it is the one used to decrypt them all */
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_recv_key_b64(&margaux, p.innerSuite,
			                                                                    marielle_inner_key, MSSrtpKeySourceZRTP,
			                                                                    marielle.rtp_session->snd.ssrc) == 0);
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_recv_key_b64(&margaux, p.innerSuite,
			                                                                    pauline_inner_key, MSSrtpKeySourceZRTP,
			                                                                    pauline.rtp_session->snd.ssrc) == 0);
			if (p.useBundledSource) { // Marielle bundles two sessions, set the key for its SSRC too
				BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_recv_key_b64(
				                   &margaux, p.innerSuite, marielle_inner_key, MSSrtpKeySourceZRTP,
				                   rtpSession_marielle_bis->snd.ssrc) == 0);
			}
		}
	}

	ssize_t len = 0;
	uint8_t buffer[160];
	uint8_t xBuffer[160];
	uint8_t bBuffer[160];
	uint32_t user_ts = 0;
	int packet_sent = 0;
	bool error = false;
	/* read the whole file by chunk of 160 bytes */
	int pauline_last_packet_seqnum = -1;
	uint32_t marielleSSRC = 0;
	uint32_t mariellebisSSRC = 0;
	uint32_t paulineSSRC = 0;
	while ((error == false) && ((len = bctbx_file_read2(fp, buffer, 160)) > 0)) {
		/* marielle create a packet header */
		mblk_t *sent_packet = rtp_session_create_packet_header(rtpSession_marielle, 0);

		/* Marielle voice activity On, audio level -32 */
		if (p.useParticipantVolume) {
			rtp_add_client_to_mixer_audio_level(sent_packet, RTP_EXTENSION_CLIENT_TO_MIXER_AUDIO_LEVEL, TRUE, -32);
		}

		/* add payload */
		sent_packet->b_cont = rtp_create_packet(buffer, len);

		/* send the packet to the relay */
		int size = rtp_session_sendm_with_ts(rtpSession_marielle, copymsg(sent_packet), user_ts);
		if (size < 0) {
			ms_error("Session Marielle could not send the packet: -%x", -size);
			error = true;
			break;
		}
		freemsg(sent_packet);

		if (p.useBundledSource) { // Marielle bundles two sessions: send an other message on the secondary session
			for (int i = 0; i < len; i++) {
				bBuffer[i] = buffer[i] ^ 0x55;
			}
			sent_packet = rtp_session_create_packet_header(rtpSession_marielle_bis, 0);
			/* add payload */
			sent_packet->b_cont = rtp_create_packet(bBuffer, len);

			/* send the packet to the relay */
			size = rtp_session_sendm_with_ts(rtpSession_marielle_bis, copymsg(sent_packet), user_ts);
			if (size < 0) {
				ms_error("Session Marielle bis could not send the packet: -%x", -size);
				error = true;
				break;
			}
			freemsg(sent_packet);
		}

		/* pauline packet is the same data but Xor each byte with 0xaa */
		for (int i = 0; i < len; i++) {
			xBuffer[i] = buffer[i] ^ 0xaa;
		}
		sent_packet = rtp_session_create_packet_header(
		    rtpSession_pauline,
		    8); // allocate 8 bytes of extra header to store possible participant volume extension header

		/* Pauline voice activity Off, audio level -96 */
		if (p.useParticipantVolume) {
			rtp_add_client_to_mixer_audio_level(sent_packet, RTP_EXTENSION_CLIENT_TO_MIXER_AUDIO_LEVEL, FALSE, -96);
		}

		/* add payload */
		sent_packet->b_cont = rtp_create_packet(xBuffer, len);

		/* send the packet to the relay */
		size = rtp_session_sendm_with_ts(rtpSession_pauline, copymsg(sent_packet), user_ts);
		if (size < 0) {
			ms_error("Session Pauline could not send the packet: -%x", -size);
			error = true;
			break;
		}
		freemsg(sent_packet);

		/*** relay receive the packet from Marielle ***/
		/* this fetch will retrieve and decrypt packet from bundled session too is any */
		mblk_t *transfered_packet = rtp_session_recvm_with_ts(rtpSession_relay_marielle, user_ts);
		if (transfered_packet == NULL) {
			ms_error("Relay-Marielle session did not received any packets!");
			error = true;
			break;
		}

		/* Check that the packet available to the relay is encrypted (at least is different than the plain one) */
		uint8_t *payload;
		size = rtp_get_payload(transfered_packet, &payload);
		if (p.useInnerEncryption) {
			BC_ASSERT_FALSE(size == len);
			if (size == len) { // They shall not be the same size, but in that case, check they are differents
				error = true;
				BC_ASSERT_TRUE(memcmp(payload, buffer, len) != 0);
			}
		} else { // no inner encryption, relay can read the packet
			BC_ASSERT_TRUE(size == len);
			if (size == len) {
				BC_ASSERT_TRUE(memcmp(payload, buffer, len) == 0);
			}
		}
		/* check the packet payload type */
		uint16_t payload_type = rtp_get_payload_type(transfered_packet);
		BC_ASSERT_EQUAL(payload_type, MARIELLE_PAYLOAD_TYPE, uint16_t, "%d");

		/* forward the packet to Margaux */
		if (packet_sent == 0) {
			if (p.autodiscoverBundleSessions == false) {
				marielleSSRC = rtp_get_ssrc(transfered_packet);
				// session are not in auto-discovery, so we must set rtpSession_relay_margaux_marielle send ssrc to
				// match the sent packet SSRC
				rtp_session_set_ssrc(rtpSession_relay_margaux_marielle, marielleSSRC);
			}
		}

		size = rtp_session_sendm_with_ts(rtpSession_relay_margaux_marielle, copymsg(transfered_packet), user_ts);
		if (size < 0) {
			ms_error("Session Relay-Margaux-Marielle could not send the packet: -%x", -size);
			error = true;
			break;
		}
		freemsg(transfered_packet);

		if (p.useBundledSource) { // Marielle bundles two sessions so relay it too
			transfered_packet = rtp_session_recvm_with_ts(rtpSession_relay_marielle_bis, user_ts);
			if (transfered_packet == NULL) {
				ms_error("Relay-Marielle bis session did not received any packets!");
				error = true;
				break;
			}

			/* Check that the packet available to the relay is encrypted (at leat is different than the plain one */
			size = rtp_get_payload(transfered_packet, &payload);
			if (p.useInnerEncryption) {
				BC_ASSERT_FALSE(size == len);
				if (size == len) { // They shall not be the same size, but in that case, check they are differents
					error = true;
					BC_ASSERT_TRUE(memcmp(payload, bBuffer, len) != 0);
				}
			} else { // no inner encryption, relay can read the packet
				BC_ASSERT_TRUE(size == len);
				if (size == len) {
					BC_ASSERT_TRUE(memcmp(payload, bBuffer, len) == 0);
				}
			}

			/* check the packet payload type */
			payload_type = rtp_get_payload_type(transfered_packet);
			BC_ASSERT_EQUAL(payload_type, MARIELLE_PAYLOAD_TYPE, uint16_t, "%d");

			/* forward the packet to Margaux */
			if (packet_sent == 0 && p.autodiscoverBundleSessions == false) {
				mariellebisSSRC = rtp_get_ssrc(transfered_packet);
				// session are not in auto-discovery, so we must set rtpSession_relay_margaux_marielle_bis send ssrc to
				// match the sent packet SSRC
				rtp_session_set_ssrc(rtpSession_relay_margaux_marielle_bis, mariellebisSSRC);
			}

			if (p.autodiscoverBundleSessions == false || packet_sent > 0) {
				size = rtp_session_sendm_with_ts(rtpSession_relay_margaux_marielle_bis, copymsg(transfered_packet),
				                                 user_ts);
			} else {
				size =
				    rtp_session_sendm_with_ts(rtpSession_relay_margaux_marielle, copymsg(transfered_packet), user_ts);
			}
			if (size < 0) {
				ms_error("Session Relay-Margaux-Marielle-bis could not send the packet: -%x", -size);
				error = true;
				break;
			}
			freemsg(transfered_packet);
		}

		/*** relay receive the packet from Pauline ***/
		transfered_packet = rtp_session_recvm_with_ts(rtpSession_relay_pauline, user_ts);
		if (transfered_packet == NULL) {
			ms_error("Relay-Pauline session did not received any packets!");
			error = true;
			break;
		}

		/* Check that the packet available to the relay is encrypted (at leat is different than the plain one */
		size = rtp_get_payload(transfered_packet, &payload);
		if (p.useInnerEncryption) {
			BC_ASSERT_FALSE(size == len);
			if (size == len) { // They shall not be the same size, but in that case, check they are differents
				BC_ASSERT_TRUE(memcmp(payload, xBuffer, len) != 0);
				error = true;
			}
		} else { // no inner encryption, relay can read the packet
			BC_ASSERT_TRUE(size == len);
			if (size == len) {
				BC_ASSERT_TRUE(memcmp(payload, xBuffer, len) == 0);
			}
		}

		/* check the packet payload type */
		payload_type = rtp_get_payload_type(transfered_packet);
		BC_ASSERT_EQUAL(payload_type, PAULINE_PAYLOAD_TYPE, uint16_t, "%d");

		/* forward the packet to Margaux - except if skip begin or discard is active */
		if (packet_sent == 0 && p.autodiscoverBundleSessions == false) {
			paulineSSRC = rtp_get_ssrc(transfered_packet);
			// session are not in auto-discovery, so we must set rtpSession_relay_margaux_pauline send ssrc to match
			// the sent packet SSRC
			rtp_session_set_ssrc(rtpSession_relay_margaux_pauline, paulineSSRC);
		}

		if (packet_sent == 0 && p.autodiscoverBundleSessions == true) {
			size = rtp_session_sendm_with_ts(rtpSession_relay_margaux_marielle, copymsg(transfered_packet), user_ts);
			if (size < 0) {
				ms_error("Session Relay-Margaux-Pauline could not send the packet: -%x", -size);
				error = true;
				break;
			}
		} else if (!(p.skipPaulineBegin && packet_sent < 12) &&
		           !(p.discardPaulinePackets && packet_sent > 19 && packet_sent < 30)) {
			size = rtp_session_sendm_with_ts(rtpSession_relay_margaux_pauline, copymsg(transfered_packet), user_ts);
			if (size < 0) {
				ms_error("Session Relay-Margaux-Pauline could not send the packet: -%x", -size);
				error = true;
				break;
			}
		}
		freemsg(transfered_packet);

		/* margaux receive the packet from marielle
		 * This fetch will also retrieve and decrypt Pauline's session packet and get it ready to be fetched on
		 * margaux_pauline rtp session */

		if (packet_sent == 0 && !p.autodiscoverBundleSessions) {
			rtpSession_margaux_marielle->ssrc_set = TRUE;
			rtpSession_margaux_marielle->rcv.ssrc = marielleSSRC;
			if (p.useBundledSource) {
				rtpSession_margaux_marielle_bis->ssrc_set = TRUE;
				rtpSession_margaux_marielle_bis->rcv.ssrc = mariellebisSSRC;
			}
			rtpSession_margaux_pauline->ssrc_set = TRUE;
			rtpSession_margaux_pauline->rcv.ssrc = paulineSSRC;
		}
		mblk_t *received_packet = rtp_session_recvm_with_ts(rtpSession_margaux_marielle, user_ts);
		if (received_packet == NULL) {
			ms_error("Margaux session did not received any packets relayed from Marielle!");
			error = true;
			break;
		}

		/* Check the received payload is the same than the bytes reads from file */
		size = rtp_get_payload(received_packet, &payload);
		BC_ASSERT_TRUE(size == len);
		if (size == len) {
			BC_ASSERT_TRUE(memcmp(payload, buffer, len) == 0);
		}
		/* check participant volume */
		if (p.useParticipantVolume) {
			bool_t voice_activity;
			/* Marielle voice activity On, audio level -32 */
			BC_ASSERT_EQUAL(rtp_get_client_to_mixer_audio_level(
			                    received_packet, RTP_EXTENSION_CLIENT_TO_MIXER_AUDIO_LEVEL, &voice_activity),
			                -32, int, "%d");
			BC_ASSERT_EQUAL(voice_activity, TRUE, bool_t, "%d");
		}

		/* check the packet payload type */
		payload_type = rtp_get_payload_type(received_packet);
		BC_ASSERT_EQUAL(payload_type, RELAY_PAYLOAD_TYPE, uint16_t, "%d");

		freemsg(received_packet);

		if (p.useBundledSource) { // Marielle bundles two sessions, fetch this one too
			received_packet = rtp_session_recvm_with_ts(rtpSession_margaux_marielle_bis, user_ts);
			if (received_packet == NULL) {
				ms_error("Margaux session did not received any packets relayed from Marielle bis!");
				error = true;
				break;
			}

			/* Check the received payload is the same than the bytes reads from file */
			uint8_t *payload;
			size = rtp_get_payload(received_packet, &payload);
			BC_ASSERT_TRUE(size == len);
			if (size == len) {
				BC_ASSERT_TRUE(memcmp(payload, bBuffer, len) == 0);
			}

			/* check the packet payload type */
			payload_type = rtp_get_payload_type(received_packet);
			BC_ASSERT_EQUAL(payload_type, RELAY_PAYLOAD_TYPE, uint16_t, "%d");

			freemsg(received_packet);
		}

		/* margaux receive the packet from pauline - except if we skip pauline's begin or discard is active*/
		if (!(p.skipPaulineBegin && packet_sent < 12) &&
		    !(p.discardPaulinePackets && packet_sent > 19 && packet_sent < 30)) {
			received_packet = rtp_session_recvm_with_ts(rtpSession_margaux_pauline, user_ts);
			if (received_packet == NULL) {
				ms_error("Margaux session did not received any packets relayed from Pauline!");
				error = true;
				break;
			}

			/* Check the received payload is the same than the bytes reads from file */
			size = rtp_get_payload(received_packet, &payload);
			BC_ASSERT_TRUE(size == len);
			if (size == len) {
				BC_ASSERT_TRUE(memcmp(payload, xBuffer, len) == 0);
			}
			/* check participant volume */
			if (p.useParticipantVolume) {
				bool_t voice_activity;
				/* Pauline voice activity Off, audio level -96 */
				BC_ASSERT_EQUAL(rtp_get_client_to_mixer_audio_level(
				                    received_packet, RTP_EXTENSION_CLIENT_TO_MIXER_AUDIO_LEVEL, &voice_activity),
				                -96, int, "%d");
				BC_ASSERT_EQUAL(voice_activity, FALSE, bool_t, "%d");
			}

			/* check the packet payload type */
			payload_type = rtp_get_payload_type(received_packet);
			BC_ASSERT_EQUAL(payload_type, RELAY_PAYLOAD_TYPE, uint16_t, "%d");

			/* Make sure we receive a continuous flow of seq num - even in case of packet discarded by relay */
			auto packet_seqnum = rtp_get_seqnumber(received_packet);
			if (pauline_last_packet_seqnum != -1) {
				pauline_last_packet_seqnum = (pauline_last_packet_seqnum + 1) % 0x10000;
				BC_ASSERT_EQUAL((int)packet_seqnum, pauline_last_packet_seqnum, int, "%d");
			}
			pauline_last_packet_seqnum = packet_seqnum;
			freemsg(received_packet);
		}

		user_ts += 10;
		packet_sent++;
	}

	/* Check keys are correctly set, do it after the exchange as using ekt will set the keys while receiving the first
	 * packet */
	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&marielle, MediaStreamSendOnly, FALSE) ==
	               MSSrtpKeySourceSDES);
	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&pauline, MediaStreamSendOnly, FALSE) ==
	               MSSrtpKeySourceSDES);
	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&margaux, MediaStreamRecvOnly, FALSE) ==
	               MSSrtpKeySourceSDES);
	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_crypto_suite(&marielle, MediaStreamSendOnly, FALSE) ==
	               p.outerSuite);
	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_crypto_suite(&pauline, MediaStreamSendOnly, FALSE) ==
	               p.outerSuite);
	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_crypto_suite(&margaux, MediaStreamRecvOnly, FALSE) ==
	               p.outerSuite);

	if (p.useInnerEncryption) {
		BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_crypto_suite(&marielle, MediaStreamSendOnly, TRUE) ==
		               p.innerSuite);
		BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_crypto_suite(&pauline, MediaStreamSendOnly, TRUE) ==
		               p.innerSuite);
		BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_crypto_suite(&margaux, MediaStreamRecvOnly, TRUE) ==
		               p.innerSuite);
		if (p.useEkt) {
			BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&marielle, MediaStreamSendOnly, TRUE) ==
			               MSSrtpKeySourceEKT);
			BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&pauline, MediaStreamSendOnly, TRUE) ==
			               MSSrtpKeySourceEKT);
			BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&margaux, MediaStreamRecvOnly, TRUE) ==
			               MSSrtpKeySourceEKT);
		} else {
			BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&marielle, MediaStreamSendOnly, TRUE) ==
			               MSSrtpKeySourceZRTP);
			BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&pauline, MediaStreamSendOnly, TRUE) ==
			               MSSrtpKeySourceZRTP);
			BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&margaux, MediaStreamRecvOnly, TRUE) ==
			               MSSrtpKeySourceZRTP);
		}
	}

	BC_ASSERT_TRUE(error == false);

	/* cleaning */
	if (p.autodiscoverBundleSessions == true) {
		bctbx_list_free(margaux_sessions_list);
		bctbx_list_free(relay_sessions_list);
	}
	bctbx_file_close(fp);
	rtp_bundle_delete(rtpBundle_relay);
	rtp_bundle_delete(rtpBundle_margaux);
	if (p.useBundledSource) {
		rtp_bundle_delete(rtpBundle_marielle);
		rtp_bundle_delete(rtpBundle_relay_marielle);
		rtp_session_destroy(rtpSession_marielle_bis);
		rtp_session_destroy(rtpSession_relay_marielle_bis);
		if (rtpSession_relay_margaux_marielle_2) rtp_session_destroy(rtpSession_relay_margaux_marielle_2);
		rtp_session_destroy(rtpSession_relay_margaux_marielle_bis);
		rtp_session_destroy(rtpSession_margaux_marielle_bis);
	}
	ms_media_stream_sessions_uninit(&marielle);
	ms_media_stream_sessions_uninit(&margaux); // This will destroy rtpSession_margaux_marielle
	rtp_session_destroy(rtpSession_margaux_pauline);
	ms_media_stream_sessions_uninit(&pauline);
	ms_media_stream_sessions_uninit(&relay_marielle);
	ms_media_stream_sessions_uninit(&relay_margaux); // This will destroy rtpSession_relay_margaux_marielle
	rtp_session_destroy(rtpSession_relay_margaux_pauline);
	ms_media_stream_sessions_uninit(&relay_pauline);

	return error == false;
}
static void double_encrypted_relayed_data(void) {
	test_params p;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AES_128_SHA1_80;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
};

static void double_encrypted_relayed_data_with_volume(void) {
	test_params p;
	p.useParticipantVolume = true; // include participant volume info in RTP packet header
	/* use short bundle id: they will fit in the padding left by the volume info */
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AES_128_SHA1_80;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	/* use long bundle id: they will need a pullup with insert keeping current extension header */
	p.useLongBundleId = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AEAD_AES_256_GCM;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
};

static void double_encrypted_relayed_data_bundled_source(void) {
	test_params p;
	p.useBundledSource = true;
	// Bundle two streams in Marie source
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AES_128_SHA1_80;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));

	// include participant volumes (use short bundle Id)
	p.useParticipantVolume = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AEAD_AES_256_GCM;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));

	// include participant volumes (use long bundle Id)
	p.useLongBundleId = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AES_128_SHA1_80;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
}

static void double_encrypted_relayed_data_shared_mid(void) {
	test_params p;
	p.useSharedMid = true;
	// usedSharedMid implies bundled source and long bundle Id
	// explicitely set them anyway
	p.useParticipantVolume = true;
	p.useLongBundleId = true;
	p.useBundledSource = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	// with EKT and discard packets
	p.useEkt = true;
	p.discardPaulinePackets = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
}

static void double_encrypted_relayed_data_autodiscoverd_bundled_sessions(void) {
	test_params p;
	p.autodiscoverBundleSessions = true;
	// usedSharedMid implies shared Mid, Ekt, bundled source and long bundle Id
	// explicitely set them anyway
	p.useSharedMid = true;
	p.useParticipantVolume = true;
	p.useLongBundleId = true;
	p.useBundledSource = true;
	p.useEkt = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AES_128_SHA1_80;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
}

static void double_encrypted_relayed_data_discarded_packets(void) {
	test_params p;
	p.discardPaulinePackets = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AES_128_SHA1_80;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
}

static void double_encrypted_relayed_data_discarded_packets_with_ekt_volume_and_bundled_source(void) {
	test_params p;
	p.useEkt = true;
	p.discardPaulinePackets = true;
	p.useParticipantVolume = true;
	p.useLongBundleId = true;
	p.useBundledSource = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AES_128_SHA1_80;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
}

static void double_encrypted_relayed_data_use_ekt(void) {
	test_params p;
	p.useEkt = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AES_128_SHA1_80;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
};

static void double_encrypted_relayed_data_with_volume_use_ekt(void) {
	test_params p;
	p.useEkt = true;
	p.useParticipantVolume = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AES_128_SHA1_80;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));

	p.useLongBundleId = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AEAD_AES_256_GCM;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
};

static void double_encrypted_relayed_data_bundled_source_use_ekt(void) {
	test_params p;
	p.useEkt = true;
	p.useBundledSource = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AES_128_SHA1_80;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));

	p.useParticipantVolume = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AEAD_AES_256_GCM;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));

	p.useLongBundleId = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AES_128_SHA1_80;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
};

static void double_encrypted_relayed_data_use_ekt_skip_init_ROC(void) {
	test_params p;
	p.useEkt = true;
	p.skipPaulineBegin = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.innerSuite = MS_AES_128_SHA1_80;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
};

static void simple_encrypted_relayed_data(void) {
	test_params p;
	p.useInnerEncryption = false;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.discardPaulinePackets = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
	p.autodiscoverBundleSessions = true;
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(p));
}

static test_t tests[] = {
    TEST_NO_TAG("Double Encrypted relayed data", double_encrypted_relayed_data),
    TEST_NO_TAG("Double Encrypted relayed data with volume info", double_encrypted_relayed_data_with_volume),
    TEST_NO_TAG("Double Encrypted relayed data bundled source", double_encrypted_relayed_data_bundled_source),
    TEST_NO_TAG("Double Encrypted relayed data with packets discarded by relay",
                double_encrypted_relayed_data_discarded_packets),
    TEST_NO_TAG("Double Encrypted relayed data with ekt", double_encrypted_relayed_data_use_ekt),
    TEST_NO_TAG("Double Encrypted relayed data with volume info and ekt",
                double_encrypted_relayed_data_with_volume_use_ekt),
    TEST_NO_TAG("Double Encrypted relayed data with bundled source and ekt",
                double_encrypted_relayed_data_bundled_source_use_ekt),
    TEST_NO_TAG("Double Encrypted relayed data with ekt, skip initial ROC",
                double_encrypted_relayed_data_use_ekt_skip_init_ROC),
    TEST_NO_TAG("Double Encrypted relayed data with ekt, packets discarded by relay",
                double_encrypted_relayed_data_discarded_packets_with_ekt_volume_and_bundled_source),
    TEST_NO_TAG("Double Encrypted relayed data shared MID in bundle", double_encrypted_relayed_data_shared_mid),
    TEST_NO_TAG("Double Encrypted relayed data autodiscovered bundled sessions",
                double_encrypted_relayed_data_autodiscoverd_bundled_sessions),
    TEST_NO_TAG("Simple Encrypted relayed data", simple_encrypted_relayed_data),
};

test_suite_t double_encryption_test_suite = {"RTP Data Double Encryption",
                                             tester_before_all,
                                             tester_after_all,
                                             NULL,
                                             NULL,
                                             sizeof(tests) / sizeof(tests[0]),
                                             tests};
