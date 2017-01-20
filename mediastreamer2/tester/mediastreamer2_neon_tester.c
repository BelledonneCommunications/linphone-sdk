/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2006-2014 Belledonne Communications, Grenoble

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include "mediastreamer2_tester.h"
#include <ortp/port.h>

#if MS_HAS_ARM_NEON && defined(HAVE_SPEEXDSP)
#include <arm_neon.h>
#include <speex/speex.h>

static MSFactory *_factory= NULL;

static int tester_before_all() {
	ortp_set_log_level_mask(ORTP_LOG_DOMAIN, ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR | ORTP_FATAL);
	_factory = ms_factory_new();
	ms_factory_init_voip(_factory);
	ms_factory_init_plugins(_factory);
	srand(time(0));
	return 0;
}

static int tester_after_all() {
	ms_factory_destroy(_factory);
	return 0;
}


// tests the inner product with and without neon (using speex)
extern int libspeex_cpu_features;
extern spx_int32_t inner_prod( const spx_int16_t* a, const spx_int16_t* b, int len);

#if 0 /* These are inner product implementations that we used for fixing speex's one */
spx_int32_t ms_inner_product_neon_xcode(const spx_int16_t *a, const spx_int16_t *b, unsigned int len) {
	int16x4_t a4;
	int16x4_t b4;
	spx_int32_t sum = 0; // sum is 0
	int32x4_t part = vdupq_n_s32(0);
	int32x2_t dAccL, dAccH;
	spx_int32_t s;

	len >>=2;

	while (len--) {
		a4 = vld1_s16(a); // load 4 * 16b from a
		b4 = vld1_s16(b); // load 4 * 16b from b
		part = vmlal_s16(part, a4, b4); // part[n] += a[n] * b[n]
		a+=4;
		b+=4;
	}
	/* Split 128-bit qAccumulator into 64-bit dAccL and dAccH for
	 * accumulation */
	dAccL = vget_low_s32(part);
	dAccH = vget_high_s32(part);

	/* Accumulate 2 lanes in dAccL and dAccH into 2 lanes in dAccL */
	dAccL = vadd_s32(dAccL, dAccH);
	/* Accumulate 2 lanes in dAccL into first (and second) lane of dAccL */
	dAccL = vpadd_s32(dAccL, dAccL);

	s = vget_lane_s32(dAccL, 0);
	// ms_message("S[%d] = %d", len,s);

	/* Add accumulated value to retval */
	sum += (s >> 6);

	return sum;

}

spx_int32_t ms_inner_product_neon_xcode_optim(const spx_int16_t *a, const spx_int16_t *b, unsigned int len) {
	int16x4_t a4;
	int16x4_t b4;
	spx_int32_t sum = 0; // sum is 0
	int32x4_t partial = vdupq_n_s32(0);
	int64x2_t back;

	len >>=2;

	while (len--) {
		a4 = vld1_s16(a); // load 4 * 16b from a
		b4 = vld1_s16(b); // load 4 * 16b from b

		partial = vmlal_s16(partial, a4, b4); // part[n] += a[n] * b[n] vector multiply and add
		a+=4;
		b+=4;
	}
	back = vpaddlq_s32(partial); // sum the 4 s32 in 2 64b
	back = vshrq_n_s64(back, 6); // shift by 6

	sum += vgetq_lane_s64(back, 0) + vgetq_lane_s64(back, 1);
	partial = vdupq_n_s32(0); // reset partial sum

	return sum;

}

spx_int32_t ms_inner_product_neon_xcode_optim8(const spx_int16_t *a, const spx_int16_t *b, unsigned int len) {
	int16x8_t a8;
	int16x8_t b8;
	spx_int32_t sum = 0; // sum is 0
	int32x4_t partial = vdupq_n_s32(0);
	int64x2_t back;

	len >>=3;

	while (len--) {
		a8 = vld1q_s16(a); // load 8 16b from a
		b8 = vld1q_s16(b); // load 8 16b from b

		partial = vmlal_s16(partial, vget_low_s16(a8), vget_low_s16(b8)); // part[n] += a[n] * b[n] vector multiply and add
		partial = vmlal_s16(partial, vget_high_s16(a8), vget_high_s16(b8)); // part[n] += a[n] * b[n] vector multiply and add
		a+=8;
		b+=8;
	}
	back = vpaddlq_s32(partial); // sum the 4 s32 in 2 64b
	back = vshrq_n_s64(back, 6); // shift by 6

	sum += vgetq_lane_s64(back, 0) + vgetq_lane_s64(back, 1);
	partial = vdupq_n_s32(0); // reset partial sum

	return sum;

}

