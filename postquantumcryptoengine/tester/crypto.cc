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
	K448 k448Sha256 = K448(BCTBX_MD_SHA256);
	K448 k448Sha384 = K448(BCTBX_MD_SHA384);
	K448 k448Sha512 = K448(BCTBX_MD_SHA512);

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

	BC_ASSERT_EQUAL(k448Sha256.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(k448Sha256.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(k448Sha256.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	pk.clear();
	sk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	BC_ASSERT_EQUAL(k448Sha384.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(k448Sha384.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(k448Sha384.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	pk.clear();
	sk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	BC_ASSERT_EQUAL(k448Sha512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(k448Sha512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(k448Sha512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	/* Test vectors */

	BC_ASSERT_EQUAL(k255.decaps(ssb, X25519Sha256Pattern.at(1), X25519Sha256Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(X25519Sha256Pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(k448Sha256.decaps(ssb, X448Sha256Pattern.at(1), X448Sha256Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(X448Sha256Pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(k448Sha384.decaps(ssb, X448Sha384Pattern.at(1), X448Sha384Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(X448Sha384Pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(k448Sha512.decaps(ssb, X448Sha512Pattern.at(1), X448Sha512Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(X448Sha512Pattern.at(2) == ssb);

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
	BC_ASSERT_EQUAL(hqc128.decaps(ssb, HQC128Pattern.at(1), HQC128Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(HQC128Pattern.at(2) == ssb);

	ssb.clear();

	// Test for HQC192
	BC_ASSERT_EQUAL(hqc192.decaps(ssb, HQC192Pattern.at(1), HQC192Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(HQC192Pattern.at(2) == ssb);

	ssb.clear();

	// Test for HQC256
	BC_ASSERT_EQUAL(hqc256.decaps(ssb, HQC256Pattern.at(1), HQC256Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(HQC256Pattern.at(2) == ssb);

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
	BC_ASSERT_EQUAL(kyber512.decaps(ssb, KYBER512Pattern.at(1), KYBER512Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(KYBER512Pattern.at(2) == ssb);

	ssb.clear();

	// Test for KYBER768
	BC_ASSERT_EQUAL(kyber768.decaps(ssb, KYBER768Pattern.at(1), KYBER768Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(KYBER768Pattern.at(2) == ssb);

	ssb.clear();

	// Test for KYBER1024
	BC_ASSERT_EQUAL(kyber1024.decaps(ssb, KYBER1024Pattern.at(1), KYBER1024Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(KYBER1024Pattern.at(2) == ssb);

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
	BC_ASSERT_EQUAL(mlkem512.decaps(ssb, MLKEM512Pattern.at(1), MLKEM512Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(MLKEM512Pattern.at(2) == ssb);

	ssb.clear();

	// Test for MLKEM768
	BC_ASSERT_EQUAL(mlkem768.decaps(ssb, MLKEM768Pattern.at(1), MLKEM768Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(MLKEM768Pattern.at(2) == ssb);

	ssb.clear();

	// Test for MLKEM1024
	BC_ASSERT_EQUAL(mlkem1024.decaps(ssb, MLKEM1024Pattern.at(1), MLKEM1024Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(MLKEM1024Pattern.at(2) == ssb);

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

	HYBRID_KEM hybK255_k255 = HYBRID_KEM({std::make_shared<K25519>(BCTBX_MD_SHA256), std::make_shared<K25519>(BCTBX_MD_SHA256)}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hybK255_k255.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK255_k255.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(hybK255_k255.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K25519 - K25519

	BC_ASSERT_EQUAL(hybK255_k255.decaps(ssb, hybridX25519X25519Sha256Pattern.at(1), hybridX25519X25519Sha256Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX25519X25519Sha256Pattern.at(2) == ssb);

	ssb.clear();

	/* K25519 - KYBER512 */

	HYBRID_KEM hybK255Kyber512 = HYBRID_KEM({std::make_shared<K25519>(BCTBX_MD_SHA256), std::make_shared<KYBER512>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hybK255Kyber512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK255Kyber512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(hybK255Kyber512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K25519 - KYBER512

	BC_ASSERT_EQUAL(hybK255Kyber512.decaps(ssb, hybridX25519Kyber512Sha256Pattern.at(1), hybridX25519Kyber512Sha256Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX25519Kyber512Sha256Pattern.at(2) == ssb);

	ssb.clear();

	/* K25519 - MLKEM512 */

	HYBRID_KEM hybK255Mlkem512 = HYBRID_KEM({std::make_shared<K25519>(BCTBX_MD_SHA256), std::make_shared<MLKEM512>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hybK255Mlkem512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK255Mlkem512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(hybK255Mlkem512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K25519 - MLKEM512

	BC_ASSERT_EQUAL(hybK255Mlkem512.decaps(ssb, hybridX25519Mlkem512Sha256Pattern.at(1), hybridX25519Mlkem512Sha256Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX25519Mlkem512Sha256Pattern.at(2) == ssb);

	ssb.clear();

	/* K25519 - HQC128 */

	HYBRID_KEM hybK255Hqc128 = HYBRID_KEM({std::make_shared<K25519>(BCTBX_MD_SHA256), std::make_shared<HQC128>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hybK255Hqc128.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK255Hqc128.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(hybK255Hqc128.decaps(ssb, ct, sk), 0, int, "%d");


	BC_ASSERT_TRUE(ssa == ssb);
	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K25519 - HQC128

	BC_ASSERT_EQUAL(hybK255Hqc128.decaps(ssb, hybridX25519Hqc128Sha256Pattern.at(1), hybridX25519Hqc128Sha256Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX25519Hqc128Sha256Pattern.at(2) == ssb);

	ssb.clear();

	/* K448 - KYBER1024 */

	HYBRID_KEM hybK448Kyber1024Sha256 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA256), std::make_shared<KYBER1024>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hybK448Kyber1024Sha256.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Kyber1024Sha256.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Kyber1024Sha256.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hybK448Kyber1024Sha384 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA384), std::make_shared<KYBER1024>()}, BCTBX_MD_SHA384);

	BC_ASSERT_EQUAL(hybK448Kyber1024Sha384.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Kyber1024Sha384.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Kyber1024Sha384.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hybK448Kyber1024Sha512 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA512), std::make_shared<KYBER1024>()}, BCTBX_MD_SHA512);

	BC_ASSERT_EQUAL(hybK448Kyber1024Sha512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Kyber1024Sha512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Kyber1024Sha512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K448 - KYBER1024

	BC_ASSERT_EQUAL(hybK448Kyber1024Sha256.decaps(ssb, hybridX448Kyber1024Sha256Pattern.at(1), hybridX448Kyber1024Sha256Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX448Kyber1024Sha256Pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hybK448Kyber1024Sha384.decaps(ssb, hybridX448Kyber1024Sha384Pattern.at(1), hybridX448Kyber1024Sha384Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX448Kyber1024Sha384Pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hybK448Kyber1024Sha512.decaps(ssb, hybridX448Kyber1024Sha512Pattern.at(1), hybridX448Kyber1024Sha512Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX448Kyber1024Sha512Pattern.at(2) == ssb);

	ssb.clear();

	/* K448 - MLKEM1024 */

	HYBRID_KEM hybK448Mlkem1024Sha256 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA256), std::make_shared<MLKEM1024>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hybK448Mlkem1024Sha256.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Mlkem1024Sha256.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Mlkem1024Sha256.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hybK448Mlkem1024Sha384 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA384), std::make_shared<MLKEM1024>()}, BCTBX_MD_SHA384);

	BC_ASSERT_EQUAL(hybK448Mlkem1024Sha384.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Mlkem1024Sha384.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Mlkem1024Sha384.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hybK448Mlkem1024Sha512 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA512), std::make_shared<MLKEM1024>()}, BCTBX_MD_SHA512);

	BC_ASSERT_EQUAL(hybK448Mlkem1024Sha512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Mlkem1024Sha512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Mlkem1024Sha512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K448 - MLKEM1024

	BC_ASSERT_EQUAL(hybK448Mlkem1024Sha256.decaps(ssb, hybridX448Mlkem1024Sha256Pattern.at(1), hybridX448Mlkem1024Sha256Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX448Mlkem1024Sha256Pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hybK448Mlkem1024Sha384.decaps(ssb, hybridX448Mlkem1024Sha384Pattern.at(1), hybridX448Mlkem1024Sha384Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX448Mlkem1024Sha384Pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hybK448Mlkem1024Sha512.decaps(ssb, hybridX448Mlkem1024Sha512Pattern.at(1), hybridX448Mlkem1024Sha512Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX448Mlkem1024Sha512Pattern.at(2) == ssb);

	ssb.clear();

	/* K448 - HQC256 */

	HYBRID_KEM hybK448Hqc256Sha256 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA256), std::make_shared<HQC256>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hybK448Hqc256Sha256.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Hqc256Sha256.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Hqc256Sha256.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	sk.clear();
	pk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hybK448Hqc256Sha384 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA384), std::make_shared<HQC256>()}, BCTBX_MD_SHA384);

	BC_ASSERT_EQUAL(hybK448Hqc256Sha384.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Hqc256Sha384.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Hqc256Sha384.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	sk.clear();
	pk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hybK448Hqc256Sha512 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA512), std::make_shared<HQC256>()}, BCTBX_MD_SHA512);

	BC_ASSERT_EQUAL(hybK448Hqc256Sha512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Hqc256Sha512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Hqc256Sha512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	sk.clear();
	pk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K448 - HQC256

	BC_ASSERT_EQUAL(hybK448Hqc256Sha256.decaps(ssb, hybridX448Hqc256Sha256Pattern.at(1), hybridX448Hqc256Sha256Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX448Hqc256Sha256Pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hybK448Hqc256Sha384.decaps(ssb, hybridX448Hqc256Sha384Pattern.at(1), hybridX448Hqc256Sha384Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX448Hqc256Sha384Pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hybK448Hqc256Sha512.decaps(ssb, hybridX448Hqc256Sha512Pattern.at(1), hybridX448Hqc256Sha512Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX448Hqc256Sha512Pattern.at(2) == ssb);

	ssb.clear();

	/* K25519 - KYBER512 - HQC128 */

	HYBRID_KEM hybK255_kyb512Hqc128 = HYBRID_KEM({std::make_shared<K25519>(BCTBX_MD_SHA256), std::make_shared<KYBER512>(), std::make_shared<HQC128>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hybK255_kyb512Hqc128.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK255_kyb512Hqc128.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_FALSE(ssa.empty());
	BC_ASSERT_EQUAL(hybK255_kyb512Hqc128.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);
	ct.clear();
	pk.clear();
	sk.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K25519 - KYBER512 - HQC128

	BC_ASSERT_EQUAL(hybK255_kyb512Hqc128.decaps(ssb, hybridX25519Kyber512Hqc128Sha256Pattern.at(1), hybridX25519Kyber512Hqc128Sha256Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX25519Kyber512Hqc128Sha256Pattern.at(2) == ssb);

	ssb.clear();


	/* K448 - KYBER1024 - HQC256 */
	HYBRID_KEM hybK448Kyber1024Hqc256Sha256 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA256), std::make_shared<KYBER1024>(), std::make_shared<HQC256>()}, BCTBX_MD_SHA256);

	BC_ASSERT_EQUAL(hybK448Kyber1024Hqc256Sha256.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Kyber1024Hqc256Sha256.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Kyber1024Hqc256Sha256.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	sk.clear();
	pk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hybK448Kyber1024Hqc256Sha384 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA384), std::make_shared<KYBER1024>(), std::make_shared<HQC256>()}, BCTBX_MD_SHA384);

	BC_ASSERT_EQUAL(hybK448Kyber1024Hqc256Sha384.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Kyber1024Hqc256Sha384.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Kyber1024Hqc256Sha384.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	sk.clear();
	pk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	HYBRID_KEM hybK448Kyber1024Hqc256Sha512 = HYBRID_KEM({std::make_shared<K448>(BCTBX_MD_SHA512), std::make_shared<KYBER1024>(), std::make_shared<HQC256>()}, BCTBX_MD_SHA512);

	BC_ASSERT_EQUAL(hybK448Kyber1024Hqc256Sha512.keyGen(pk, sk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Kyber1024Hqc256Sha512.encaps(ct, ssa, pk), 0, int, "%d");
	BC_ASSERT_EQUAL(hybK448Kyber1024Hqc256Sha512.decaps(ssb, ct, sk), 0, int, "%d");

	BC_ASSERT_TRUE(ssa == ssb);

	sk.clear();
	pk.clear();
	ct.clear();
	ssa.clear();
	ssb.clear();

	// Test vector : K448 - KYBER1024 - HQC256
	BC_ASSERT_EQUAL(hybK448Kyber1024Hqc256Sha256.decaps(ssb, hybridX448Kyber1024Hqc256Sha256Pattern.at(1), hybridX448Kyber1024Hqc256Sha256Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX448Kyber1024Hqc256Sha256Pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hybK448Kyber1024Hqc256Sha384.decaps(ssb, hybridX448Kyber1024Hqc256Sha384Pattern.at(1), hybridX448Kyber1024Hqc256Sha384Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX448Kyber1024Hqc256Sha384Pattern.at(2) == ssb);

	ssb.clear();

	BC_ASSERT_EQUAL(hybK448Kyber1024Hqc256Sha512.decaps(ssb, hybridX448Kyber1024Hqc256Sha512Pattern.at(1), hybridX448Kyber1024Hqc256Sha512Pattern.at(0)), 0, int, "%d");
	BC_ASSERT_TRUE(hybridX448Kyber1024Hqc256Sha512Pattern.at(2) == ssb);

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
