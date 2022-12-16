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

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/mstonedetector.h"
#include "mediastreamer2/msvolume.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"
#include "mediastreamer2/msutils.h"
#include "bctoolbox/vfs_standard.h"

#define HELLO_8K_1S_FILE  "sounds/hello8000-1s.wav"

static RtpProfile rtp_profile;
static MSFactory *_factory= NULL;


static int tester_before_all(void) {
	//ms_init();
	_factory = ms_factory_new();
	ms_factory_init_voip(_factory);
	ms_factory_init_plugins(_factory);

	ms_factory_enable_statistics(_factory, TRUE);
	ortp_init();
	rtp_profile_set_payload (&rtp_profile,0,&payload_type_pcmu8000);
	return 0;
}

static int tester_after_all(void) {
	//ms_exit();

	ms_factory_destroy(_factory);
	rtp_profile_clear_all(&rtp_profile);
	return 0;
}

#define MARIELLE_RTP_PORT 2564
#define MARIELLE_RTCP_PORT 2565
#define MARIELLE_IP "127.0.0.1"

#define MARGAUX_RTP_PORT 9864
#define MARGAUX_RTCP_PORT 9865
#define MARGAUX_IP "127.0.0.1"

#define PAULINE_RTP_PORT 9868
#define PAULINE_RTCP_PORT 9869
#define PAULINE_IP "127.0.0.1"

/* Relay gets three pairs of port one for each correspondant */
#define RELAY_MARIELLE_RTP_PORT 9874
#define RELAY_MARIELLE_RTCP_PORT 9875
#define RELAY_MARGAUX_RTP_PORT 9876
#define RELAY_MARGAUX_RTCP_PORT 9877
#define RELAY_PAULINE_RTP_PORT 9878
#define RELAY_PAULINE_RTCP_PORT 9879
#define RELAY_IP "127.0.0.1"

/* identify streams in bundle */
/* short ID header extension fit in 2 bytes, which means they can be added in the padding space left by audio level extension */
#define SHORT_MID_MARIELLE_SESSION "m"
#define SHORT_MID_MARIELLE_SESSION_BIS "n"
#define SHORT_MID_PAULINE_SESSION "p"
#define SHORT_MID_MARIELLE_SOURCE_SESSION "a"
#define SHORT_MID_MARIELLE_SOURCE_SESSION_BIS "b"
/* long ID header extension fit in 8 or 9 bytes, which means they cannot fit in any padding and request the allocation of more space */
#define LONG_MID_MARIELLE_SESSION "marielle"
#define LONG_MID_MARIELLE_SESSION_BIS "marielle_bis"
#define LONG_MID_PAULINE_SESSION "pauline"
#define LONG_MID_MARIELLE_SOURCE_SESSION "Marielle"
#define LONG_MID_MARIELLE_SOURCE_SESSION_BIS "MarielleBis"


