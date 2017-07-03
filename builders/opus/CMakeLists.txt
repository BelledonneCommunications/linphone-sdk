############################################################################
# CMakeLists.txt
# Copyright (C) 2015  Belledonne Communications, Grenoble France
#
############################################################################
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

cmake_minimum_required(VERSION 3.0)
project(OPUS C)


option(ENABLE_STATIC "Build static library (default is shared library)." NO)
option(ENABLE_ASM "Enable assembly optimizations." YES)
option(ENABLE_ASSERTIONS "Enable additional software error checking." NO)
option(ENABLE_CUSTOM_MODES "Enable non-Opus modes, e.g. 44.1 kHz & 2^n frames." NO)
option(ENABLE_FIXED_POINT "Compile as fixed-point (for machines without a fast enough FPU)." NO)
option(ENABLE_FIXED_POINT_DEBUG "Debug fixed-point implementation." NO)
option(ENABLE_FLOAT_API "Compile with the floating point API (to disable for machines with no float library)." YES)
option(ENABLE_FLOAT_APPROX "Enable fast approximations for floating point." NO)
option(ENABLE_FUZZING "Causes the encoder to make random decisions." NO)
option(ENABLE_INTRINSICS "Enable intrinsics optimizations." YES)
option(ENABLE_RTCD "Enable run-time CPU capabilities detection." YES)


include(CheckCSourceCompiles)
include(CMakePushCheckState)

macro(check_intrinsics NAME_OF_INTRINSICS COMPILER_FLAG_FOR_INTRINSICS VAR_IF_PRESENT VAR_IF_DEFAULT TEST_PROGRAM_BODY)
	message(STATUS "Checking if compiler supports ${NAME_OF_INTRINSICS} intrinsics")
	check_c_source_compiles("${TEST_PROGRAM_BODY}" ${VAR_IF_PRESENT})
	if(${VAR_IF_PRESENT})
		set(${VAR_IF_DEFAULT} ${VAR_IF_PRESENT})
	else()
		message(STATUS "Checking if compiler supports ${NAME_OF_INTRINSICS} intrinsics with ${COMPILER_FLAG_FOR_INTRINSICS}")
		cmake_push_check_state(RESET)
		string(REPLACE ";" " " COMPILER_FLAG_FOR_INTRINSICS_STR "${COMPILER_FLAG_FOR_INTRINSICS}")
		set(CMAKE_REQUIRED_FLAGS "${COMPILER_FLAG_FOR_INTRINSICS_STR}")
		check_c_source_compiles("${TEST_PROGRAM_BODY}" ${VAR_IF_PRESENT}_FLAGS)
		if(${VAR_IF_PRESENT}_FLAGS)
			set(${VAR_IF_PRESENT} ${${VAR_IF_PRESENT}_FLAGS})
		endif()
		cmake_pop_check_state()
	endif()
endmacro()


string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SYSTEM_PROCESSOR)
if(SYSTEM_PROCESSOR MATCHES "^arm*hf*")
	set(ARM_NEON_INTR_CFLAGS "-mfpu=neon")
elseif(SYSTEM_PROCESSOR MATCHES "^arm*eabi*")
	set(ARM_NEON_INTR_CFLAGS "-mfpu=neon" "-mfloat-abi=softfp")
else()
	set(ARM_NEON_INTR_CFLAGS "-mfpu=neon")
endif()
set(X86_SSE_CFLAGS "-msse")
set(X86_SSE2_CFLAGS "-msse2")
set(X86_SSE4_1_CFLAGS "-msse4.1")
set(X86_AVX_CFLAGS "-mavx")


if(MSVC)
	set(USE_ALLOCA 1)
else()
	include(CheckIncludeFile)

	check_c_source_compiles("int main(int argc, char *argv[]) { static int x; char a[++x]; a[sizeof a - 1] = 0; int N; return a[0]; }" VAR_ARRAYS)
	if(NOT VAR_ARRAYS)
		check_include_file("alloca.h" HAVE_ALLOCA_H)
		if(HAVE_ALLOCA_H)
			check_c_source_compiles("#include <alloca.h>
	#include <stdlib.h>
	int main(int argc, char *argv[]) {
	int foo=10;
	int *array = alloca(foo);
	}"
				USE_ALLOCA)
		endif()
	endif()
