/*
	lime_crypto-tester.cpp
	@author Johan Pascal
	@copyright 	Copyright (C) 2018  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
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

#define BCTBX_LOG_DOMAIN "lime-tester"
#include <bctoolbox/logging.h>

#include "lime-tester.hpp"
#include "lime-tester-utils.hpp"
#include "lime_keys.hpp"
#include "lime_crypto_primitives.hpp"

#include <bctoolbox/tester.h>
#include <bctoolbox/port.h>
#include <bctoolbox/exception.hh>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdio.h>
#include <string.h>

using namespace::std;
using namespace::lime;

constexpr uint64_t BENCH_TIMING_MS=200;

/* Function */
static void snprintSI(std::string &output, double x, const char *unit, const char *spacer = " ") {
	const char *small[] = {" ","m","µ","n","p"};
	const char *big[] = {" ","k","M","G","T"};

	constexpr size_t tempBufferSize = 100;
	char tempBuffer[tempBufferSize]; // hoping no one will use this function to print more than 100 chars...

	if (12+strlen(spacer)+strlen(unit)>sizeof(tempBuffer)) {// 6 digit, 1 point, 2 digits + unit + 1 prefix + spacer + NULL term
	throw BCTBX_EXCEPTION << "snprintSI tmpBuffer is too small to hold your data";
	}

	if (x < 1) {
		unsigned di=0;
		for (di=0; di<sizeof(small)/sizeof(*small)-1 && x && x < 1; di++) {
			x *= 1000.0;
		}
		snprintf(tempBuffer, sizeof(tempBuffer), "%6.2f%s%s%s", x, spacer, small[di], unit);
	} else {
		unsigned di=0;
		for (di=0; di<sizeof(big)/sizeof(*big)-1 && x && x >= 1000; di++) {
			x /= 1000.0;
		}
		snprintf(tempBuffer, sizeof(tempBuffer), "%6.2f%s%s%s", x, spacer, big[di], unit);
	}

	output = tempBuffer;
}

template <typename Curve>
void keyExchange_test(void) {
	/* We need a RNG */
	std::shared_ptr<RNG> rng = make_RNG();
	/* Create Alice and Bob ECDH context */
	std::shared_ptr<keyExchange<Curve>> Alice = make_keyExchange<Curve>();
	std::shared_ptr<keyExchange<Curve>> Bob = make_keyExchange<Curve>();

	/* Generate key pairs */
	Alice->createKeyPair(rng);
	Bob->createKeyPair(rng);

	/* Exchange keys */
	Alice->set_peerPublic(Bob->get_selfPublic());
	Bob->set_peerPublic(Alice->get_selfPublic());

	/* Compute shared secret */
	Alice->computeSharedSecret();
	Bob->computeSharedSecret();

	/* Compare them */
	BC_ASSERT_TRUE(Alice->get_sharedSecret()==Bob->get_sharedSecret());
}

template <typename Curve>
void keyExchange_bench(uint64_t runTime_ms) {
	constexpr size_t batch_size = 100;

	/* We need a RNG */
	std::shared_ptr<RNG> rng = make_RNG();

	/* Create Alice and Bob ECDH context */
	std::shared_ptr<keyExchange<Curve>> Alice = make_keyExchange<Curve>();
	std::shared_ptr<keyExchange<Curve>> Bob = make_keyExchange<Curve>();

	auto start = bctbx_get_cur_time_ms();
	uint64_t span=0;
	size_t runCount = 0;

	while (span<runTime_ms) {
		for (size_t i=0; i<batch_size; i++) {
			/* Generate key pairs */
			Alice->createKeyPair(rng);
			Bob->createKeyPair(rng);
		}
		span = bctbx_get_cur_time_ms() - start;
		runCount += batch_size;
	}

	auto freq = 2000*runCount/static_cast<double>(span);
	std::string freq_unit, period_unit;
	snprintSI(freq_unit, freq, "keys/s");
	snprintSI(period_unit, 1/freq, "s/keys");
	std::cout<<"Key generation "<<int(2*runCount)<<" ECDH keys in "<<int(span)<<" ms : "<<period_unit<<" "<<freq_unit<<endl;

	/* Exchange keys */
	Alice->set_peerPublic(Bob->get_selfPublic());
	Bob->set_peerPublic(Alice->get_selfPublic());

	start = bctbx_get_cur_time_ms();
	span=0;
	runCount = 0;

	while (span<runTime_ms) {
		for (size_t i=0; i<batch_size; i++) {
			/* Compute shared secret */
			Alice->computeSharedSecret();
		}
		span = bctbx_get_cur_time_ms() - start;
		runCount += batch_size;
	}
	freq = 1000*runCount/static_cast<double>(span);
	snprintSI(freq_unit, freq, "computations/s");
	snprintSI(period_unit, 1/freq, "s/computation");
	std::cout<<"Shared Secret "<<int(runCount)<<" computations in "<<int(span)<<" ms : "<<period_unit<<" "<<freq_unit<<endl<<endl;
}

