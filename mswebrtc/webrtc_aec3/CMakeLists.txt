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

if(ENABLE_AEC3)

project(MSWebRTCAEC3 VERSION 1.1.1 LANGUAGES C CXX)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/mediastreamer2/plugins")

set(PACKAGE "${PROJECT_NAME}")
set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "support@belledonne-communications.com")
set(PACKAGE_TARNAME "mswebrtcaec3")
set(PACKAGE_URL "")
set(VERSION "${PACKAGE_VERSION}")

option(ENABLE_AEC3 "Enable the WebRTC echo canceller 3 support." YES)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS NO)

include(GNUInstallDirs)

add_link_options("LINKER:-no-undefined")

if(NOT CMAKE_INSTALL_RPATH AND CMAKE_INSTALL_PREFIX)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_FULL_LIBDIR})
	message(STATUS "Setting install rpath to ${CMAKE_INSTALL_RPATH}")
endif()

find_package(BCToolbox 5.3.0 REQUIRED)
find_package(Mediastreamer2 5.3.0 REQUIRED)

find_library(LIBM NAMES m)

add_definitions(-DWEBRTC_APM_DEBUG_DUMP=0)
add_definitions(-DRTC_DISABLE_LOGGING=1)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake" "${CMAKE_CURRENT_BINARY_DIR}/config.h")

set(WEBRTC_AEC3_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(-DHAVE_CONFIG_H)
if(WIN32)
	add_definitions(-DWEBRTC_WIN)
endif()
if(ANDROID)
	add_definitions("-DANDROID")
endif()

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
		add_definitions(-DWEBRTC_MAC -DWEBRTC_THREAD_RR -DWEBRTC_CLOCK_TYPE_REALTIME)
	else()
		add_definitions(-DWEBRTC_LINUX -DWEBRTC_THREAD_RR)
	endif()
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SYSTEM_PROCESSOR)
string(REGEX MATCH "^(arm*|aarch64)" FIXED_POINT_PROCESSOR "${SYSTEM_PROCESSOR}")
if(FIXED_POINT_PROCESSOR)
	add_definitions(-D__ARMEL__)
endif()

if(SYSTEM_PROCESSOR MATCHES "^i.?86.*" OR SYSTEM_PROCESSOR MATCHES "^x86.*" OR SYSTEM_PROCESSOR MATCHES "^x86_64.*" OR SYSTEM_PROCESSOR MATCHES "^amd64.*" OR SYSTEM_PROCESSOR MATCHES "^x64.*")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx2 -mfma")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma")
endif()

set(AEC3_CXX17_FLAGS "-std=c++17")
set(AEC3_CXX14_FLAGS "-std=c++14")

set(AEC3_AUDIO_PROCESSING_DIR "${WEBRTC_AEC3_SRC_DIR}/modules/audio_processing")
set(AEC3_SRC_DIR "${WEBRTC_AEC3_SRC_DIR}/modules/audio_processing/aec3")
set(AEC3_API_DIR "${WEBRTC_AEC3_SRC_DIR}/api/audio")
set(AEC3_RTCBASE_DIR "${WEBRTC_AEC3_SRC_DIR}/rtc_base")