endif()

if(ENABLE_ASM)
	set(ARCH_OPTIONS "")
	if(CMAKE_OSX_ARCHITECTURES)
		set(ARCH_OPTIONS "${ARCH_OPTIONS} -arch ${CMAKE_OSX_ARCHITECTURES}")
	endif()
	if(CMAKE_OSX_SYSROOT)
		set(ARCH_OPTIONS "${ARCH_OPTIONS} -isysroot ${CMAKE_OSX_SYSROOT}")
	endif()
	get_directory_property(DIRECTORY_PROPERTY_COMPILE_OPTIONS "COMPILE_OPTIONS")
	string(REPLACE ";" " " DIRECTORY_PROPERTY_COMPILE_OPTIONS "${DIRECTORY_PROPERTY_COMPILE_OPTIONS}")
	set(CMAKE_ASM_TARGET_FLAG "")
	if(CMAKE_C_COMPILER_TARGET)
		set(CMAKE_ASM_TARGET_FLAG "--target=${CMAKE_C_COMPILER_TARGET}")
	endif()
	string(TOUPPER "${CMAKE_BUILD_TYPE}" UPPER_BUILD_TYPE)
	set(CMAKE_ASM_COMPILE_OBJECT "<CMAKE_ASM_COMPILER> ${CMAKE_ASM_TARGET_FLAG} ${CMAKE_CPP_FLAGS} ${CMAKE_C_FLAGS} ${DIRECTORY_PROPERTY_COMPILE_OPTIONS} ${ARCH_OPTIONS} ${CMAKE_CPP_FLAGS_${UPPER_BUILD_TYPE}} ${CMAKE_C_FLAGS_${UPPER_BUILD_TYPE}} -o <OBJECT> -c <SOURCE>")
	enable_language(ASM)
	#No optimization available yet for arm64
	if(SYSTEM_PROCESSOR MATCHES "^arm.*")
		if(ENABLE_FIXED_POINT)
			set(OPUS_ARM_ASM 1)
			check_c_source_compiles("
#ifdef __GNUC_MINOR__
#if (__GNUC__ * 1000 + __GNUC_MINOR__) < 3004
#error GCC before 3.4 has critical bugs compiling inline assembly
#endif
#endif
int main(int argc, char *argv[]) { __asm__ (\"\"); }
" OPUS_ARM_INLINE_ASM)
			if(OPUS_ARM_INLINE_ASM)
				check_c_source_compiles("int main(int argc, char *argv[]) { __asm__(\"qadd r3,r3,r3\"); }" OPUS_ARM_INLINE_EDSP)
				check_c_source_compiles("int main(int argc, char *argv[]) { __asm__(\"shadd8 r3,r3,r3\"); }" OPUS_ARM_INLINE_MEDIA)
				check_c_source_compiles("int main(int argc, char *argv[]) { __asm__(\"vorr d0,d0,d0\"); }" OPUS_ARM_INLINE_NEON)
			endif()
			find_package(Perl)
			if(PERL_FOUND)
				set(OPUS_ARM_EXTERNAL_ASM 1)
				if(OPUS_ARM_INLINE_EDSP)
					set(OPUS_ARM_PRESUME_EDSP 1)
					set(OPUS_ARM_MAY_HAVE_EDSP 1)
				endif()
				if(OPUS_ARM_INLINE_MEDIA)
					set(OPUS_ARM_PRESUME_MEDIA 1)
					set(OPUS_ARM_MAY_HAVE_MEDIA 1)
				endif()
				if(OPUS_ARM_INLINE_NEON)
					set(OPUS_ARM_PRESUME_NEON 1)
					set(OPUS_ARM_MAY_HAVE_NEON 1)
				endif()
				if(ENABLE_RTCD)
					if(NOT OPUS_ARM_MAY_HAVE_EDSP)
						message(STATUS "Trying to force-enable armv5e EDSP instructions...")
						check_c_source_compiles("int main(int argc, char *argv[]) { __asm__(\".arch armv5te\\\\n.object_arch armv4t\\\\nqadd r3,r3,r3\"); }" OPUS_ARM_MAY_HAVE_EDSP)
					endif()
					if(NOT OPUS_ARM_MAY_HAVE_MEDIA)
						message(STATUS "Trying to force-enable ARMv6 media instructions...")
						check_c_source_compiles("int main(int argc, char *argv[]) { __asm__(\".arch armv6\\\\n.object_arch armv4t\\\\nshadd8 r3,r3,r3\"); }" OPUS_ARM_MAY_HAVE_MEDIA)
					endif()
					if(NOT OPUS_ARM_MAY_HAVE_NEON)
						message(STATUS "Trying to force-enable NEON instructions...")
						check_c_source_compiles("int main(int argc, char *argv[]) { __asm__(\".arch armv7-a\\\\n.fpu neon\\\\n.object_arch armv4t\\\\nvorr d0,d0,d0\"); }" OPUS_ARM_MAY_HAVE_NEON)
					endif()
				endif()
				set(RTCD_SUPPORT "")
				if(OPUS_ARM_MAY_HAVE_EDSP AND NOT OPUS_ARM_PRESUME_EDSP)
					set(RTCD_SUPPORT "${RTCD_SUPPORT} (EDSP)")
				endif()
				if(OPUS_ARM_MAY_HAVE_MEDIA AND NOT OPUS_ARM_PRESUME_MEDIA)
					set(RTCD_SUPPORT "${RTCD_SUPPORT} (Media)")
				endif()
				if(OPUS_ARM_MAY_HAVE_NEON AND NOT OPUS_ARM_PRESUME_NEON)
					set(RTCD_SUPPORT "${RTCD_SUPPORT} (NEON)")
				endif()
				if(RTCD_SUPPORT)
					set(RTCD_SUPPORT "ARM${RTCD_SUPPORT}")
				endif()
			endif()
		endif()
	endif()
endif()

if(ENABLE_INTRINSICS)
	if(SYSTEM_PROCESSOR MATCHES "^arm.*" OR SYSTEM_PROCESSOR MATCHES "^aarch64.*")
		check_intrinsics("ARM Neon" "${ARM_NEON_INTR_CFLAGS}" OPUS_ARM_MAY_HAVE_NEON_INTR OPUS_ARM_PRESUME_NEON_INTR "
#include <arm_neon.h>
int main(int argc, char *argv[]) {
	static float32x4_t A0, A1, SUMM;
	SUMM = vmlaq_f32(SUMM, A0, A1);
	return (int)vgetq_lane_f32(SUMM, 0);
}")
		if(OPUS_ARM_MAY_HAVE_NEON_INTR)
			check_intrinsics("Aarch64 Neon" "${ARM_NEON_INTR_CFLAGS}" OPUS_ARM_MAY_HAVE_AARCH64_NEON_INTR OPUS_ARM_PRESUME_AARCH64_NEON_INTR "
#include <arm_neon.h>
int main(int argc, char *argv[]) {
	static int32_t IN;
	static int16_t OUT;
	OUT = vqmovns_s32(IN);
}")
		else()
			message(WARNING "Compiler does not support ARM intrinsics")
		endif()
	elseif(SYSTEM_PROCESSOR MATCHES "^i.?86.*" OR SYSTEM_PROCESSOR MATCHES "^x86.*" OR SYSTEM_PROCESSOR MATCHES "^x86_64.*" OR SYSTEM_PROCESSOR MATCHES "^amd64.*")
		check_intrinsics("SSE" "${X86_SSE_CFLAGS}" OPUS_X86_MAY_HAVE_SSE OPUS_X86_PRESUME_SSE "
#include <xmmintrin.h>
#include <time.h>
int main(int argc, char *argv[]) {
	__m128 mtest;
	mtest = _mm_set1_ps((float)time(NULL));
	mtest = _mm_mul_ps(mtest, mtest);
	return _mm_cvtss_si32(mtest);
}")
		check_intrinsics("SSE2" "${X86_SSE2_CFLAGS}" OPUS_X86_MAY_HAVE_SSE2 OPUS_X86_PRESUME_SSE2 "
#include <emmintrin.h>
#include <time.h>
int main(int argc, char *argv[]) {
	__m128i mtest;
	mtest = _mm_set1_epi32((int)time(NULL));
	mtest = _mm_mul_epu32(mtest, mtest);
	return _mm_cvtsi128_si32(mtest);
}")
		check_intrinsics("SSE4.1" "${X86_SSE4_1_CFLAGS}" OPUS_X86_MAY_HAVE_SSE4_1 OPUS_X86_PRESUME_SSE4_1 "
#include <smmintrin.h>
#include <time.h>
int main(int argc, char *argv[]) {
	__m128i mtest;
	mtest = _mm_set1_epi32((int)time(NULL));
	mtest = _mm_mul_epi32(mtest, mtest);
	return _mm_cvtsi128_si32(mtest);
}")
		check_intrinsics("AVX" "${X86_AVX_CFLAGS}" OPUS_X86_MAY_HAVE_AVX OPUS_X86_PRESUME_AVX "
#include <immintrin.h>
#include <time.h>
int main(int argc, char *argv[]) {
	__m256 mtest;
	mtest = _mm256_set1_ps((float)time(NULL));
	mtest = _mm256_addsub_ps(mtest, mtest);
	return _mm_cvtss_si32(_mm256_extractf128_ps(mtest, 0));
}")
		set(RTCD_SUPPORT "")
		if(OPUS_X86_MAY_HAVE_SSE AND NOT OPUS_X86_PRESUME_SSE)
			set(RTCD_SUPPORT "${RTCD_SUPPORT} SSE")
		endif()
		if(OPUS_X86_MAY_HAVE_SSE2 AND NOT OPUS_X86_PRESUME_SSE2)
			set(RTCD_SUPPORT "${RTCD_SUPPORT} SSE2")
		endif()
		if(OPUS_X86_MAY_HAVE_SSE4_1 AND NOT OPUS_X86_PRESUME_SSE4_1)
			set(RTCD_SUPPORT "${RTCD_SUPPORT} SSE4.1")
		endif()
		if(OPUS_X86_MAY_HAVE_AVX AND NOT OPUS_X86_PRESUME_AVX)
			set(RTCD_SUPPORT "${RTCD_SUPPORT} AVX")
		endif()
		if(RTCD_SUPPORT)
			set(RTCD_SUPPORT "x86${RTCD_SUPPORT}")
		endif()
		if(ENABLE_RTCD AND RTCD_SUPPORT)
			check_c_source_compiles("
#include <stdio.h>
int main(int argc, char *argv[]) {
	unsigned int CPUInfo0;
	unsigned int CPUInfo1;
	unsigned int CPUInfo2;
	unsigned int CPUInfo3;
	unsigned int InfoType;
	__asm__ __volatile__ (
		\"cpuid\":
		\"=a\" (CPUInfo0),
		\"=b\" (CPUInfo1),
		\"=c\" (CPUInfo2),
		\"=d\" (CPUInfo3) :
		\"a\" (InfoType), \"c\" (0)
	);
}" CPU_INFO_BY_ASM)
			if(NOT CPU_INFO_BY_ASM)
				check_c_source_compiles("
#include <cpuid.h>
int main(int argc, char *argv[]) {
	unsigned int CPUInfo0;
	unsigned int CPUInfo1;
	unsigned int CPUInfo2;
	unsigned int CPUInfo3;
	unsigned int InfoType;
	__get_cpuid(InfoType, &CPUInfo0, &CPUInfo1, &CPUInfo2, &CPUInfo3);
}" CPU_INFO_BY_C)
			endif()
		endif()
	else()
		message(WARNING "No intrinsics support for your architecture")
		set(ENABLE_INTRINSICS OFF CACHE BOOL "Enable intrinsics optimizations." FORCE)
	endif()
endif()

if(ENABLE_FIXED_POINT)
	set(FIXED_POINT 1)
endif()
if(ENABLE_FIXED_POINT_DEBUG)
	set(FIXED_DEBUG 1)
endif()
if(NOT ENABLE_FLOAT_API)
	set(DISABLE_FLOAT_API 1)
endif()
if(ENABLE_CUSTOM_MODES)
	set(CUSTOM_MODES 1)
endif()
if(ENABLE_FLOAT_APPROX)
	set(FLOAT_APPROX 1)
endif()
if(ENABLE_FUZZING)
	set(FUZZING 1)
endif()
if(RTCD_SUPPORT)
	set(OPUS_HAVE_RTCD 1)
endif()

add_definitions("-DOPUS_BUILD")
if(MSVC AND NOT ENABLE_STATIC)
	add_definitions("-DDLL_EXPORT")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)


include_directories(
	include
	celt
	silk
	silk/float
	silk/fixed
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
)
if(WIN32)
	include_directories(win32)
endif()
if (APPLE)
	set(ARM2GNU_PARAMS "--apple")
endif()


set(CELT_SOURCES
	celt/bands.c
	celt/celt.c
	celt/celt_encoder.c
	celt/celt_decoder.c
	celt/cwrs.c
	celt/entcode.c
	celt/entdec.c
	celt/entenc.c
	celt/kiss_fft.c
	celt/laplace.c
	celt/mathops.c
	celt/mdct.c
	celt/modes.c
	celt/pitch.c
	celt/celt_lpc.c
	celt/quant_bands.c
	celt/rate.c
	celt/vq.c
)

set(CELT_SOURCES_ARM
	celt/arm/armcpu.c
	celt/arm/arm_celt_map.c
)

set(CELT_SOURCES_ARM_ASM
	${CMAKE_CURRENT_SOURCE_DIR}/celt/arm/celt_pitch_xcorr_arm.s
)
if(OPUS_ARM_EXTERNAL_ASM)
	set(CELT_SOURCES_EXTERNAL_ARM_ASM
		${CMAKE_CURRENT_BINARY_DIR}/celt/arm/celt_pitch_xcorr_arm-gnu.S
	)
	set_source_files_properties(${CELT_SOURCES_EXTERNAL_ARM_ASM} PROPERTIES GENERATED TRUE)
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/celt/arm/celt_pitch_xcorr_arm-gnu.S
		COMMAND "${PERL_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/celt/arm/arm2gnu.pl" ${ARM2GNU_PARAMS} "<" "${CELT_SOURCES_ARM_ASM}" ">" "${CELT_SOURCES_EXTERNAL_ARM_ASM}")
endif()

if(OPUS_ARM_ASM)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/celt/arm/armopts.s.in ${CMAKE_CURRENT_BINARY_DIR}/celt/arm/armopts.s @ONLY)
	set(CELT_AM_SOURCES_ARM_ASM
		${CMAKE_CURRENT_BINARY_DIR}/celt/arm/armopts.s
	)
endif()
if(OPUS_ARM_EXTERNAL_ASM)
	set(CELT_AM_SOURCES_EXTERNAL_ARM_ASM
		${CMAKE_CURRENT_BINARY_DIR}/celt/arm/armopts-gnu.S
	)
	set_source_files_properties(${CELT_AM_SOURCES_EXTERNAL_ARM_ASM} PROPERTIES GENERATED TRUE)
	add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/celt/arm/armopts-gnu.S
		COMMAND "${PERL_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/celt/arm/arm2gnu.pl" ${ARM2GNU_PARAMS} "<" "${CELT_AM_SOURCES_ARM_ASM}" ">" "${CELT_AM_SOURCES_EXTERNAL_ARM_ASM}")
endif()
if(OPUS_ARM_MAY_HAVE_NEON_INTR)
	set(CELT_SOURCES_ARM_NEON_INTR
		celt/arm/celt_neon_intr.c
	)
	set_source_files_properties(${CELT_SOURCES_ARM_NEON_INTR} PROPERTIES COMPILE_FLAGS "${ARM_NEON_INTR_CFLAGS}")
endif()
if(OPUS_X86_MAY_HAVE_SSE)
	set(CELT_SOURCES_SSE
		celt/x86/x86cpu.c
		celt/x86/x86_celt_map.c
		celt/x86/pitch_sse.c
	)
	set_source_files_properties(${CELT_SOURCES_SSE} PROPERTIES COMPILE_FLAGS "${X86_SSE_CFLAGS}")
endif()
if(OPUS_X86_MAY_HAVE_SSE2)
	set(CELT_SOURCES_SSE2 celt/x86/pitch_sse2.c)
	set_source_files_properties(${CELT_SOURCES_SSE2} PROPERTIES COMPILE_FLAGS "${X86_SSE2_CFLAGS}")
endif()
if(OPUS_X86_MAY_HAVE_SSE4_1)
	set(CELT_SOURCES_SSE4_1
		celt/x86/celt_lpc_sse.c
		celt/x86/pitch_sse4_1.c
	)
	set_source_files_properties(${CELT_SOURCES_SSE4_1} PROPERTIES COMPILE_FLAGS "${X86_SSE4_1_CFLAGS}")
endif()

set(SILK_SOURCES
	silk/CNG.c
	silk/code_signs.c
	silk/init_decoder.c
	silk/decode_core.c
	silk/decode_frame.c
	silk/decode_parameters.c
	silk/decode_indices.c
	silk/decode_pulses.c
	silk/decoder_set_fs.c
	silk/dec_API.c
	silk/enc_API.c
	silk/encode_indices.c
	silk/encode_pulses.c
	silk/gain_quant.c
	silk/interpolate.c
	silk/LP_variable_cutoff.c
	silk/NLSF_decode.c
	silk/NSQ.c
	silk/NSQ_del_dec.c
	silk/PLC.c
	silk/shell_coder.c
	silk/tables_gain.c
	silk/tables_LTP.c
	silk/tables_NLSF_CB_NB_MB.c
	silk/tables_NLSF_CB_WB.c
	silk/tables_other.c
	silk/tables_pitch_lag.c
	silk/tables_pulses_per_block.c
	silk/VAD.c
	silk/control_audio_bandwidth.c
	silk/quant_LTP_gains.c
	silk/VQ_WMat_EC.c
	silk/HP_variable_cutoff.c
	silk/NLSF_encode.c
	silk/NLSF_VQ.c
	silk/NLSF_unpack.c
	silk/NLSF_del_dec_quant.c
	silk/process_NLSFs.c
	silk/stereo_LR_to_MS.c
	silk/stereo_MS_to_LR.c
	silk/check_control_input.c
	silk/control_SNR.c
	silk/init_encoder.c
	silk/control_codec.c
	silk/A2NLSF.c
	silk/ana_filt_bank_1.c
	silk/biquad_alt.c
	silk/bwexpander_32.c
	silk/bwexpander.c
	silk/debug.c
	silk/decode_pitch.c
	silk/inner_prod_aligned.c
	silk/lin2log.c
	silk/log2lin.c
	silk/LPC_analysis_filter.c
	silk/LPC_inv_pred_gain.c
	silk/table_LSF_cos.c
	silk/NLSF2A.c
	silk/NLSF_stabilize.c
	silk/NLSF_VQ_weights_laroia.c
	silk/pitch_est_tables.c
	silk/resampler.c
	silk/resampler_down2_3.c
	silk/resampler_down2.c
	silk/resampler_private_AR2.c
	silk/resampler_private_down_FIR.c
	silk/resampler_private_IIR_FIR.c
	silk/resampler_private_up2_HQ.c
	silk/resampler_rom.c
	silk/sigm_Q15.c
	silk/sort.c
	silk/sum_sqr_shift.c
	silk/stereo_decode_pred.c
	silk/stereo_encode_pred.c
	silk/stereo_find_predictor.c
	silk/stereo_quant_pred.c
)

set(SILK_SOURCES_FIXED
	silk/fixed/LTP_analysis_filter_FIX.c
	silk/fixed/LTP_scale_ctrl_FIX.c
	silk/fixed/corrMatrix_FIX.c
	silk/fixed/encode_frame_FIX.c
	silk/fixed/find_LPC_FIX.c
	silk/fixed/find_LTP_FIX.c
	silk/fixed/find_pitch_lags_FIX.c
	silk/fixed/find_pred_coefs_FIX.c
	silk/fixed/noise_shape_analysis_FIX.c
	silk/fixed/prefilter_FIX.c
	silk/fixed/process_gains_FIX.c
	silk/fixed/regularize_correlations_FIX.c
	silk/fixed/residual_energy16_FIX.c
	silk/fixed/residual_energy_FIX.c
	silk/fixed/solve_LS_FIX.c
	silk/fixed/warped_autocorrelation_FIX.c
	silk/fixed/apply_sine_window_FIX.c
	silk/fixed/autocorr_FIX.c
	silk/fixed/burg_modified_FIX.c
	silk/fixed/k2a_FIX.c
	silk/fixed/k2a_Q16_FIX.c
	silk/fixed/pitch_analysis_core_FIX.c
	silk/fixed/vector_ops_FIX.c
	silk/fixed/schur64_FIX.c
	silk/fixed/schur_FIX.c
)

set(SILK_SOURCES_FLOAT
	silk/float/apply_sine_window_FLP.c
	silk/float/corrMatrix_FLP.c
	silk/float/encode_frame_FLP.c
	silk/float/find_LPC_FLP.c
	silk/float/find_LTP_FLP.c
	silk/float/find_pitch_lags_FLP.c
	silk/float/find_pred_coefs_FLP.c
	silk/float/LPC_analysis_filter_FLP.c
	silk/float/LTP_analysis_filter_FLP.c
	silk/float/LTP_scale_ctrl_FLP.c
	silk/float/noise_shape_analysis_FLP.c
	silk/float/prefilter_FLP.c
	silk/float/process_gains_FLP.c
	silk/float/regularize_correlations_FLP.c
	silk/float/residual_energy_FLP.c
	silk/float/solve_LS_FLP.c
	silk/float/warped_autocorrelation_FLP.c
	silk/float/wrappers_FLP.c
	silk/float/autocorrelation_FLP.c
	silk/float/burg_modified_FLP.c
	silk/float/bwexpander_FLP.c
	silk/float/energy_FLP.c
	silk/float/inner_product_FLP.c
	silk/float/k2a_FLP.c
	silk/float/levinsondurbin_FLP.c
	silk/float/LPC_inv_pred_gain_FLP.c
	silk/float/pitch_analysis_core_FLP.c
	silk/float/scale_copy_vector_FLP.c
	silk/float/scale_vector_FLP.c
	silk/float/schur_FLP.c
	silk/float/sort_FLP.c
)

if(OPUS_ARM_MAY_HAVE_NEON_INTR)
	set(SILK_SOURCES_ARM_NEON_INTR
		silk/arm/arm_silk_map.c
		silk/arm/NSQ_neon.c
	)
	set_source_files_properties(${SILK_SOURCES_ARM_NEON_INTR} PROPERTIES COMPILE_FLAGS "${ARM_NEON_INTR_CFLAGS}")
endif()
if(OPUS_X86_MAY_HAVE_SSE4_1)
	set(SILK_SOURCES_SSE4_1
		silk/x86/NSQ_sse.c
		silk/x86/NSQ_del_dec_sse.c
		silk/x86/x86_silk_map.c
		silk/x86/VAD_sse.c
		silk/x86/VQ_WMat_EC_sse.c
	)
	set_source_files_properties(${SILK_SOURCES_SSE4_1} PROPERTIES COMPILE_FLAGS "${X86_SSE4_1_CFLAGS}")
	if(ENABLE_FIXED_POINT)
		set(SILK_SOURCES_FIXED_SSE4_1
			silk/fixed/x86/vector_ops_FIX_sse.c
			silk/fixed/x86/burg_modified_FIX_sse.c
			silk/fixed/x86/prefilter_FIX_sse.c
		)
		set_source_files_properties(${SILK_SOURCES_FIXED_SSE4_1} PROPERTIES COMPILE_FLAGS "${X86_SSE4_1_CFLAGS}")
	endif()
endif()

set(OPUS_SOURCES
	src/opus.c
	src/opus_decoder.c
	src/opus_encoder.c
	src/opus_multistream.c
	src/opus_multistream_encoder.c
	src/opus_multistream_decoder.c
	src/repacketizer.c
)

set(OPUS_SOURCES_FLOAT
	src/analysis.c
	src/mlp.c
	src/mlp_data.c
)

if(ENABLE_FIXED_POINT)
	list(APPEND SILK_SOURCES ${SILK_SOURCES_FIXED})
else()
	list(APPEND SILK_SOURCES ${SILK_SOURCES_FLOAT})
endif()
if(SILK_SOURCES_ARM_NEON_INTR)
	list(APPEND SILK_SOURCES ${SILK_SOURCES_ARM_NEON_INTR})
endif()
if(SILK_SOURCES_SSE4_1)
	list(APPEND SILK_SOURCES ${SILK_SOURCES_SSE4_1})
endif()
if(SILK_SOURCES_FIXED_SSE4_1)
	list(APPEND SILK_SOURCES ${SILK_SOURCES_FIXED_SSE4_1})
endif()

if (ENABLE_FLOAT_API)
	list(APPEND OPUS_SOURCES ${OPUS_SOURCES_FLOAT})
endif()

if(OPUS_ARM_ASM)
	list(APPEND CELT_SOURCES ${CELT_SOURCES_ARM})
endif()
if(OPUS_ARM_EXTERNAL_ASM)
	list(APPEND CELT_SOURCES ${CELT_SOURCES_EXTERNAL_ARM_ASM} ${CELT_AM_SOURCES_EXTERNAL_ARM_ASM})
endif()
if(CELT_SOURCES_ARM_NEON_INTR)
	list(APPEND CELT_SOURCES ${CELT_SOURCES_ARM_NEON_INTR})
endif()
if(CELT_SOURCES_SSE)
	list(APPEND CELT_SOURCES ${CELT_SOURCES_SSE})
endif()
if(CELT_SOURCES_SSE2)
	list(APPEND CELT_SOURCES ${CELT_SOURCES_SSE2})
endif()
if(CELT_SOURCES_SSE4_1)
	list(APPEND CELT_SOURCES ${CELT_SOURCES_SSE4_1})
endif()
if(MSVC AND CMAKE_GENERATOR MATCHES "^.* ARM")
	add_definitions(-DNO_INLINE_ASM)
endif()


if(ENABLE_STATIC)
	add_library(opus STATIC ${CELT_SOURCES} ${SILK_SOURCES} ${OPUS_SOURCES})
else()
	add_library(opus SHARED ${CELT_SOURCES} ${SILK_SOURCES} ${OPUS_SOURCES})
	set_target_properties(opus PROPERTIES VERSION 5 LINKER_LANGUAGE C)
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/opus.pdb
				DESTINATION bin
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
	endif()
endif()
target_compile_options(opus PRIVATE "-w")
target_compile_definitions(opus PRIVATE "-DHAVE_CONFIG_H")
if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	set_source_files_properties(src/opus_decoder.c PROPERTIES COMPILE_FLAGS "/wd4703")
endif()

install(TARGETS opus
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

file(GLOB HEADER_FILES "include/*.h")

install(FILES ${HEADER_FILES}
	DESTINATION include/opus
	PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
)