static void exchange(void) {
#ifdef EC25519_ENABLED
	keyExchange_test<C255>();
	if (bench) {
		std::cout<<"Bench for Curve 25519:"<<endl;
		keyExchange_bench<C255>(BENCH_TIMING_MS);
	}
#endif
#ifdef EC448_ENABLED
	keyExchange_test<C448>();
	if (bench) {
		std::cout<<"Bench for Curve 448:"<<endl;
		keyExchange_bench<C448>(BENCH_TIMING_MS);
	}
#endif
}

/**
 * Testing sign, verify and DSA to keyEchange key conversion
 * Scenario:
 * - Alice and Bob generate a Signature key pair
 * - They both sign a message, exchange it and verify it
 * - each of them convert their private Signature key into a private keyExchange one and derive the matching public key
 * - each of them convert the peer Signature public key into a keyExchange public key
 * - both compute the shared secret and compare
 */
template <typename Curve>
void signAndVerify_test(void) {
	/* We need a RNG */
	auto rng = make_RNG();
	/* Create Alice, Bob, Vera Signature context */
	auto AliceDSA = make_Signature<Curve>();
	auto BobDSA = make_Signature<Curve>();
	auto Vera = make_Signature<Curve>();

	std::string aliceMessageString{"Lluchiwn ein gwydrau achos Ni yw y byd Ni yw y byd, Ni yw y byd, Carwn ein gelynion achos Ni yw y byd. Ni yw y byd, dewch bawb ynghyd, Tynnwn ein dillad achos Ni yw y byd. Ni yw y byd, Ni yw y byd, Dryswn ein cyfoedion achos Ni yw y byd. Ni yw y byd, dewch bawb ynghyd, Gwaeddwn yn llawen achos Ni yw y byd."};
	std::string bobMessageString{"Neidiwn i'r awyr achos ni yw y byd Ni yw y byd, dewch bawb ynghyd, Chwalwn ddisgyrchiant achos Ni yw y byd, Rowliwn yn y rhedyn achos Ni yw y byd. Rhyddhawn ein penblethau! Ni yw y byd, dewch bawb ynghyd, Paratown am chwyldro achos Ni yw y byd"};
	std::vector<uint8_t> aliceMessage{aliceMessageString.cbegin(), aliceMessageString.cend()};
	std::vector<uint8_t> bobMessage{bobMessageString.cbegin(), bobMessageString.cend()};

	/* Generate Signature key pairs */
	AliceDSA->createKeyPair(rng);
	BobDSA->createKeyPair(rng);

	/* Sign messages*/
	DSA<Curve, lime::DSAtype::signature> aliceSignature;
	DSA<Curve, lime::DSAtype::signature> bobSignature;
	AliceDSA->sign(aliceMessage, aliceSignature);
	BobDSA->sign(bobMessage, bobSignature);

	/* Vera check messages authenticity */
	Vera->set_public(AliceDSA->get_public());
	BC_ASSERT_TRUE(Vera->verify(aliceMessage, aliceSignature));
	BC_ASSERT_FALSE(Vera->verify(bobMessage, aliceSignature));
	Vera->set_public(BobDSA->get_public());
	BC_ASSERT_FALSE(Vera->verify(aliceMessage, bobSignature));
	BC_ASSERT_TRUE(Vera->verify(bobMessage, bobSignature));

	/* Bob and Alice create keyExchange context */
	auto AliceKeyExchange = make_keyExchange<Curve>();
	auto BobKeyExchange = make_keyExchange<Curve>();

	/* Convert keys */
	AliceKeyExchange->set_secret(AliceDSA->get_secret()); // auto convert from DSA to X format
	AliceKeyExchange->deriveSelfPublic(); // derive public from private
	AliceKeyExchange->set_peerPublic(BobDSA->get_public()); // import Bob DSA public key

	BobKeyExchange->set_secret(BobDSA->get_secret()); // convert from DSA to X format
	BobKeyExchange->set_selfPublic(BobDSA->get_public()); // convert from DSA to X format
	BobKeyExchange->set_peerPublic(AliceDSA->get_public()); // import Alice DSA public key

	/* Compute shared secret */
	AliceKeyExchange->computeSharedSecret();
	BobKeyExchange->computeSharedSecret();

	/* Compare them */
	BC_ASSERT_TRUE(AliceKeyExchange->get_sharedSecret()==BobKeyExchange->get_sharedSecret());
}