spx_int32_t ms_inner_product_neon_xcode_optim16(const spx_int16_t *a, const spx_int16_t *b, unsigned int len) {
	int16x8_t a8,b8;
	int16x8_t c8,d8;
	spx_int32_t sum = 0; // sum is 0
	int32x4_t partial = vdupq_n_s32(0);
	int32x4_t p2 =  vdupq_n_s32(0);
	int64x2_t back;

	len >>=4;

	while (len--) {
		a8 = vld1q_s16(a); // load 8 16b from a
		b8 = vld1q_s16(b); // load 8 16b from b
		c8 = vld1q_s16(a+8); // load 8 16b from b
		d8 = vld1q_s16(b+8); // load 8 16b from b

		partial = vmlal_s16(partial, vget_low_s16(a8), vget_low_s16(b8)); // part[n] += a[n] * b[n] vector multiply and add
		partial = vmlal_s16(partial, vget_high_s16(a8), vget_high_s16(b8)); // part[n] += a[n] * b[n] vector multiply and add
		p2 = vmlal_s16(p2, vget_low_s16(c8), vget_low_s16(d8)); // part[n] += a[n] * b[n] vector multiply and add
		p2 = vmlal_s16(p2, vget_high_s16(c8), vget_high_s16(d8)); // part[n] += a[n] * b[n] vector multiply and add

		a+=16;
		b+=16;
	}
	back = vpaddlq_s32(partial); // sum the 4 s32 in 2 64b
	back = vpadalq_s32(back, p2); // add to all these the 2nd partial
	back = vshrq_n_s64(back, 6); // shift by 6

	sum += vgetq_lane_s64(back, 0) + vgetq_lane_s64(back, 1);
	partial = vdupq_n_s32(0); // reset partial sum

	return sum;

}

/* intrinsics */
spx_int32_t ms_inner_product_neon_intrinsics(const spx_int16_t *a, const spx_int16_t *b, unsigned int len) {
	int16x8_t a8;
	int16x8_t b8;
	int32x4_t partial = vdupq_n_s32(0);
	int64x2_t back;

	len >>=3;

	while (len--) {
		a8 = vld1q_s16(a); // load 8 16b from a
		b8 = vld1q_s16(b); // load 8 16b from b

		partial = vmlal_s16(partial, vget_low_s16(a8), vget_low_s16(b8)); // part[n] += a[n] * b[n] vector multiply and add
		partial = vmlal_s16(partial, vget_high_s16(a8), vget_high_s16(b8)); // part[n] += a[n] * b[n] vector multiply and add
		a+=8;
		b+=8;
	}
	back = vpaddlq_s32(partial); // sum the 4 s32 in 2 64b
	back = vshrq_n_s64(back, 6); // shift by 6

	return vgetq_lane_s64(back, 0) + vgetq_lane_s64(back, 1);
}
#endif


static void inner_product_test(void) {
#ifdef SPEEX_LIB_CPU_FEATURE_NEON
#define SAMPLE_SIZE 64 /* has to be %8 and < 64 ! */
#define ITERATIONS 1000000
	static spx_int16_t test_sample[SAMPLE_SIZE];
	static spx_int16_t test_sample2[SAMPLE_SIZE];
	int length = SAMPLE_SIZE;
	uint64_t soft_ms, neon_ms;
	int i;
	volatile spx_int32_t non_neon_result;
	volatile spx_int32_t neon_result;
	float percent_off;
	bool_t fast_enough;

	// put some values to process
	for( i = 0; i<SAMPLE_SIZE; i++) {
		test_sample[i] = ortp_random() % 16384;
		test_sample2[i] = ortp_random() % 16384;
	}

	// ARM_NEON is enabled, we can force libspeex to use it
	libspeex_cpu_features = SPEEX_LIB_CPU_FEATURE_NEON;

	// disable neon & perform inner product
	libspeex_cpu_features &= ~SPEEX_LIB_CPU_FEATURE_NEON;
	i = ITERATIONS;
	{
		uint64_t start = ms_get_cur_time_ms();
		while (i--) {
			non_neon_result = inner_prod((const spx_int16_t*)test_sample, (const spx_int16_t*)test_sample2, length);
		}
		soft_ms = ms_get_cur_time_ms() - start;
	}

	// enable neon and perform the same operation
	libspeex_cpu_features |= SPEEX_LIB_CPU_FEATURE_NEON;
	i = ITERATIONS;
	{
		uint64_t start = ms_get_cur_time_ms();
		while (i--) {
			neon_result= inner_prod((const spx_int16_t*)test_sample, (const spx_int16_t*)test_sample2, length);
		}
		neon_ms = ms_get_cur_time_ms() - start;
	}

	percent_off = ((float)abs(non_neon_result-neon_result))/MAX(non_neon_result, neon_result)*100;
	ms_debug("%10d, NON Neon: %10d - diff: %d - percent off: %f",
	         non_neon_result, neon_result, abs(non_neon_result-neon_result), percent_off);

	fast_enough = (float)neon_ms < (float)soft_ms/5;

	// we expect the result to be very similar and at least 5 times faster with NEON
	BC_ASSERT(percent_off < 1.0);
	BC_ASSERT(fast_enough);
	ms_message("NEON = %" PRIu64 " ms, SOFT: %" PRIu64 " ms", neon_ms, soft_ms);
	if( !fast_enough ) {
		ms_error("NEON not fast enough it seems");
	}
#else
	ms_warning("Test skipped, not using the speex from git://git.linphone.org/speex.git with NEON support");
#endif

}

static test_t tests[] = {
	{ "Inner product", inner_product_test }
};

test_suite_t neon_test_suite = {
	"NEON",
	tester_before_all,
	tester_after_all,
	NULL,
	NULL,
	sizeof(tests)/sizeof(test_t),
	tests
};

#endif // ARM NEON
