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

static test_t tests[] = {
	TEST_NO_TAG("Key Exchange", exchange),
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
