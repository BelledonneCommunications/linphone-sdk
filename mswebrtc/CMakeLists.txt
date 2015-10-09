############################################################################
# CMakeLists.txt
# Copyright (C) 2014  Belledonne Communications, Grenoble France
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

cmake_minimum_required(VERSION 3.0)
project(MSWEBRTC)


set(PACKAGE "mswebrtc")
set(PACKAGE_NAME "${PACKAGE}")
set(PACKAGE_VERSION "1.0.0")
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "support@belledonne-communications.com")
set(PACKAGE_TARNAME "mswebrtc")
set(PACKAGE_URL "")
set(VERSION "${PACKAGE_VERSION}")


option(ENABLE_AECM "Enable the WebRTC echo canceller for mobile support." YES)
option(ENABLE_ISAC "Enable the ISAC audio codec support." YES)
set(ISAC_FLAVOUR "fix" CACHE STRING "ISAC audio codec flavour.")
set_property(CACHE ISAC_FLAVOUR PROPERTY STRINGS "fix" "main")
option(ENABLE_STATIC "Build static library (default is shared library)." NO)


if(NOT ENABLE_AECM AND NOT ENABLE_ISAC)
	message(FATAL_ERROR "You need to enable at least one feature of WebRTC.")
endif()


if(LINPHONE_BUILDER_GROUP_EXTERNAL_SOURCE_PATH_BUILDERS)
	include("${EP_ms2_CONFIG_DIR}/Mediastreamer2Config.cmake")
else()
	find_package(Mediastreamer2 REQUIRED)
endif()


if(ENABLE_AECM)
	set(BUILD_AEC 1)
endif()
if(ENABLE_ISAC)
	set(BUILD_ISAC 1)
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)


include_directories(
	${MEDIASTREAMER2_INCLUDE_DIRS}
	${CMAKE_CURRENT_BINARY_DIR}
	webrtc
	webrtc/common_audio/signal_processing/include
)

add_definitions(-DHAVE_CONFIG_H)
if(WIN32)
	add_definitions(-DWEBRTC_WIN)
endif()
if("${CMAKE_SYSTEM_NAME}" STREQUAL "WindowsPhone" AND "${CMAKE_GENERATOR_PLATFORM}" STREQUAL "ARM")
	add_definitions(
		-D__ARMEL__
		-DWEBRTC_ARCH_ARM
	)
endif()
if(UNIX)
	if(APPLE)
		add_definitions(-DWEBRTC_MAC -DWEBRTC_THREAD_RR -DWEBRTC_CLOCK_TYPE_REALTIME)
	else()
		add_definitions(-DWEBRTC_LINUX -DWEBRTC_THREAD_RR)
	endif()
endif()

string(REGEX MATCH "^(arm*|aarch64)" FIXED_POINT_PROCESSOR "${CMAKE_SYSTEM_PROCESSOR}")
if(FIXED_POINT_PROCESSOR)
	add_definitions(-D__ARMEL__)
endif()

