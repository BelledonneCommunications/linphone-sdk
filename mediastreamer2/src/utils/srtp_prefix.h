/*
  mediastreamer2 library - modular sound and video processing and streaming
  Copyright (C) 2006-2014 Belledonne Communications, Grenoble

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef __SRTP2_H__
#define __SRTP2_H__

#if defined(MS2_WINDOWS_PHONE)
// Windows phone doesn't use make install
#include <srtp.h>
#elif SRTP_VERSION==1
#include <srtp/srtp.h>
#else
#include <srtp2/srtp.h>
#define err_status_t srtp_err_status_t
#define err_status_ok srtp_err_status_ok
#define crypto_policy_t srtp_crypto_policy_t
#define crypto_policy_set_aes_cm_256_hmac_sha1_80 srtp_crypto_policy_set_aes_cm_256_hmac_sha1_80
#define crypto_policy_set_aes_cm_128_hmac_sha1_32 srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32
#define crypto_policy_set_aes_cm_128_null_auth srtp_crypto_policy_set_aes_cm_128_null_auth
#define crypto_policy_set_null_cipher_hmac_sha1_80 srtp_crypto_policy_set_null_cipher_hmac_sha1_80
#define crypto_policy_set_aes_cm_128_hmac_sha1_80 srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80
#define crypto_policy_set_aes_cm_256_hmac_sha1_32 srtp_crypto_policy_set_aes_cm_256_hmac_sha1_32
#define ssrc_t srtp_ssrc_t
#endif

#endif