static bool_t double_encrypted_rtp_relay_data_base(
					MSCryptoSuite outer_suite,
					MSCryptoSuite inner_suite,
					bool participant_volume=false,
					bool use_long_bundle_id=false,
					bool bundled_source=false,
					bool use_ekt = false) {
	if (!ms_srtp_supported()) {
		ms_warning("srtp not available, skiping...");
		return TRUE;
	}

	char* hello_file = bc_tester_res(HELLO_8K_1S_FILE);
	bctbx_vfs_file_t *fp = bctbx_file_open(&bcStandardVfs, hello_file, "r");
	bc_free(hello_file);

	RtpProfile* profile = rtp_profile_new("default profile");
	MSEKTParametersSet ekt_params;
	if (use_ekt) {
		uint8_t master_salt[14] = {0x01, 0x10, 0x11, 0x04, 0x40, 0x44, 0x07, 0x70, 0x77, 0x0a, 0xa0, 0xaa, 0xf0, 0x0f}; // 14 bytes master salt even if we may end up using only 12 bytes
		// 32 bytes EKT key value even if we may endup using only 16
		uint8_t key_value[32] = {0x23, 0xd4, 0x19, 0x12, 0x7e, 0x85, 0xa3, 0x14, 0xa8, 0x47, 0x71, 0x2d, 0x04, 0x3c, 0x31, 0x50, 0xad, 0x2d, 0x16, 0x97, 0xa1, 0x60, 0x41, 0xe4, 0xc5, 0xec, 0x78, 0xc1, 0xdf, 0x99, 0xb8, 0xd9};
		MSEKTCipherType ekt_cipher = MS_EKT_CIPHERTYPE_AESKW128;
		if (inner_suite == MS_AES_256_SHA1_80 || inner_suite == MS_AES_256_SHA1_32 || inner_suite == MS_AEAD_AES_256_GCM)  {
			ekt_cipher = MS_EKT_CIPHERTYPE_AESKW256;
		}
		ekt_params.ekt_cipher_type = ekt_cipher;
		ekt_params.ekt_srtp_crypto_suite = inner_suite;
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

	switch (outer_suite) {
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

	switch (inner_suite) {
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

	rtp_profile_set_payload (profile,0,&payload_type_pcmu8000);

	/* Margaux is the final recipient: build 2 rtpsession, they will be bundled  */
	/* First session is created duplex just because it is easier, it is used in RECV only */
	RtpSession *rtpSession_margaux_marielle = ms_create_duplex_rtp_session(MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT, ms_factory_get_mtu(_factory));
	rtp_session_enable_jitter_buffer(rtpSession_margaux_marielle, FALSE); // Disable jitter buffer for the final recipient, we want to get data when they arrive, we're assuming no loss
	rtp_session_enable_rtcp(rtpSession_margaux_marielle, FALSE);

	/* Second session, in RECV only, is bundled so we do not need to define local port */
	RtpSession *rtpSession_margaux_pauline = rtp_session_new(RTP_SESSION_RECVONLY);
	rtp_session_set_recv_buf_size(rtpSession_margaux_pauline, MAX(ms_factory_get_mtu(_factory) , 1500));
	rtp_session_enable_jitter_buffer(rtpSession_margaux_pauline, FALSE); // Disable jitter buffer for the final recipient, we want to get data when they arrive, we're assuming no loss
	rtp_session_enable_rtcp(rtpSession_margaux_pauline, FALSE);

	RtpSession *rtpSession_margaux_marielle_bis = NULL;
	if (bundled_source) { // Marielle source bundles two sessions so margaux receives 3
		/* Third session, in RECV only, is bundled so we do not need to define local port */
		rtpSession_margaux_marielle_bis = rtp_session_new(RTP_SESSION_RECVONLY);
		rtp_session_set_recv_buf_size(rtpSession_margaux_marielle_bis, MAX(ms_factory_get_mtu(_factory) , 1500));
		rtp_session_enable_jitter_buffer(rtpSession_margaux_marielle_bis, FALSE); // Disable jitter buffer for the final recipient, we want to get data when they arrive, we're assuming no loss
		rtp_session_enable_rtcp(rtpSession_margaux_marielle_bis, FALSE);
	}

	/* create a bundle, margaux_marielle is the main session */
	RtpBundle *rtpBundle_margaux = rtp_bundle_new();
	if (use_long_bundle_id) {
		rtp_bundle_add_session(rtpBundle_margaux, LONG_MID_MARIELLE_SESSION, rtpSession_margaux_marielle);
		rtp_bundle_add_session(rtpBundle_margaux, LONG_MID_PAULINE_SESSION, rtpSession_margaux_pauline);
		if (bundled_source) { // Marielle source bundles two sessions so margaux receives 3
			rtp_bundle_add_session(rtpBundle_margaux, LONG_MID_MARIELLE_SESSION_BIS, rtpSession_margaux_marielle_bis);
		}
	} else {
		rtp_bundle_add_session(rtpBundle_margaux, SHORT_MID_MARIELLE_SESSION, rtpSession_margaux_marielle);
		rtp_bundle_add_session(rtpBundle_margaux, SHORT_MID_PAULINE_SESSION, rtpSession_margaux_pauline);
		if (bundled_source) { // Marielle source bundles two sessions so margaux receives 3
			rtp_bundle_add_session(rtpBundle_margaux, SHORT_MID_MARIELLE_SESSION_BIS, rtpSession_margaux_marielle_bis);
		}
	}
	rtp_bundle_set_mid_extension_id(rtpBundle_margaux, RTP_EXTENSION_MID);
	MSMediaStreamSessions margaux;
	margaux.rtp_session = rtpSession_margaux_marielle;
	margaux.srtp_context = NULL;
	margaux.zrtp_context = NULL;
	margaux.dtls_context = NULL;
	margaux.ticker = NULL;

	/* the relay needs to open rtp session with all endpoints, 2 bundled sessions for margaux. Relay's RtpSession are all in transfer mode  */
	// relay_margaux: one main session created as duplex just because it is easier
	RtpSession *rtpSession_relay_margaux_marielle = ms_create_duplex_rtp_session(RELAY_IP, RELAY_MARGAUX_RTP_PORT, RELAY_MARGAUX_RTCP_PORT, ms_factory_get_mtu(_factory));
	rtp_session_set_remote_addr_and_port(rtpSession_relay_margaux_marielle, MARGAUX_IP, MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT);
	rtp_session_enable_transfer_mode(rtpSession_relay_margaux_marielle, TRUE); // relay rtp session is in transfer mode
	rtp_session_enable_rtcp(rtpSession_relay_margaux_marielle, FALSE);
	// relay_margaux: secondary session in the bundle, minimal settings
	RtpSession *rtpSession_relay_margaux_pauline = rtp_session_new(RTP_SESSION_SENDONLY);
	rtp_session_enable_transfer_mode(rtpSession_relay_margaux_pauline, TRUE); // relay rtp session is in transfer mode
	rtp_session_enable_rtcp(rtpSession_relay_margaux_pauline, FALSE);

	RtpSession *rtpSession_relay_margaux_marielle_bis = NULL;
	if (bundled_source) { // Marielle source bundles two sessions so margaux receives 3
		rtpSession_relay_margaux_marielle_bis = rtp_session_new(RTP_SESSION_SENDONLY);
		rtp_session_enable_transfer_mode(rtpSession_relay_margaux_marielle_bis, TRUE); // relay rtp session is in transfer mode	
		rtp_session_enable_rtcp(rtpSession_relay_margaux_marielle_bis, FALSE);
	}

	/* create a bundle, margaux_marielle is the main session */
	RtpBundle *rtpBundle_relay = rtp_bundle_new();
	if (use_long_bundle_id) {
		rtp_bundle_add_session(rtpBundle_relay, LONG_MID_MARIELLE_SESSION, rtpSession_relay_margaux_marielle);
		rtp_bundle_add_session(rtpBundle_relay, LONG_MID_PAULINE_SESSION, rtpSession_relay_margaux_pauline);
		if (bundled_source) { // Marielle source bundles two sessions so margaux receives 3
			rtp_bundle_add_session(rtpBundle_relay, LONG_MID_MARIELLE_SESSION_BIS, rtpSession_relay_margaux_marielle_bis);
		}
	} else {
		rtp_bundle_add_session(rtpBundle_relay, SHORT_MID_MARIELLE_SESSION, rtpSession_relay_margaux_marielle);
		rtp_bundle_add_session(rtpBundle_relay, SHORT_MID_PAULINE_SESSION, rtpSession_relay_margaux_pauline);
		if (bundled_source) { // Marielle source bundles two sessions so margaux receives 3
			rtp_bundle_add_session(rtpBundle_relay, SHORT_MID_MARIELLE_SESSION_BIS, rtpSession_relay_margaux_marielle_bis);
		}
	}
	rtp_bundle_set_mid_extension_id(rtpBundle_relay, RTP_EXTENSION_MID);
	MSMediaStreamSessions relay_margaux;
	relay_margaux.rtp_session = rtpSession_relay_margaux_marielle;
	relay_margaux.srtp_context = NULL;
	relay_margaux.zrtp_context = NULL;
	relay_margaux.dtls_context = NULL;
	relay_margaux.ticker = NULL;
	if (use_ekt) {
		ms_media_stream_sessions_set_ekt_mode(&relay_margaux, MS_EKT_TRANSFER);
	}

	// marielle_relay: session used in recv only but created in duplex just because it is easier
	RtpSession *rtpSession_relay_marielle = ms_create_duplex_rtp_session(RELAY_IP, RELAY_MARIELLE_RTP_PORT, RELAY_MARIELLE_RTCP_PORT, ms_factory_get_mtu(_factory));
	rtp_session_enable_transfer_mode(rtpSession_relay_marielle, TRUE); // relay rtp session is in transfer mode
	rtp_session_enable_rtcp(rtpSession_relay_marielle, FALSE);
	MSMediaStreamSessions relay_marielle;
	relay_marielle.rtp_session = rtpSession_relay_marielle;
	relay_marielle.srtp_context = NULL;
	relay_marielle.zrtp_context = NULL;
	relay_marielle.dtls_context = NULL;
	relay_marielle.ticker = NULL;
	if (use_ekt) {
		ms_media_stream_sessions_set_ekt_mode(&relay_marielle, MS_EKT_TRANSFER);
	}
	RtpSession *rtpSession_relay_marielle_bis = NULL;
	RtpBundle *rtpBundle_relay_marielle = NULL;
	if (bundled_source) { // Marielle bundles two sessions so relay must get ready for it
		rtpSession_relay_marielle_bis = rtp_session_new(RTP_SESSION_RECVONLY);
		rtp_session_set_payload_type(rtpSession_relay_marielle_bis, 0);
		rtp_session_enable_transfer_mode(rtpSession_relay_marielle_bis, TRUE); // relay rtp session is in transfer mode
		rtp_session_enable_rtcp(rtpSession_relay_marielle_bis, FALSE);
		rtpBundle_relay_marielle = rtp_bundle_new();
		if (use_long_bundle_id) {
			rtp_bundle_add_session(rtpBundle_relay_marielle, LONG_MID_MARIELLE_SOURCE_SESSION, rtpSession_relay_marielle);
			rtp_bundle_add_session(rtpBundle_relay_marielle, LONG_MID_MARIELLE_SOURCE_SESSION_BIS, rtpSession_relay_marielle_bis);
		} else {
			rtp_bundle_add_session(rtpBundle_relay_marielle, SHORT_MID_MARIELLE_SOURCE_SESSION, rtpSession_relay_marielle);
			rtp_bundle_add_session(rtpBundle_relay_marielle, SHORT_MID_MARIELLE_SOURCE_SESSION_BIS, rtpSession_relay_marielle_bis);
		}
		rtp_bundle_set_mid_extension_id(rtpBundle_relay_marielle, RTP_EXTENSION_MID);
	}


	// pauline_relay: session used in recv only but created in duplex just because it is easier
	RtpSession *rtpSession_relay_pauline = ms_create_duplex_rtp_session(RELAY_IP, RELAY_PAULINE_RTP_PORT, RELAY_PAULINE_RTCP_PORT, ms_factory_get_mtu(_factory));
	rtp_session_enable_transfer_mode(rtpSession_relay_pauline, TRUE); // relay rtp session is in transfer mode
	rtp_session_enable_rtcp(rtpSession_relay_pauline, FALSE);
	MSMediaStreamSessions relay_pauline;
	relay_pauline.rtp_session = rtpSession_relay_pauline;
	relay_pauline.srtp_context = NULL;
	relay_pauline.zrtp_context = NULL;
	relay_pauline.dtls_context = NULL;
	relay_pauline.ticker = NULL;
	if (use_ekt) {
		ms_media_stream_sessions_set_ekt_mode(&relay_pauline, MS_EKT_TRANSFER);
	}

	/* Marielle is a source */
	RtpSession *rtpSession_marielle = ms_create_duplex_rtp_session(MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT, ms_factory_get_mtu(_factory));
	rtp_session_set_profile(rtpSession_marielle, profile);
	rtp_session_set_remote_addr_and_port(rtpSession_marielle, RELAY_IP, RELAY_MARIELLE_RTP_PORT, RELAY_MARIELLE_RTCP_PORT);
	rtp_session_set_payload_type(rtpSession_marielle, 0);
	rtp_session_enable_rtcp(rtpSession_marielle, FALSE);
	MSMediaStreamSessions marielle;
	marielle.rtp_session = rtpSession_marielle;
	marielle.srtp_context = NULL;
	marielle.zrtp_context = NULL;
	marielle.dtls_context = NULL;
	marielle.ticker = NULL;
	RtpSession *rtpSession_marielle_bis = NULL;
	RtpBundle *rtpBundle_marielle = NULL;
	if (bundled_source) { // Marielle bundles two sessions
		rtpSession_marielle_bis = rtp_session_new(RTP_SESSION_SENDONLY);
		rtp_session_set_payload_type(rtpSession_marielle_bis, 0);
		rtp_session_enable_rtcp(rtpSession_marielle_bis, FALSE);
		rtpBundle_marielle = rtp_bundle_new();
		if (use_long_bundle_id) {
			rtp_bundle_add_session(rtpBundle_marielle, LONG_MID_MARIELLE_SOURCE_SESSION, rtpSession_marielle);
			rtp_bundle_add_session(rtpBundle_marielle, LONG_MID_MARIELLE_SOURCE_SESSION_BIS, rtpSession_marielle_bis);
		} else {
			rtp_bundle_add_session(rtpBundle_marielle, SHORT_MID_MARIELLE_SOURCE_SESSION, rtpSession_marielle);
			rtp_bundle_add_session(rtpBundle_marielle, SHORT_MID_MARIELLE_SOURCE_SESSION_BIS, rtpSession_marielle_bis);
		}
		rtp_bundle_set_mid_extension_id(rtpBundle_marielle, RTP_EXTENSION_MID);
	}

	/* Pauline is a source */
	RtpSession *rtpSession_pauline = ms_create_duplex_rtp_session(PAULINE_IP, PAULINE_RTP_PORT, PAULINE_RTCP_PORT, ms_factory_get_mtu(_factory));
	rtp_session_set_profile(rtpSession_pauline, profile);
	rtp_session_set_remote_addr_and_port(rtpSession_pauline, RELAY_IP, RELAY_PAULINE_RTP_PORT, RELAY_PAULINE_RTCP_PORT);
	rtp_session_set_payload_type(rtpSession_pauline, 0);
	rtp_session_enable_rtcp(rtpSession_pauline, FALSE);
	MSMediaStreamSessions pauline;
	pauline.rtp_session = rtpSession_pauline;
	pauline.srtp_context = NULL;
	pauline.zrtp_context = NULL;
	pauline.dtls_context = NULL;
	pauline.ticker = NULL;

	/* set marielle send keys: inner and outer */
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&marielle, outer_suite, marielle_outer_key, MSSrtpKeySourceSDES) == 0);

	/* set pauline send keys: inner and outer */
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&pauline, outer_suite, pauline_outer_key, MSSrtpKeySourceSDES) == 0);

	/* set margaux recv keys: outer and inners matching marielle and pauline */
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&margaux, outer_suite, margaux_outer_key, MSSrtpKeySourceSDES) == 0);

	/* set the relay outer keys for all sessions */
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_send_key_b64(&relay_margaux, outer_suite, margaux_outer_key, MSSrtpKeySourceSDES) == 0);
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&relay_marielle, outer_suite, marielle_outer_key, MSSrtpKeySourceSDES) == 0);
	BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_recv_key_b64(&relay_pauline, outer_suite, pauline_outer_key, MSSrtpKeySourceSDES) == 0);

	/* set inner keys */
	if (use_ekt) { // Just set the same EKT for all sources and recipient, they will generate keys and decrypt them
		BC_ASSERT_TRUE(ms_media_stream_sessions_set_send_ekt(&marielle, &ekt_params) == 0);
		BC_ASSERT_TRUE(ms_media_stream_sessions_set_send_ekt(&pauline, &ekt_params) == 0);
		BC_ASSERT_TRUE(ms_media_stream_sessions_add_recv_ekt(&margaux, &ekt_params) == 0);
	} else { // set the inner keys
		// Marielle (even when 2 sessions are bundled from marielle, they will use the same key)
		BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_send_key_b64(&marielle, inner_suite, marielle_inner_key, MSSrtpKeySourceZRTP) == 0);
		// Pauline
		BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_send_key_b64(&pauline, inner_suite, pauline_inner_key, MSSrtpKeySourceZRTP) == 0);
		/* margaux inner keys are both set in margaux_marielle(attached to margaux MSMediaSessions)) rtpSession as it is the main one in the bundle, it is the one used to decrypt them all */
		BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_recv_key_b64(&margaux, inner_suite, marielle_inner_key, MSSrtpKeySourceZRTP, marielle.rtp_session->snd.ssrc) == 0);
		BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_recv_key_b64(&margaux, inner_suite, pauline_inner_key, MSSrtpKeySourceZRTP, pauline.rtp_session->snd.ssrc) == 0);
		if (bundled_source) { // Marielle bundles two sessions, set the key for its SSRC too
			BC_ASSERT_TRUE(ms_media_stream_sessions_set_srtp_inner_recv_key_b64(&margaux, inner_suite, marielle_inner_key, MSSrtpKeySourceZRTP, rtpSession_marielle_bis->snd.ssrc) == 0);
		}
	}

	ssize_t len = 0;
	uint8_t buffer[160];
	uint8_t xBuffer[160];
	uint8_t bBuffer[160];
	uint32_t user_ts = 0;
	bool error = false;
	/* read the whole file by chunk of 160 bytes */
	while ( (error == false) && ((len = bctbx_file_read2(fp, buffer, 160)) > 0) ) {
		/* marielle create a packet with the chunk */
		mblk_t *sent_packet = rtp_session_create_packet(rtpSession_marielle, 0, buffer, len);

		/* Marielle voice activity On, audio level -32 */
		if(participant_volume) {
			rtp_add_client_to_mixer_audio_level(sent_packet, RTP_EXTENSION_CLIENT_TO_MIXER_AUDIO_LEVEL, TRUE, -32);
		}

		/* send the packet to the relay */
		int size = rtp_session_sendm_with_ts(rtpSession_marielle, copymsg(sent_packet), user_ts);
		if (size < 0) {
			ms_error("Session Marielle could not send the packet: -%x", -size);
			error = true;
			break;
		}
		freemsg(sent_packet);

		if (bundled_source) { // Marielle bundles two sessions: send an other message on the secondary session
			for (int i=0; i<len; i++) {
				bBuffer[i] = buffer[i]^0x55;
			}
			sent_packet = rtp_session_create_packet(rtpSession_marielle_bis, 0, bBuffer, len);
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
		for (int i=0; i<len; i++) {
			xBuffer[i] = buffer[i]^0xaa;
		}
		sent_packet = rtp_session_create_packet(rtpSession_pauline, 0, xBuffer, len);

		/* Pauline voice activity Off, audio level -96 */
		if(participant_volume) {
			rtp_add_client_to_mixer_audio_level(sent_packet, RTP_EXTENSION_CLIENT_TO_MIXER_AUDIO_LEVEL, FALSE, -96);
		}

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

		/* Check that the packet available to the relay is encrypted (at leat is different than the plain one */
		uint8_t *payload;
		size = rtp_get_payload(transfered_packet, &payload);
		BC_ASSERT_NOT_EQUAL((ssize_t)size, len, ssize_t, "%ld");
		if (size == len) { // They shall not be the same size, but in that case, check they are differents
			BC_ASSERT_TRUE(memcmp(payload, buffer, len) != 0);
		}

		/* forward the packet to Margaux */
		size = rtp_session_sendm_with_ts(rtpSession_relay_margaux_marielle, copymsg(transfered_packet), user_ts);
		if (size < 0) {
			ms_error("Session Relay-Margaux-Marielle could not send the packet: -%x", -size);
			error = true;
			break;
		}
		freemsg(transfered_packet);

		if (bundled_source) { // Marielle bundles two sessions so relay it too
			transfered_packet = rtp_session_recvm_with_ts(rtpSession_relay_marielle_bis, user_ts);
			if (transfered_packet == NULL) {
				ms_error("Relay-Marielle bis session did not received any packets!");
				error = true;
				break;
			}

			/* Check that the packet available to the relay is encrypted (at leat is different than the plain one */
			size = rtp_get_payload(transfered_packet, &payload);
			BC_ASSERT_NOT_EQUAL((ssize_t)size, len, ssize_t, "%ld");
			if (size == len) { // They shall not be the same size, but in that case, check they are differents
				BC_ASSERT_TRUE(memcmp(payload, bBuffer, len) != 0);
			}

			/* forward the packet to Margaux */
			size = rtp_session_sendm_with_ts(rtpSession_relay_margaux_marielle_bis, copymsg(transfered_packet), user_ts);
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
		BC_ASSERT_NOT_EQUAL((ssize_t)size, len, ssize_t, "%ld");
		if (size == len) { // They shall not be the same size, but in that case, check they are differents
			BC_ASSERT_TRUE(memcmp(payload, xBuffer, len) != 0);
		}

		/* forward the packet to Margaux */
		size = rtp_session_sendm_with_ts(rtpSession_relay_margaux_pauline, copymsg(transfered_packet), user_ts);
		if (size < 0) {
			ms_error("Session Relay-Margaux-Pauline could not send the packet: -%x", -size);
			error = true;
			break;
		}
		freemsg(transfered_packet);

		/* margaux receive the packet from marielle
		 * This fetch will also retrieve and decrypt Pauline's session packet and get it ready to be fetched on margaux_pauline rtp session */
		mblk_t *received_packet = rtp_session_recvm_with_ts(rtpSession_margaux_marielle, user_ts);
		if (received_packet == NULL) {
			ms_error("Margaux session did not received any packets relayed from Marielle!");
			error = true;
			break;
		}

		/* Check the received payload is the same than the bytes reads from file */
		size = rtp_get_payload(received_packet, &payload);
		BC_ASSERT_EQUAL((ssize_t)size, len, ssize_t, "%ld");
		if (size == len) {
			BC_ASSERT_TRUE(memcmp(payload, buffer, len) == 0);
		}
		/* check participant volume */
		if(participant_volume) {
			bool_t voice_activity;
			/* Marielle voice activity On, audio level -32 */
			BC_ASSERT_EQUAL(rtp_get_client_to_mixer_audio_level(received_packet, RTP_EXTENSION_CLIENT_TO_MIXER_AUDIO_LEVEL, &voice_activity), -32, int, "%d");
			BC_ASSERT_EQUAL(voice_activity, TRUE, bool_t, "%d");
		}
		freemsg(received_packet);

		if (bundled_source) { // Marielle bundles two sessions, fetch this one too
			received_packet = rtp_session_recvm_with_ts(rtpSession_margaux_marielle_bis, user_ts);
			if (received_packet == NULL) {
				ms_error("Margaux session did not received any packets relayed from Marielle bis!");
				error = true;
				break;
			}

			/* Check the received payload is the same than the bytes reads from file */
			uint8_t *payload;
			size = rtp_get_payload(received_packet, &payload);
			BC_ASSERT_EQUAL((ssize_t)size, len, ssize_t, "%ld");
			if (size == len) {
				BC_ASSERT_TRUE(memcmp(payload, bBuffer, len) == 0);
			}
			freemsg(received_packet);
		}

		/* margaux receive the packet from pauline */
		received_packet = rtp_session_recvm_with_ts(rtpSession_margaux_pauline, user_ts);
		if (received_packet == NULL) {
			ms_error("Margaux session did not received any packets relayed from Pauline!");
			error = true;
			break;
		}

		/* Check the received payload is the same than the bytes reads from file */
		size = rtp_get_payload(received_packet, &payload);
		BC_ASSERT_EQUAL((ssize_t)size, len, ssize_t, "%ld");
		if (size == len) {
			BC_ASSERT_TRUE(memcmp(payload, xBuffer, len) == 0);
		}
		/* check participant volume */
		if(participant_volume) {
			bool_t voice_activity;
			/* Pauline voice activity Off, audio level -96 */
			BC_ASSERT_EQUAL(rtp_get_client_to_mixer_audio_level(received_packet, RTP_EXTENSION_CLIENT_TO_MIXER_AUDIO_LEVEL, &voice_activity), -96, int, "%d");
			BC_ASSERT_EQUAL(voice_activity, FALSE, bool_t, "%d");
		}
		freemsg(received_packet);

		user_ts += 10;
	}

	/* Check keys are correctly set, do it after the exchange as using ekt will set the keys while receiving the first packet */
	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&marielle, MediaStreamSendOnly, FALSE)==MSSrtpKeySourceSDES);
	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&pauline, MediaStreamSendOnly, FALSE)==MSSrtpKeySourceSDES);
	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&margaux, MediaStreamRecvOnly, FALSE)==MSSrtpKeySourceSDES);
	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_crypto_suite(&marielle, MediaStreamSendOnly, FALSE)==outer_suite);
	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_crypto_suite(&pauline, MediaStreamSendOnly, FALSE)==outer_suite);
	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_crypto_suite(&margaux, MediaStreamRecvOnly, FALSE)==outer_suite);

	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_crypto_suite(&marielle, MediaStreamSendOnly, TRUE)==inner_suite);
	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_crypto_suite(&pauline, MediaStreamSendOnly, TRUE)==inner_suite);
	BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_crypto_suite(&margaux, MediaStreamRecvOnly, TRUE)==inner_suite);
	if (use_ekt) { // Just set the same EKT for all sources and recipient, they will generate keys and decrypt them
		BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&marielle, MediaStreamSendOnly, TRUE)==MSSrtpKeySourceEKT);
		BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&pauline, MediaStreamSendOnly, TRUE)==MSSrtpKeySourceEKT);
		BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&margaux, MediaStreamRecvOnly, TRUE)==MSSrtpKeySourceEKT);
	} else {
		BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&marielle, MediaStreamSendOnly, TRUE)==MSSrtpKeySourceZRTP);
		BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&pauline, MediaStreamSendOnly, TRUE)==MSSrtpKeySourceZRTP);
		BC_ASSERT_TRUE(ms_media_stream_sessions_get_srtp_key_source(&margaux, MediaStreamRecvOnly, TRUE)==MSSrtpKeySourceZRTP);
	}


	BC_ASSERT_TRUE(error == false);

	/* cleaning */
	bctbx_file_close(fp);
	rtp_bundle_delete(rtpBundle_relay);
	rtp_bundle_delete(rtpBundle_margaux);
	if (bundled_source) {
		rtp_bundle_delete(rtpBundle_marielle);
		rtp_bundle_delete(rtpBundle_relay_marielle);
		rtp_session_destroy(rtpSession_marielle_bis);
		rtp_session_destroy(rtpSession_relay_marielle_bis);
		rtp_session_destroy(rtpSession_relay_margaux_marielle_bis);
		rtp_session_destroy(rtpSession_margaux_marielle_bis);
	}
	rtp_profile_destroy(profile);
	ms_media_stream_sessions_uninit(&marielle);
	ms_media_stream_sessions_uninit(&margaux); // This will destroy rtpSession_margaux_marielle
	rtp_session_destroy(rtpSession_margaux_pauline);
	ms_media_stream_sessions_uninit(&pauline);
	ms_media_stream_sessions_uninit(&relay_marielle);
	ms_media_stream_sessions_uninit(&relay_margaux); // This will destroy rtpSession_relay_margaux_marielle
	rtp_session_destroy(rtpSession_relay_margaux_pauline);
	ms_media_stream_sessions_uninit(&relay_pauline);

	return error==false;
}