template <typename Curve>
void signAndVerify_bench(uint64_t runTime_ms ) {
	constexpr size_t batch_size = 100;

	/* We need a RNG */
	auto rng = make_RNG();
	/* Create Alice, Vera Signature context */
	auto Alice = make_Signature<Curve>();
	auto Vera = make_Signature<Curve>();

	// the message to sign is a public Key for keyExchange algo
	auto keyExchangeContext = make_keyExchange<Curve>();
	keyExchangeContext->createKeyPair(rng);
	auto XpublicKey = keyExchangeContext->get_selfPublic();

	auto start = bctbx_get_cur_time_ms();
	uint64_t span=0;
	size_t runCount = 0;

	while (span<runTime_ms) {
		for (size_t i=0; i<batch_size; i++) {
			/* Generate Signature key pairs */
			Alice->createKeyPair(rng);
		}
		span = bctbx_get_cur_time_ms() - start;
		runCount += batch_size;
	}

	auto freq = 1000*runCount/static_cast<double>(span);
	std::string freq_unit, period_unit;
	snprintSI(freq_unit, freq, "generations/s");
	snprintSI(period_unit, 1/freq, "s/generation");
	std::cout<<"Generate "<<int(runCount)<<" Signature key pairs in "<<int(span)<<" ms : "<<period_unit<<" "<<freq_unit<<endl;

	start = bctbx_get_cur_time_ms();
	span=0;
	runCount = 0;

	/* Sign messages*/
	DSA<Curve, lime::DSAtype::signature> aliceSignature;

	while (span<runTime_ms) {
		for (size_t i=0; i<batch_size; i++) {
			Alice->sign(XpublicKey, aliceSignature);
		}
		span = bctbx_get_cur_time_ms() - start;
		runCount += batch_size;
	}

	freq = 1000*runCount/static_cast<double>(span);
	snprintSI(freq_unit, freq, "signatures/s");
	snprintSI(period_unit, 1/freq, "s/signature");
	std::cout<<"Sign "<<int(runCount)<<" messages "<<int(span)<<" ms : "<<period_unit<<" "<<freq_unit<<endl;

	start = bctbx_get_cur_time_ms();
	span=0;
	runCount = 0;
	/* Vera check messages authenticity */
	Vera->set_public(Alice->get_public());
	while (span<runTime_ms) {
		for (size_t i=0; i<batch_size; i++) {
			Vera->verify(XpublicKey, aliceSignature);
		}
		span = bctbx_get_cur_time_ms() - start;
		runCount += batch_size;
	}

	freq = 1000*runCount/static_cast<double>(span);
	snprintSI(freq_unit, freq, "verifies/s");
	snprintSI(period_unit, 1/freq, "s/verify");
	std::cout<<"Verify "<<int(runCount)<<" messages "<<int(span)<<" ms : "<<period_unit<<" "<<freq_unit<<endl<<endl;

	BC_ASSERT_TRUE(Vera->verify(XpublicKey, aliceSignature));
}

static void signAndVerify(void) {
#ifdef EC25519_ENABLED
	signAndVerify_test<C255>();
	if (bench) {
		std::cout<<"Bench for Curve 25519:"<<endl;
		signAndVerify_bench<C255>(BENCH_TIMING_MS);
	}
#endif
#ifdef EC448_ENABLED
	signAndVerify_test<C448>();
	if (bench) {
		std::cout<<"Bench for Curve 448:"<<endl;
		signAndVerify_bench<C448>(BENCH_TIMING_MS);
	}
#endif
}

static test_t tests[] = {
	TEST_NO_TAG("Key Exchange", exchange),
	TEST_NO_TAG("Signature", signAndVerify),
};

test_suite_t lime_crypto_test_suite = {
	"Crypto",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};
