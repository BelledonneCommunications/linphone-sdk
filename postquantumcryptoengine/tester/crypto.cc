/*
 * Copyright (c) 2016-2020 Belledonne Communications SARL.
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

#include <stdio.h>
#include <cmath>
#include "postquantumcryptoengine-tester.h"
#include "postquantumcryptoengine/crypto.hh"
#include "bctoolbox/exception.hh"
#include "KEM_patterns.cc"

#include <array>

using namespace bctoolbox;


static void ECDH_KEM_test(void) {
	std::vector<uint8_t> pk, sk, ct, ssa, ssb;

	K25519 k255 = K25519(BCTBX_MD_SHA256);
	K448 k448_sha256 = K448(BCTBX_MD_SHA256);
	K448 k448_sha384 = K448(BCTBX_MD_SHA384);
	K448 k448_sha512 = K448(BCTBX_MD_SHA512);

	BC_ASSERT_EQUAL(k255.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(k255.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(k255.decaps(ssb, ct, sk), 0, int, "%d");
	BC_ASSERT_FALSE(ssb.empty());

	BC_ASSERT_TRUE(ssa == ssb);

	pk.clear();
	sk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	BC_ASSERT_EQUAL(k448_sha256.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(k448_sha256.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(k448_sha256.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	pk.clear();
	sk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	BC_ASSERT_EQUAL(k448_sha384.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(k448_sha384.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(k448_sha384.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	pk.clear();
	sk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	BC_ASSERT_EQUAL(k448_sha512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(k448_sha512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(k448_sha512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	/* Test vectors */

	BC_ASSERT_EQUAL(k255.decaps(ssb, X25516_SHA256_pattern.at(1), X25516_SHA256_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(X25516_SHA256_pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(k448_sha256.decaps(ssb, X448_SHA256_pattern.at(1), X448_SHA256_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(X448_SHA256_pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(k448_sha384.decaps(ssb, X448_SHA384_pattern.at(1), X448_SHA384_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(X448_SHA384_pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(k448_sha512.decaps(ssb, X448_SHA512_pattern.at(1), X448_SHA512_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(X448_SHA512_pattern.at(2) == ssb);

	ssb.clear();
}

static void HQC_test(void){
	std::vector<uint8_t> pk, sk, ct, ssa, ssb;

	HQC128   hqc128 = HQC128();
	HQC192   hqc192 = HQC192();
	HQC256   hqc256 = HQC256();

	BC_ASSERT_EQUAL(hqc128.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hqc128.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(hqc128.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	pk.clear();
	sk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	BC_ASSERT_EQUAL(hqc192.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hqc192.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(hqc192.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	pk.clear();
	sk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	BC_ASSERT_EQUAL(hqc256.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hqc256.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(hqc256.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	pk.clear();
	sk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	// Test for HQC128
	BC_ASSERT_EQUAL(hqc128.decaps(ssb, HQC128_pattern.at(1), HQC128_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(HQC128_pattern.at(2) == ssb);

	ssb.clear();

	// Test for HQC192
	BC_ASSERT_EQUAL(hqc192.decaps(ssb, HQC192_pattern.at(1), HQC192_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(HQC192_pattern.at(2) == ssb);

	ssb.clear();

	// Test for HQC256
	BC_ASSERT_EQUAL(hqc256.decaps(ssb, HQC256_pattern.at(1), HQC256_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(HQC256_pattern.at(2) == ssb);

	ssb.clear();
}

static void KYBER_test(void){
	std::vector<uint8_t> pk, sk, ct, ssa, ssb;

	KYBER512  kyber512 = KYBER512();
	KYBER768  kyber768 = KYBER768();
	KYBER1024 kyber1024 = KYBER1024();

	BC_ASSERT_EQUAL(kyber512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(kyber512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(kyber512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	pk.clear();
	sk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	BC_ASSERT_EQUAL(kyber768.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(kyber768.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(kyber768.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	pk.clear();
	sk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	BC_ASSERT_EQUAL(kyber1024.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(kyber1024.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(kyber1024.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	pk.clear();
	sk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	// Test for KYBER512
	BC_ASSERT_EQUAL(kyber512.decaps(ssb, KYBER512_pattern.at(1), KYBER512_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(KYBER512_pattern.at(2) == ssb);

	ssb.clear();

	// Test for KYBER768
	BC_ASSERT_EQUAL(kyber768.decaps(ssb, KYBER768_pattern.at(1), KYBER768_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(KYBER768_pattern.at(2) == ssb);

	ssb.clear();

	// Test for KYBER1024
	BC_ASSERT_EQUAL(kyber1024.decaps(ssb, KYBER1024_pattern.at(1), KYBER1024_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(KYBER1024_pattern.at(2) == ssb);

	ssb.clear();
}

static void MLKEM_test(void){
	std::vector<uint8_t> pk, sk, ct, ssa, ssb;

	MLKEM512  mlkem512 = MLKEM512();
	MLKEM768  mlkem768 = MLKEM768();
	MLKEM1024 mlkem1024 = MLKEM1024();

	BC_ASSERT_EQUAL(mlkem512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(mlkem512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(mlkem512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	pk.clear();
	sk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	BC_ASSERT_EQUAL(mlkem768.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(mlkem768.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(mlkem768.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	pk.clear();
	sk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	BC_ASSERT_EQUAL(mlkem1024.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(mlkem1024.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(mlkem1024.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	pk.clear();
	sk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	// Test for MLKEM512
	BC_ASSERT_EQUAL(mlkem512.decaps(ssb, MLKEM512_pattern.at(1), MLKEM512_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(MLKEM512_pattern.at(2) == ssb);

	ssb.clear();

	// Test for MLKEM768
	BC_ASSERT_EQUAL(mlkem768.decaps(ssb, MLKEM768_pattern.at(1), MLKEM768_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(MLKEM768_pattern.at(2) == ssb);

	ssb.clear();

	// Test for MLKEM1024
	BC_ASSERT_EQUAL(mlkem1024.decaps(ssb, MLKEM1024_pattern.at(1), MLKEM1024_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(MLKEM1024_pattern.at(2) == ssb);

	ssb.clear();
}

/*
 * Code to generate patterns for hybrid KEMs
 */
#if 0
static void localPrintHex(std::string title, std::vector<uint8_t> buf) {
	std::cout<<title;
	for (uint8_t b : buf) {
		std::cout<<"0x"<<std::hex << std::setfill('0') << std::setw(2) << int(b);
		std::cout<<", ";
	}
	std::cout<<std::endl;
}
#endif

static void HYBRID_KEM_test(){
	std::vector<uint8_t> sk, sk1, sk2, ct1, ct2, pk, ct, ssa, ssb;

	/* X25519 - X25519 */

	HYBRID_KEM hyb_k255_k255 = HYBRID_KEM({std::make_shared<K25519>(BCTBX_MD_SHA256), std::make_shared<K25519>(BCTBX_MD_SHA256)}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hyb_k255_k255.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k255_k255.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(hyb_k255_k255.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K25519 - K25519

	BC_ASSERT_EQUAL(hyb_k255_k255.decaps(ssb, hybrid_X25519_X25519_SHA256_pattern.at(1), hybrid_X25519_X25519_SHA256_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X25519_X25519_SHA256_pattern.at(2) == ssb);

	ssb.clear();

	/* K25519 - KYBER512 */

	HYBRID_KEM hyb_k255_kyber512 = HYBRID_KEM({std::make_shared<K25519>(BCTBX_MD_SHA256), std::make_shared<KYBER512>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hyb_k255_kyber512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k255_kyber512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(hyb_k255_kyber512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K25519 - KYBER512

	BC_ASSERT_EQUAL(hyb_k255_kyber512.decaps(ssb, hybrid_X25519_KYBER512_SHA256_pattern.at(1), hybrid_X25519_KYBER512_SHA256_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X25519_KYBER512_SHA256_pattern.at(2) == ssb);

	ssb.clear();

	/* K25519 - MLKEM512 */

	HYBRID_KEM hyb_k255_mlkem512 = HYBRID_KEM({std::make_shared<K25519>(BCTBX_MD_SHA256), std::make_shared<MLKEM512>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hyb_k255_mlkem512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k255_mlkem512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(hyb_k255_mlkem512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K25519 - MLKEM512

	BC_ASSERT_EQUAL(hyb_k255_mlkem512.decaps(ssb, hybrid_X25519_MLKEM512_SHA256_pattern.at(1), hybrid_X25519_MLKEM512_SHA256_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X25519_MLKEM512_SHA256_pattern.at(2) == ssb);

	ssb.clear();

	/* K25519 - HQC128 */

	HYBRID_KEM hyb_k255_hqc128 = HYBRID_KEM({std::make_shared<K25519>(BCTBX_MD_SHA256), std::make_shared<HQC128>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hyb_k255_hqc128.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k255_hqc128.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(hyb_k255_hqc128.decaps(ssb, ct, sk), 0, int, "%d");


	BC_ASSERT_TRUE(ssa == ssb);
	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K25519 - HQC128

	BC_ASSERT_EQUAL(hyb_k255_hqc128.decaps(ssb, hybrid_X25519_HQC128_SHA256_pattern.at(1), hybrid_X25519_HQC128_SHA256_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X25519_HQC128_SHA256_pattern.at(2) == ssb);

	ssb.clear();

	/* K448 - KYBER1024 */

	HYBRID_KEM hyb_k448_kyber1024_sha256 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA256), std::make_shared<KYBER1024>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hyb_k448_kyber1024_sha256.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_kyber1024_sha256.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_kyber1024_sha256.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hyb_k448_kyber1024_sha384 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA384), std::make_shared<KYBER1024>()}, BCTBX_MD_SHA384);

	BC_ASSERT_EQUAL(hyb_k448_kyber1024_sha384.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_kyber1024_sha384.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_kyber1024_sha384.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hyb_k448_kyber1024_sha512 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA512), std::make_shared<KYBER1024>()}, BCTBX_MD_SHA512);

	BC_ASSERT_EQUAL(hyb_k448_kyber1024_sha512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_kyber1024_sha512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_kyber1024_sha512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K448 - KYBER1024

	BC_ASSERT_EQUAL(hyb_k448_kyber1024_sha256.decaps(ssb, hybrid_X448_KYBER1024_SHA256_pattern.at(1), hybrid_X448_KYBER1024_SHA256_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X448_KYBER1024_SHA256_pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hyb_k448_kyber1024_sha384.decaps(ssb, hybrid_X448_KYBER1024_SHA384_pattern.at(1), hybrid_X448_KYBER1024_SHA384_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X448_KYBER1024_SHA384_pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hyb_k448_kyber1024_sha512.decaps(ssb, hybrid_X448_KYBER1024_SHA512_pattern.at(1), hybrid_X448_KYBER1024_SHA512_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X448_KYBER1024_SHA512_pattern.at(2) == ssb);

	ssb.clear();

	/* K448 - MLKEM1024 */

	HYBRID_KEM hyb_k448_mlkem1024_sha256 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA256), std::make_shared<MLKEM1024>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hyb_k448_mlkem1024_sha256.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_mlkem1024_sha256.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_mlkem1024_sha256.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hyb_k448_mlkem1024_sha384 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA384), std::make_shared<MLKEM1024>()}, BCTBX_MD_SHA384);

	BC_ASSERT_EQUAL(hyb_k448_mlkem1024_sha384.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_mlkem1024_sha384.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_mlkem1024_sha384.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hyb_k448_mlkem1024_sha512 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA512), std::make_shared<MLKEM1024>()}, BCTBX_MD_SHA512);

	BC_ASSERT_EQUAL(hyb_k448_mlkem1024_sha512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_mlkem1024_sha512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_mlkem1024_sha512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K448 - MLKEM1024

	BC_ASSERT_EQUAL(hyb_k448_mlkem1024_sha256.decaps(ssb, hybrid_X448_MLKEM1024_SHA256_pattern.at(1), hybrid_X448_MLKEM1024_SHA256_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X448_MLKEM1024_SHA256_pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hyb_k448_mlkem1024_sha384.decaps(ssb, hybrid_X448_MLKEM1024_SHA384_pattern.at(1), hybrid_X448_MLKEM1024_SHA384_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X448_MLKEM1024_SHA384_pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hyb_k448_mlkem1024_sha512.decaps(ssb, hybrid_X448_MLKEM1024_SHA512_pattern.at(1), hybrid_X448_MLKEM1024_SHA512_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X448_MLKEM1024_SHA512_pattern.at(2) == ssb);

	ssb.clear();

	/* K448 - HQC256 */

	HYBRID_KEM hyb_k448_hqc256_sha256 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA256), std::make_shared<HQC256>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hyb_k448_hqc256_sha256.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_hqc256_sha256.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_hqc256_sha256.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	sk.clear();
	pk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hyb_k448_hqc256_sha384 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA384), std::make_shared<HQC256>()}, BCTBX_MD_SHA384);

	BC_ASSERT_EQUAL(hyb_k448_hqc256_sha384.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_hqc256_sha384.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_hqc256_sha384.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	sk.clear();
	pk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hyb_k448_hqc256_sha512 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA512), std::make_shared<HQC256>()}, BCTBX_MD_SHA512);

	BC_ASSERT_EQUAL(hyb_k448_hqc256_sha512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_hqc256_sha512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_hqc256_sha512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	sk.clear();
	pk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K448 - HQC256

	BC_ASSERT_EQUAL(hyb_k448_hqc256_sha256.decaps(ssb, hybrid_X448_HQC256_SHA256_pattern.at(1), hybrid_X448_HQC256_SHA256_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X448_HQC256_SHA256_pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hyb_k448_hqc256_sha384.decaps(ssb, hybrid_X448_HQC256_SHA384_pattern.at(1), hybrid_X448_HQC256_SHA384_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X448_HQC256_SHA384_pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hyb_k448_hqc256_sha512.decaps(ssb, hybrid_X448_HQC256_SHA512_pattern.at(1), hybrid_X448_HQC256_SHA512_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X448_HQC256_SHA512_pattern.at(2) == ssb);

	ssb.clear();

	/* K25519 - KYBER512 - HQC128 */

	HYBRID_KEM hyb_k255_kyb512_hqc128 = HYBRID_KEM({std::make_shared<K25519>(BCTBX_MD_SHA256), std::make_shared<KYBER512>(), std::make_shared<HQC128>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hyb_k255_kyb512_hqc128.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k255_kyb512_hqc128.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(hyb_k255_kyb512_hqc128.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);
	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K25519 - KYBER512 - HQC128

	BC_ASSERT_EQUAL(hyb_k255_kyb512_hqc128.decaps(ssb, hybrid_X25519_KYBER512_HQC128_SHA256_pattern.at(1), hybrid_X25519_KYBER512_HQC128_SHA256_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X25519_KYBER512_HQC128_SHA256_pattern.at(2) == ssb);

	ssb.clear();


	/* K448 - KYBER1024 - HQC256 */
	HYBRID_KEM hyb_k448_kyber1024_hqc256_sha256 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA256), std::make_shared<KYBER1024>(), std::make_shared<HQC256>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hyb_k448_kyber1024_hqc256_sha256.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_kyber1024_hqc256_sha256.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_kyber1024_hqc256_sha256.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	sk.clear();
	pk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hyb_k448_kyber1024_hqc256_sha384 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA384), std::make_shared<KYBER1024>(), std::make_shared<HQC256>()}, BCTBX_MD_SHA384);

	BC_ASSERT_EQUAL(hyb_k448_kyber1024_hqc256_sha384.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_kyber1024_hqc256_sha384.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_kyber1024_hqc256_sha384.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	sk.clear();
	pk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hyb_k448_kyber1024_hqc256_sha512 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA512), std::make_shared<KYBER1024>(), std::make_shared<HQC256>()}, BCTBX_MD_SHA512);

	BC_ASSERT_EQUAL(hyb_k448_kyber1024_hqc256_sha512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_kyber1024_hqc256_sha512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hyb_k448_kyber1024_hqc256_sha512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	sk.clear();
	pk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K448 - KYBER1024 - HQC256
	BC_ASSERT_EQUAL(hyb_k448_kyber1024_hqc256_sha256.decaps(ssb, hybrid_X448_KYBER1024_HQC256_SHA256_pattern.at(1), hybrid_X448_KYBER1024_HQC256_SHA256_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X448_KYBER1024_HQC256_SHA256_pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hyb_k448_kyber1024_hqc256_sha384.decaps(ssb, hybrid_X448_KYBER1024_HQC256_SHA384_pattern.at(1), hybrid_X448_KYBER1024_HQC256_SHA384_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X448_KYBER1024_HQC256_SHA384_pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hyb_k448_kyber1024_hqc256_sha512.decaps(ssb, hybrid_X448_KYBER1024_HQC256_SHA512_pattern.at(1), hybrid_X448_KYBER1024_HQC256_SHA512_pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybrid_X448_KYBER1024_HQC256_SHA512_pattern.at(2) == ssb);

	ssb.clear();
}

static test_t crypto_tests[] = {
	TEST_NO_TAG("ECDH KEM", ECDH_KEM_test),
	TEST_NO_TAG("HQC", HQC_test),
	TEST_NO_TAG("KYBER", KYBER_test),
	TEST_NO_TAG("MLKEM", MLKEM_test),
	TEST_NO_TAG("HYBRID KEM", HYBRID_KEM_test),
};

test_suite_t crypto_test_suite = {"Crypto", NULL, NULL, NULL, NULL,
							   sizeof(crypto_tests) / sizeof(crypto_tests[0]), crypto_tests, 0};
