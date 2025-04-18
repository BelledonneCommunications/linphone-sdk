############################################################################
# CMakeLists.txt
# Copyright (C) 2014-2024  Belledonne Communications, Grenoble France
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

cmake_minimum_required(VERSION 3.22)

project(MSWebRTC VERSION 5.4.0 LANGUAGES C CXX)

if(UNIX AND NOT APPLE)
	add_link_options("LINKER:-no-undefined") 
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/mediastreamer2/plugins")

set(PACKAGE "${PROJECT_NAME}")
set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "support@belledonne-communications.com")
set(PACKAGE_TARNAME "mswebrtc")
set(PACKAGE_URL "")
set(VERSION "${PACKAGE_VERSION}")

option(ENABLE_AEC "Build the WebRTC echo canceller support." YES)
option(ENABLE_VAD "Build the WebRTC VAD support" YES)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CheckCXXCompilerFlag)

if(NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

find_package(BCToolbox 5.3.0 REQUIRED)
find_package(Mediastreamer2 5.3.0 REQUIRED)

find_library(LIBM NAMES m)

if(ENABLE_AEC)
	set(BUILD_AEC 1)
endif()
if(ENABLE_VAD)
	set(BUILD_VAD 1)
endif()

add_definitions(-DWEBRTC_APM_DEBUG_DUMP=0)
add_definitions(-DRTC_DISABLE_LOGGING=1)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake" "${CMAKE_CURRENT_BINARY_DIR}/config.h")

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(-DHAVE_CONFIG_H)

if(CMAKE_SYSTEM_NAME STREQUAL "WindowsPhone" AND CMAKE_GENERATOR_PLATFORM STREQUAL "ARM")
	add_definitions(
		-D__ARMEL__
		-DWEBRTC_ARCH_ARM
	)
elseif(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore" AND CMAKE_GENERATOR MATCHES "^.* ARM$")
	add_definitions(
		-D__ARMEL__
		-DWEBRTC_ARCH_ARM
	)
endif()

if(UNIX)
    add_definitions(-DWEBRTC_POSIX)
	if(APPLE)
		add_definitions(-DWEBRTC_MAC)
		if(IOS)
			add_definitions(-DWEBRTC_IOS)
		endif()
	else()
		add_definitions(-DWEBRTC_LINUX)
		if(ANDROID)
			add_definitions(-DWEBRTC_ANDROID)
		endif()
	endif()
elseif(WIN32)
    add_definitions(-DWEBRTC_WIN)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(COMPILER_IS_CLANG TRUE)
else()
	set(COMPILER_IS_CLANG FALSE)
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SYSTEM_PROCESSOR)
string(REGEX MATCH "^(arm*|aarch64)" FIXED_POINT_PROCESSOR "${SYSTEM_PROCESSOR}")
if(FIXED_POINT_PROCESSOR)
	add_definitions(-D__ARMEL__)
endif()

# Detect ARM architecture
if(SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
	add_definitions(-DWEBRTC_ARCH_ARM64)
endif()
if(SYSTEM_PROCESSOR MATCHES "arm" OR SYSTEM_PROCESSOR MATCHES "aarch64")
	if(SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
		add_definitions(-DWEBRTC_ARCH_ARM64)
		set(COMPILER_SUPPORTS_NEON FALSE)
		check_cxx_compiler_flag(" -march=armv8-a" COMPILER_SUPPORTS_ARMV8)
			if(COMPILER_SUPPORTS_ARMV8)
				add_definitions(-DWEBRTC_HAS_NEON)
			endif()
	else()
		add_definitions(-DWEBRTC_ARCH_ARM)
		set(COMPILER_SUPPORTS_ARMV8 FALSE)		
		if(SYSTEM_PROCESSOR MATCHES "armv[7-9]")
			add_definitions(-DWEBRTC_ARCH_ARM_V7)
			check_cxx_compiler_flag("-mfpu=neon" COMPILER_SUPPORTS_NEON)
			if(COMPILER_SUPPORTS_NEON)
				add_definitions(-DWEBRTC_HAS_NEON)
			endif()
		endif()

	endif()
else()
    # Check for SSE support on non-ARM architectures
	if (MSVC)
		set(COMPILER_SUPPORTS_SSE2 TRUE)
	else()
		check_cxx_compiler_flag("-msse2" COMPILER_SUPPORTS_SSE2)
	endif()
endif()

set(AEC3_CXX17_FLAGS "-std=c++17")
set(AEC3_CXX14_FLAGS "-std=c++14")

set(AUDIO_PROCESSING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules/audio_processing")
set(AEC3_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules/audio_processing/aec3")
set(RTCBASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/rtc_base")
set(COMMON_AUDIO_SIGNAL_PROC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/common_audio/signal_processing")
set(VAD_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/common_audio/vad")

set(WEBRTC_SRC
	mswebrtc.c
)

if(ENABLE_AEC)
	list(APPEND WEBRTC_SRC 
		aec3.cc
		mswebrtc_aec3.cc
		${CMAKE_CURRENT_SOURCE_DIR}/api/audio/echo_canceller3_config.cc
		${AUDIO_PROCESSING_DIR}/logging/apm_data_dumper.cc
		${AUDIO_PROCESSING_DIR}/audio_buffer.cc
		${AUDIO_PROCESSING_DIR}/high_pass_filter.cc
		${AUDIO_PROCESSING_DIR}/splitting_filter.cc
		${AUDIO_PROCESSING_DIR}/three_band_filter_bank.cc
		${AUDIO_PROCESSING_DIR}/utility/cascaded_biquad_filter.cc
		${AEC3_SRC_DIR}/echo_canceller3.cc
		${AEC3_SRC_DIR}/block_framer.cc
		${AEC3_SRC_DIR}/api_call_jitter_metrics.cc
		${AEC3_SRC_DIR}/adaptive_fir_filter.cc
		${AEC3_SRC_DIR}/adaptive_fir_filter_erl.cc
		${AEC3_SRC_DIR}/aec3_common.cc
		${AEC3_SRC_DIR}/aec3_fft.cc
		${AEC3_SRC_DIR}/aec_state.cc
		${AEC3_SRC_DIR}/alignment_mixer.cc
		${AEC3_SRC_DIR}/block_buffer.cc
		${AEC3_SRC_DIR}/block_delay_buffer.cc
		${AEC3_SRC_DIR}/block_processor.cc
		${AEC3_SRC_DIR}/block_processor_metrics.cc
		${AEC3_SRC_DIR}/clockdrift_detector.cc
		${AEC3_SRC_DIR}/coarse_filter_update_gain.cc
		${AEC3_SRC_DIR}/comfort_noise_generator.cc
		${AEC3_SRC_DIR}/config_selector.cc
		${AEC3_SRC_DIR}/decimator.cc
		${AEC3_SRC_DIR}/dominant_nearend_detector.cc
		${AEC3_SRC_DIR}/downsampled_render_buffer.cc
		${AEC3_SRC_DIR}/echo_audibility.cc
		${AEC3_SRC_DIR}/echo_path_delay_estimator.cc
		${AEC3_SRC_DIR}/echo_path_variability.cc
		${AEC3_SRC_DIR}/echo_remover.cc
		${AEC3_SRC_DIR}/echo_remover_metrics.cc
		${AEC3_SRC_DIR}/erle_estimator.cc
		${AEC3_SRC_DIR}/erl_estimator.cc
		${AEC3_SRC_DIR}/fft_buffer.cc
		${AEC3_SRC_DIR}/filter_analyzer.cc
		${AEC3_SRC_DIR}/frame_blocker.cc
		${AEC3_SRC_DIR}/fullband_erle_estimator.cc
		${AEC3_SRC_DIR}/matched_filter.cc
		${AEC3_SRC_DIR}/matched_filter_lag_aggregator.cc
		${AEC3_SRC_DIR}/moving_average.cc
		${AEC3_SRC_DIR}/multi_channel_content_detector.cc
		${AEC3_SRC_DIR}/refined_filter_update_gain.cc
		${AEC3_SRC_DIR}/render_buffer.cc
		${AEC3_SRC_DIR}/render_delay_buffer.cc
		${AEC3_SRC_DIR}/render_delay_controller.cc
		${AEC3_SRC_DIR}/render_delay_controller_metrics.cc
		${AEC3_SRC_DIR}/render_signal_analyzer.cc
		${AEC3_SRC_DIR}/residual_echo_estimator.cc
		${AEC3_SRC_DIR}/reverb_decay_estimator.cc
		${AEC3_SRC_DIR}/reverb_frequency_response.cc
		${AEC3_SRC_DIR}/reverb_model.cc
		${AEC3_SRC_DIR}/reverb_model_estimator.cc
		${AEC3_SRC_DIR}/signal_dependent_erle_estimator.cc
		${AEC3_SRC_DIR}/spectrum_buffer.cc
		${AEC3_SRC_DIR}/stationarity_estimator.cc
		${AEC3_SRC_DIR}/subband_erle_estimator.cc
		${AEC3_SRC_DIR}/subband_nearend_detector.cc
		${AEC3_SRC_DIR}/subtractor.cc
		${AEC3_SRC_DIR}/subtractor_output_analyzer.cc
		${AEC3_SRC_DIR}/subtractor_output.cc
		${AEC3_SRC_DIR}/suppression_filter.cc
		${AEC3_SRC_DIR}/suppression_gain.cc
		${AEC3_SRC_DIR}/transparent_mode.cc
		${CMAKE_CURRENT_SOURCE_DIR}/common_audio/audio_util.cc
		${CMAKE_CURRENT_SOURCE_DIR}/common_audio/resampler/push_sinc_resampler.cc
		${CMAKE_CURRENT_SOURCE_DIR}/common_audio/resampler/sinc_resampler.cc	
		${CMAKE_CURRENT_SOURCE_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft.cc
		${CMAKE_CURRENT_SOURCE_DIR}/system_wrappers/source/cpu_features.cc
		${CMAKE_CURRENT_SOURCE_DIR}/system_wrappers/source/field_trial.cc
		${CMAKE_CURRENT_SOURCE_DIR}/system_wrappers/source/metrics.cc
		${COMMON_AUDIO_SIGNAL_PROC_DIR}/splitting_filter.c
		${RTCBASE_DIR}/checks.cc
		${RTCBASE_DIR}/platform_thread_types.cc
		${RTCBASE_DIR}/race_checker.cc
		${RTCBASE_DIR}/string_encode.cc
		${RTCBASE_DIR}/string_utils.cc
		${RTCBASE_DIR}/experiments/field_trial_parser.cc
		${RTCBASE_DIR}/memory/aligned_malloc.cc
	)
	if(ANDROID)
		list(APPEND WEBRTC_SRC
			${RTCBASE_DIR}/platform_thread.cc
			${CMAKE_CURRENT_SOURCE_DIR}/modules/utility/source/helpers_android.cc
		)
	endif()
endif()

if(ENABLE_VAD)
	list(APPEND WEBRTC_SRC
		vad.cc
		${VAD_SRC_DIR}/vad.cc
		${VAD_SRC_DIR}/vad_core.c
		${VAD_SRC_DIR}/vad_filterbank.c
		${VAD_SRC_DIR}/vad_gmm.c
		${VAD_SRC_DIR}/vad_sp.c
		${VAD_SRC_DIR}/webrtc_vad.c
		${COMMON_AUDIO_SIGNAL_PROC_DIR}/division_operations.c
		${COMMON_AUDIO_SIGNAL_PROC_DIR}/energy.c
		${COMMON_AUDIO_SIGNAL_PROC_DIR}/get_scaling_square.c
		${COMMON_AUDIO_SIGNAL_PROC_DIR}/resample_by_2_internal.c
		${COMMON_AUDIO_SIGNAL_PROC_DIR}/resample_fractional.c
		${COMMON_AUDIO_SIGNAL_PROC_DIR}/resample_48khz.c
		${COMMON_AUDIO_SIGNAL_PROC_DIR}/spl_inl.c
	)
endif()

if(ENABLE_AEC)
	# Check the architecture
	if(SYSTEM_PROCESSOR MATCHES "x86_64|amd64|x86|i386")
		set(COMPILER_SUPPORTS_AVX2 TRUE)
	else()
		set(COMPILER_SUPPORTS_AVX2 FALSE)
	endif()

	if(COMPILER_SUPPORTS_AVX2)
		list(APPEND WEBRTC_SRC
				${CMAKE_CURRENT_SOURCE_DIR}/common_audio/resampler/sinc_resampler_avx2.cc
				${AEC3_SRC_DIR}/adaptive_fir_filter_avx2.cc
				${AEC3_SRC_DIR}/adaptive_fir_filter_erl_avx2.cc
				${AEC3_SRC_DIR}/fft_data_avx2.cc
				${AEC3_SRC_DIR}/matched_filter_avx2.cc
				${AEC3_SRC_DIR}/vector_math_avx2.cc
			)
	endif()

	if(COMPILER_SUPPORTS_NEON OR COMPILER_SUPPORTS_ARMV8)
		list(APPEND WEBRTC_SRC
			${CMAKE_CURRENT_SOURCE_DIR}/common_audio/resampler/sinc_resampler_neon.cc
			${CMAKE_CURRENT_SOURCE_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft_neon.cc
		)
	elseif(COMPILER_SUPPORTS_SSE2)
		list(APPEND WEBRTC_SRC
			${CMAKE_CURRENT_SOURCE_DIR}/common_audio/resampler/sinc_resampler_sse.cc
			${CMAKE_CURRENT_SOURCE_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft_sse2.cc
		)
	endif()
endif()

if(BUILD_SHARED_LIBS)
	if(IOS)
		add_library(mswebrtc SHARED ${WEBRTC_SRC})
	else()
		add_library(mswebrtc MODULE ${WEBRTC_SRC})
	endif()
else()
	add_library(mswebrtc STATIC ${WEBRTC_SRC})
endif()

if(ENABLE_AEC)
	target_include_directories(mswebrtc PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${AUDIO_PROCESSING_DIR}
		${AEC3_SRC_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}/api/audio
		${CMAKE_CURRENT_SOURCE_DIR}/third_party/abseil-cpp
	)
endif()
if(ENABLE_VAD)
	target_include_directories(mswebrtc PRIVATE
		${VAD_SRC_DIR}/include
		${RTCBASE_DIR}
		${COMMON_AUDIO_SIGNAL_PROC_DIR}/include
	)
endif()


set(LIBS )

if(LIBM)
	list(APPEND LIBS ${LIBM})
endif()

target_link_libraries(mswebrtc PRIVATE ${Mediastreamer2_TARGET} ${LIBS})
find_package(Threads REQUIRED)
target_link_libraries(mswebrtc PRIVATE Threads::Threads)

if(ENABLE_AEC)
	if(COMPILER_IS_CLANG)
		target_compile_definitions(mswebrtc PRIVATE WEBRTC_ENABLE_AVX2)
	endif()
	if(COMPILER_SUPPORTS_ARMV8)
		target_compile_options(mswebrtc PRIVATE -march=armv8-a)
	endif()
	if(COMPILER_SUPPORTS_NEON)
		target_compile_options(mswebrtc PRIVATE -mfpu=neon)
	endif()
	if(COMPILER_SUPPORTS_AVX2)
		if(MSVC)
			target_compile_options(mswebrtc PRIVATE "/arch:AVX2")
		else()
			target_compile_options(mswebrtc PRIVATE -mavx2)
			target_compile_options(mswebrtc PRIVATE -mfma)
		endif()
	endif()

	if(MSVC)
		target_compile_definitions(mswebrtc PRIVATE NOMINMAX)
		if(COMPILER_SUPPORTS_SSE2)
			target_compile_options(mswebrtc PRIVATE "/arch:SSE2")
		endif()
	else()
		if(COMPILER_SUPPORTS_SSE2)
			target_compile_options(mswebrtc PRIVATE -msse2)
		endif()
	endif()
endif()

target_compile_definitions(mswebrtc PRIVATE WIN32_LEAN_AND_MEAN)

if(ANDROID)
	target_link_libraries(mswebrtc PRIVATE log)
endif()

set_target_properties(mswebrtc PROPERTIES LINKER_LANGUAGE CXX)
if(BUILD_SHARED_LIBS)
	if(APPLE)
		if(IOS)
			set_target_properties(mswebrtc 	PROPERTIES
							FRAMEWORK TRUE
							MACOSX_FRAMEWORK_IDENTIFIER org.linphone.mswebrtc
							MACOSX_FRAMEWORK_INFO_PLIST "${PROJECT_SOURCE_DIR}/build/osx/Info.plist.in"
			)
		endif()
	endif()
	if(MSVC)
		install(FILES $<TARGET_PDB_FILE:mswebrtc>
			DESTINATION ${CMAKE_INSTALL_BINDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			CONFIGURATIONS Debug RelWithDebInfo
		)
		if(NOT IOS)
			set_target_properties(mswebrtc PROPERTIES PREFIX "lib")
		endif()
	endif()
endif()

install(TARGETS mswebrtc
	EXPORT LibLinphoneTargets
	RUNTIME DESTINATION ${Mediastreamer2_PLUGINS_DIR}
	LIBRARY DESTINATION ${Mediastreamer2_PLUGINS_DIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	FRAMEWORK DESTINATION Frameworks
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)