set(AEC3_SRCS
	aec3.cc
	mswebrtc_aec3.cc
	${WEBRTC_AEC3_SRC_DIR}
	${AEC3_API_DIR}/echo_canceller3_config.cc
	${AEC3_AUDIO_PROCESSING_DIR}/logging/apm_data_dumper.cc
	${AEC3_AUDIO_PROCESSING_DIR}/audio_buffer.cc
	${AEC3_AUDIO_PROCESSING_DIR}/high_pass_filter.cc
	${AEC3_AUDIO_PROCESSING_DIR}/splitting_filter.cc
	${AEC3_AUDIO_PROCESSING_DIR}/three_band_filter_bank.cc
	${AEC3_AUDIO_PROCESSING_DIR}/utility/cascaded_biquad_filter.cc
	${AEC3_SRC_DIR}
	${AEC3_SRC_DIR}/echo_canceller3.cc
	${AEC3_SRC_DIR}/block_framer.cc
	${AEC3_SRC_DIR}/api_call_jitter_metrics.cc
	${AEC3_SRC_DIR}/adaptive_fir_filter_avx2.cc
	${AEC3_SRC_DIR}/adaptive_fir_filter.cc
	${AEC3_SRC_DIR}/adaptive_fir_filter_erl_avx2.cc
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
	${AEC3_SRC_DIR}/fft_data_avx2.cc
	${AEC3_SRC_DIR}/filter_analyzer.cc
	${AEC3_SRC_DIR}/frame_blocker.cc
	${AEC3_SRC_DIR}/fullband_erle_estimator.cc
	${AEC3_SRC_DIR}/matched_filter_avx2.cc
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
	${AEC3_SRC_DIR}/vector_math_avx2.cc
	${WEBRTC_AEC3_SRC_DIR}/common_audio/audio_util.cc
	${WEBRTC_AEC3_SRC_DIR}/common_audio/resampler/push_sinc_resampler.cc
	${WEBRTC_AEC3_SRC_DIR}/common_audio/resampler/sinc_resampler.cc
	${WEBRTC_AEC3_SRC_DIR}/common_audio/resampler/sinc_resampler_avx2.cc
	${WEBRTC_AEC3_SRC_DIR}/common_audio/signal_processing/splitting_filter.c
	${WEBRTC_AEC3_SRC_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft.cc
	${WEBRTC_AEC3_SRC_DIR}/system_wrappers/source/cpu_features.cc
	${WEBRTC_AEC3_SRC_DIR}/system_wrappers/source/field_trial.cc
	${WEBRTC_AEC3_SRC_DIR}/system_wrappers/source/metrics.cc
	${AEC3_RTCBASE_DIR}/checks.cc
	${AEC3_RTCBASE_DIR}/platform_thread_types.cc
	${AEC3_RTCBASE_DIR}/race_checker.cc
	${AEC3_RTCBASE_DIR}/string_encode.cc
	${AEC3_RTCBASE_DIR}/string_utils.cc
	${AEC3_RTCBASE_DIR}/experiments/field_trial_parser.cc
	${AEC3_RTCBASE_DIR}/memory/aligned_malloc.cc
)

# bc_apply_compile_flags(AEC3_CXX14_SRCS AEC3_CXX14_FLAGS)
if(SYSTEM_PROCESSOR MATCHES "(^aarch64.*|arm64)")
	add_definitions(
		-DWEBRTC_ARCH_ARM64=1
	)
endif()

if(SYSTEM_PROCESSOR MATCHES "^arm.*" OR SYSTEM_PROCESSOR MATCHES "^aarch64.*")
	list(APPEND AEC3_SRCS
		${WEBRTC_AEC3_SRC_DIR}/common_audio/resampler/sinc_resampler_neon.cc
		${WEBRTC_AEC3_SRC_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft_neon.cc
	)
elseif(SYSTEM_PROCESSOR MATCHES "^i.?86.*" OR SYSTEM_PROCESSOR MATCHES "^x86.*" OR SYSTEM_PROCESSOR MATCHES "^x86_64.*" OR SYSTEM_PROCESSOR MATCHES "^amd64.*" OR SYSTEM_PROCESSOR MATCHES "^x64.*")
	list(APPEND AEC3_SRCS
		${WEBRTC_AEC3_SRC_DIR}/common_audio/resampler/sinc_resampler_sse.cc
		${WEBRTC_AEC3_SRC_DIR}/common_audio/third_party/ooura/fft_size_128/ooura_fft_sse2.cc
	)
endif()

include_directories(
	${WEBRTC_AEC3_SRC_DIR}
	${AEC3_AUDIO_PROCESSING_DIR}
	${AEC3_SRC_DIR}
	${AEC3_API_DIR}
	${AEC3_RTCBASE_DIR}
	${WEBRTC_AEC3_SRC_DIR}/third_party/abseil-cpp
	)


set(SOURCE_FILES
	${AEC3_SRCS}
)

set(LIBS )

if(LIBM)
	list(APPEND LIBS ${LIBM})
endif()

if(BUILD_SHARED_LIBS)
	if(IOS)
		add_library(mswebrtcaec3 SHARED ${SOURCE_FILES})
	else()
		add_library(mswebrtcaec3 MODULE ${SOURCE_FILES})
	endif()
else()
	add_library(mswebrtcaec3 STATIC ${SOURCE_FILES})
endif()
target_link_libraries(mswebrtcaec3 PRIVATE ${Mediastreamer2_TARGET} ${LIBS})
target_link_libraries(msopenh264 PRIVATE ${Mediastreamer2_TARGET} ${OpenH264_TARGET})

set_target_properties(mswebrtcaec3 PROPERTIES LINKER_LANGUAGE CXX)
if(BUILD_SHARED_LIBS)
	if(APPLE)
		if(IOS)
			set_target_properties(mswebrtcaec3 	PROPERTIES
							FRAMEWORK TRUE
							MACOSX_FRAMEWORK_IDENTIFIER org.linphone.mswebrtcaec3
							MACOSX_FRAMEWORK_INFO_PLIST "${PROJECT_SOURCE_DIR}/build/osx/Info.plist.in"
			)
		endif()
	endif()
	if(MSVC)
		install(FILES $<TARGET_PDB_FILE:mswebrtcaec3>
			DESTINATION ${CMAKE_INSTALL_BINDIR}
			PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			CONFIGURATIONS Debug RelWithDebInfo
		)
		if(NOT IOS)
			set_target_properties(mswebrtcaec3 PROPERTIES PREFIX "lib")
		endif()
	endif()
endif()

install(TARGETS mswebrtcaec3
		RUNTIME DESTINATION ${Mediastreamer2_PLUGINS_DIR}
		LIBRARY DESTINATION ${Mediastreamer2_PLUGINS_DIR}
		ARCHIVE DESTINATION ${Mediastreamer2_PLUGINS_DIR}
		FRAMEWORK DESTINATION Frameworks
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)


endif()