static void double_encrypted_relayed_data( void ) {
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AES_128_SHA1_32));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AEAD_AES_256_GCM));
};

static void double_encrypted_relayed_data_with_volume( void ) {
	/* use short bundle id: they will fit in the padding left by the volume info */
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AES_128_SHA1_32, true, false));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AEAD_AES_256_GCM, true, false));
	/* use long bundle id: they will need a pullup with insert keeping current extension header */
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AES_128_SHA1_32, true, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AEAD_AES_256_GCM, true, true));
};

static void double_encrypted_relayed_data_bundled_source( void ) {
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AES_128_SHA1_32, false, false, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_80, MS_AEAD_AES_256_GCM, false, false, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AES_128_SHA1_32, true, false, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_80, MS_AEAD_AES_256_GCM, true, false, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AES_128_SHA1_32, true, true, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_80, MS_AEAD_AES_256_GCM, true, true, true));
}

static void double_encrypted_relayed_data_use_ekt( void ) {
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AES_128_SHA1_32, false, false, false, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AEAD_AES_256_GCM, false, false, false, true));
};

static void double_encrypted_relayed_data_with_volume_use_ekt( void ) {
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AES_128_SHA1_32, true, false, false, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AEAD_AES_256_GCM, true, false, false, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AES_128_SHA1_32, true, true, false, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AEAD_AES_256_GCM, true, true, false, true));
};

