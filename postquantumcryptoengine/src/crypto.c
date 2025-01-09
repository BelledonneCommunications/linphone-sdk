/*
 * Copyright (c) 2020-2025 Belledonne Communications SARL.
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <bctoolbox/crypto.h>
#include <postquantumcryptoengine/crypto.h>

/**
 * List of available key agreement algorithm
 */
uint32_t bctbxpq_key_agreement_algo_list(void) {
	uint32_t ret = 0;
	/* KEM form of ECDH X25519 and X448 is build in this project */
	if (bctbx_key_agreement_algo_list()&BCTBX_ECDH_X25519) {
		ret |= BCTBX_KEM_X25519;
	}
	if (bctbx_key_agreement_algo_list()&BCTBX_ECDH_X448) {
		ret |= BCTBX_KEM_X448;
	}
	/* LibOQS is built with support for MLKEM(512,768 and 1024), Kyber(512,768 and 1024), HQC(128,192 and 256)*/
	/* TODO: check they were actually enabled at compile time in liboqs */
	ret |= BCTBX_KEM_KYBER512
	|BCTBX_KEM_KYBER768
	|BCTBX_KEM_KYBER1024
	|BCTBX_KEM_HQC128
	|BCTBX_KEM_HQC192
	|BCTBX_KEM_HQC256
	|BCTBX_KEM_MLKEM512
	|BCTBX_KEM_MLKEM768
	|BCTBX_KEM_MLKEM1024;

	return ret;
}