set(WEBRTC_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/webrtc/common_audio/signal_processing")
set(WEBRTC_COMMON_SRCS
	${WEBRTC_COMMON_DIR}/spl_sqrt.c
	${WEBRTC_COMMON_DIR}/real_fft.c
	${WEBRTC_COMMON_DIR}/filter_ar_fast_q12.c
	${WEBRTC_COMMON_DIR}/webrtc_fft_t_1024_8.c
	${WEBRTC_COMMON_DIR}/complex_bit_reverse.c
	${WEBRTC_COMMON_DIR}/spl_init.c
	${WEBRTC_COMMON_DIR}/splitting_filter.c
	${WEBRTC_COMMON_DIR}/spl_sqrt_floor.c
	${WEBRTC_COMMON_DIR}/levinson_durbin.c
	${WEBRTC_COMMON_DIR}/filter_ar.c
	${WEBRTC_COMMON_DIR}/get_scaling_square.c
	${WEBRTC_COMMON_DIR}/resample_fractional.c
	${WEBRTC_COMMON_DIR}/resample.c
	${WEBRTC_COMMON_DIR}/downsample_fast.c
	${WEBRTC_COMMON_DIR}/filter_ma_fast_q12.c
	${WEBRTC_COMMON_DIR}/auto_corr_to_refl_coef.c
	${WEBRTC_COMMON_DIR}/auto_correlation.c
	${WEBRTC_COMMON_DIR}/refl_coef_to_lpc.c
	${WEBRTC_COMMON_DIR}/dot_product_with_scale.c
	${WEBRTC_COMMON_DIR}/copy_set_operations.c
	${WEBRTC_COMMON_DIR}/spl_version.c
	${WEBRTC_COMMON_DIR}/cross_correlation.c
	${WEBRTC_COMMON_DIR}/sqrt_of_one_minus_x_squared.c
	${WEBRTC_COMMON_DIR}/ilbc_specific_functions.c
	${WEBRTC_COMMON_DIR}/resample_by_2.c
	${WEBRTC_COMMON_DIR}/complex_fft.c
	${WEBRTC_COMMON_DIR}/resample_48khz.c
	${WEBRTC_COMMON_DIR}/energy.c
	${WEBRTC_COMMON_DIR}/get_hanning_window.c
	${WEBRTC_COMMON_DIR}/randomization_functions.c
	${WEBRTC_COMMON_DIR}/vector_scaling_operations.c
	${WEBRTC_COMMON_DIR}/webrtc_fft_t_rad.c
	${WEBRTC_COMMON_DIR}/resample_by_2_internal.c
	${WEBRTC_COMMON_DIR}/lpc_to_refl_coef.c
	${WEBRTC_COMMON_DIR}/min_max_operations.c
	${WEBRTC_COMMON_DIR}/division_operations.c
)
include_directories(${WEBRTC_COMMOND_DIR}/include)
if(ENABLE_AECM)
	set(AECM_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/webrtc/modules/audio_processing/aecm")
	set(AECM_SRCS
		aec.c
		${AECM_SRC_DIR}/echo_control_mobile.c
		${AECM_SRC_DIR}/aecm_core.c
		${AECM_SRC_DIR}/../utility/fft4g.c
		${AECM_SRC_DIR}/../utility/ring_buffer.c
		${AECM_SRC_DIR}/../utility/delay_estimator.c
		${AECM_SRC_DIR}/../utility/delay_estimator_wrapper.c
	)
	include_directories(
		${AECM_SRC_DIR}/include
		webrtc/modules/audio_processing/utility
		webrtc/system_wrappers/interface
	)
endif()
if(ENABLE_ISAC)
	set(ISAC_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/webrtc/modules/audio_coding/codecs/isac/${ISAC_FLAVOUR}")
	set(ISAC_SRCS
		isac_dec.c
		isac_enc.c
		${ISAC_SRC_DIR}/source/fft.c
		${ISAC_SRC_DIR}/source/arith_routines.c
		${ISAC_SRC_DIR}/source/arith_routines_hist.c
		${ISAC_SRC_DIR}/source/arith_routines_logist.c
		${ISAC_SRC_DIR}/source/bandwidth_estimator.c
		${ISAC_SRC_DIR}/source/decode.c
		${ISAC_SRC_DIR}/source/decode_bwe.c
		${ISAC_SRC_DIR}/source/encode.c
		${ISAC_SRC_DIR}/source/entropy_coding.c
		${ISAC_SRC_DIR}/source/filterbank_tables.c
		${ISAC_SRC_DIR}/source/filterbanks.c
		${ISAC_SRC_DIR}/source/lattice.c
		${ISAC_SRC_DIR}/source/lpc_tables.c
		${ISAC_SRC_DIR}/source/pitch_estimator.c
		${ISAC_SRC_DIR}/source/pitch_filter.c
		${ISAC_SRC_DIR}/source/pitch_gain_tables.c
		${ISAC_SRC_DIR}/source/pitch_lag_tables.c
		${ISAC_SRC_DIR}/source/spectrum_ar_model_tables.c
		${ISAC_SRC_DIR}/source/transform.c
	)
	if("${ISAC_FLAVOUR}" STREQUAL "fix")
		list(APPEND ISAC_SRCS
			${ISAC_SRC_DIR}/source/decode_plc.c
			${ISAC_SRC_DIR}/source/filters.c
			${ISAC_SRC_DIR}/source/initialize.c
			${ISAC_SRC_DIR}/source/isacfix.c
			${ISAC_SRC_DIR}/source/lattice_c.c
			${ISAC_SRC_DIR}/source/pitch_filter_c.c
			${ISAC_SRC_DIR}/source/lpc_masking_model.c
		)
	else()
		list(APPEND ISAC_SRCS
			${ISAC_SRC_DIR}/source/initialize.c
			${ISAC_SRC_DIR}/source/crc.c
			${ISAC_SRC_DIR}/source/encode_lpc_swb.c
			${ISAC_SRC_DIR}/source/filter_functions.c
			${ISAC_SRC_DIR}/source/isac.c
			${ISAC_SRC_DIR}/source/lpc_analysis.c
			${ISAC_SRC_DIR}/source/lpc_gain_swb_tables.c
			${ISAC_SRC_DIR}/source/lpc_shape_swb12_tables.c
			${ISAC_SRC_DIR}/source/lpc_shape_swb16_tables.c
			${ISAC_SRC_DIR}/util/utility.c
		)
	endif()
	include_directories(
		${ISAC_SRC_DIR}/source
		${ISAC_SRC_DIR}/util
		${ISAC_SRC_DIR}/interface
	)
endif()

set(SOURCE_FILES
	mswebrtc.c
	${WEBRTC_COMMON_SRCS}
)
if(ENABLE_AECM)
	list(APPEND SOURCE_FILES ${AECM_SRCS})
endif()
if(ENABLE_ISAC)
	list(APPEND SOURCE_FILES ${ISAC_SRCS})
endif()

set(LIBS ${MEDIASTREAMER2_LIBRARIES})

if(ENABLE_STATIC)
	add_library(mswebrtc STATIC ${SOURCE_FILES})
else()
	add_library(mswebrtc SHARED ${SOURCE_FILES})
	set_target_properties(mswebrtc PROPERTIES VERSION 0)
	if(MSVC)
		if(CMAKE_BUILD_TYPE STREQUAL "Debug")
			install(FILES ${CMAKE_CURRENT_BINARY_DIR}/Debug/libmswebrtc.pdb
				DESTINATION bin
				PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
			)
		endif()
		set_target_properties(mswebrtc PROPERTIES PREFIX "lib")
	endif()
endif()
target_link_libraries(mswebrtc ${LIBS})

install(TARGETS mswebrtc
	RUNTIME DESTINATION lib/mediastreamer/plugins
	LIBRARY DESTINATION lib/mediastreamer/plugins
	ARCHIVE DESTINATION lib/mediastreamer/plugins
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