static void double_encrypted_relayed_data_bundled_source_use_ekt( void ) {
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AES_128_SHA1_32, false, false, true, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AEAD_AES_256_GCM, false, false, true, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AES_128_SHA1_32, true, false, true, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AEAD_AES_256_GCM, true, false, true, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AES_128_SHA1_32, true, true, true, true));
	BC_ASSERT_TRUE(double_encrypted_rtp_relay_data_base(MS_AES_128_SHA1_32, MS_AEAD_AES_256_GCM, true, true, true, true));
};

static test_t tests[] = {
	TEST_NO_TAG("Double Encrypted relayed data two participants", double_encrypted_relayed_data),
	TEST_NO_TAG("Double Encrypted relayed data two participants with volume info", double_encrypted_relayed_data_with_volume),
	TEST_NO_TAG("Double Encrypted relayed data two participants bundled source", double_encrypted_relayed_data_bundled_source),
	TEST_NO_TAG("Double Encrypted relayed data two participants with ekt", double_encrypted_relayed_data_use_ekt),
	TEST_NO_TAG("Double Encrypted relayed data two participants with volume info and ekt", double_encrypted_relayed_data_with_volume_use_ekt),
	TEST_NO_TAG("Double Encrypted relayed data two participants with bundled source and ekt", double_encrypted_relayed_data_bundled_source_use_ekt),
};

test_suite_t double_encryption_test_suite = {
	"RTP Data Double Encryption",
	tester_before_all,
	tester_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
