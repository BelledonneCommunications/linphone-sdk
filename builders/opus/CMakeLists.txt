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
option(ENABLE_INTRINSICS "Enable intrinsics optimizations for ARM(float) X86(fixed)." NO)
option(ENABLE_RTCD "Enable run-time CPU capabilities detection." YES)


if(MSVC)
	set(USE_ALLOCA 1)
else()
	include(CheckCSourceCompiles)
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
	set(CMAKE_ASM_COMPILE_OBJECT "<CMAKE_ASM_COMPILER> ${CMAKE_ASM_TARGET_FLAG} ${CMAKE_CPP_FLAGS} ${CMAKE_C_FLAGS} ${DIRECTORY_PROPERTY_COMPILE_OPTIONS} ${ARCH_OPTIONS} -o <OBJECT> -c <SOURCE>")
	enable_language(ASM)
	string(TOLOWER ${CMAKE_SYSTEM_PROCESSOR} SYSTEM_PROCESSOR)
	#No optimization available yet for arm64
	if(SYSTEM_PROCESSOR MATCHES "^arm.*")
		if(ENABLE_FIXED_POINT)
			set(OPUS_ARM_ASM 1)
			include(CheckCSourceCompiles)
			check_c_source_compiles("
#ifdef __GNUC_MINOR__
#if (__GNUC__ * 1000 + __GNUC_MINOR__) < 3004
#error GCC before 3.4 has critical bugs compiling inline assembly
#endif
#endif
int main(int argc, char *argv[]) { __asm__ (\"\"); }
" OPUS_ARM_INLINE_ASM)
			check_c_source_compiles("int main(int argc, char *argv[]) { __asm__(\"qadd r3,r3,r3\"); }" OPUS_ARM_INLINE_EDSP)
			check_c_source_compiles("int main(int argc, char *argv[]) { __asm__(\"shadd8 r3,r3,r3\"); }" OPUS_ARM_INLINE_MEDIA)
			check_c_source_compiles("int main(int argc, char *argv[]) { __asm__(\"vorr d0,d0,d0\"); }" OPUS_ARM_INLINE_NEON)
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
						check_c_source_compiles("int main(int argc, char *argv[]) { __asm__(\".arch armv5te\\\\n.object_arch armv4t\\\\nqadd r3,r3,r3\"); }" OPUS_ARM_MAY_HAVE_EDSP)
					endif()
					if(NOT OPUS_ARM_MAY_HAVE_MEDIA)
						check_c_source_compiles("int main(int argc, char *argv[]) { __asm__(\".arch armv6\\\\n.object_arch armv4t\\\\nshadd8 r3,r3,r3\"); }" OPUS_ARM_MAY_HAVE_MEDIA)
					endif()
					if(NOT OPUS_ARM_MAY_HAVE_NEON)
						check_c_source_compiles("int main(int argc, char *argv[]) { __asm__(\".arch armv7-a\\\\n.fpu neon\\\\n.object_arch armv4t\\\\nvorr d0,d0,d0\"); }" OPUS_ARM_MAY_HAVE_NEON)
					endif()
				endif()
				set(RTCD_SUPPORT "")
				if(OPUS_ARM_MAY_HAVE_EDSP)
					if(NOT OPUS_ARM_PRESUME_EDSP)
						set(RTCD_SUPPORT "${RTCD_SUPPORT} (EDSP)")
					endif()
				endif()
				if(OPUS_ARM_MAY_HAVE_MEDIA)
					if(NOT OPUS_ARM_PRESUME_MEDIA)
						set(RTCD_SUPPORT "${RTCD_SUPPORT} (Media)")
					endif()
				endif()
				if(OPUS_ARM_MAY_HAVE_NEON)
					if(NOT OPUS_ARM_PRESUME_NEON)
						set(RTCD_SUPPORT "${RTCD_SUPPORT} (NEON)")
					endif()
				endif()
				if(RTCD_SUPPORT)
					set(RTCD_SUPPORT "ARM${RTCD_SUPPORT}")
				endif()
			endif()
		endif()
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

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)


include_directories(
	include
	celt
	silk
	silk/float
	silk/fixed
	${CMAKE_CURRENT_BINARY_DIR}
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
if(OPUS_ARM_PRESUME_NEON AND ENABLE_INTRINSICS)
	set(CELT_AM_SOURCES_NEON_INTRINSICS
		celt/arm/celt_neon_intr.c
	)
	if(ANDROID)
		set_source_files_properties(${CELT_AM_SOURCES_NEON_INTRINSICS} PROPERTIES COMPILE_FLAGS "-mfpu=neon")
	endif()
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

if (ENABLE_FLOAT_API)
	list(APPEND OPUS_SOURCES ${OPUS_SOURCES_FLOAT})
endif()

if(OPUS_ARM_ASM)
	list(APPEND CELT_SOURCES ${CELT_SOURCES_ARM})
	if(OPUS_ARM_EXTERNAL_ASM)
		list(APPEND CELT_SOURCES ${CELT_SOURCES_EXTERNAL_ARM_ASM} ${CELT_AM_SOURCES_EXTERNAL_ARM_ASM})
	endif()
	if(CELT_AM_SOURCES_NEON_INTRINSICS)
		list(APPEND CELT_SOURCES ${CELT_AM_SOURCES_NEON_INTRINSICS})
	endif()
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
