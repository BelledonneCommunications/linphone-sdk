############################################################################
# BcProjects.cmake
# Copyright (c) 2010-2023 Belledonne Communications SARL.
#
############################################################################
#
# This file is part of Linphone SDK 
# (see https://gitlab.linphone.org/BC/public/linphone-sdk).
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
############################################################################

if(ENABLE_TESTS_COMPONENT)
	function(add_bcunit)
		set(BUILD_SHARED_LIBS ${BUILD_BCUNIT_SHARED_LIBS})
		add_subdirectory("bcunit")
	endfunction()
	add_bcunit()
endif()

function(add_bctoolbox)
	set(BUILD_SHARED_LIBS ${BUILD_BCTOOLBOX_SHARED_LIBS})
	add_subdirectory("bctoolbox")
endfunction()
add_bctoolbox()

function(add_ortp)
	set(BUILD_SHARED_LIBS ${BUILD_ORTP_SHARED_LIBS})
	add_subdirectory("ortp")
endfunction()
add_ortp()

if(ENABLE_PQCRYPTO)
	function(add_pqcrypto)
		set(BUILD_SHARED_LIBS ${BUILD_PQCRYPTO_SHARED_LIBS})
		add_subdirectory("postquantumcryptoengine")
	endfunction()
	add_pqcrypto()
endif()

if(ENABLE_ZRTP)
	function(add_bzrtp)
		set(BUILD_SHARED_LIBS ${BUILD_BZRTP_SHARED_LIBS})
		add_subdirectory("bzrtp")
	endfunction()
	add_bzrtp()
endif()

if(BUILD_BCG729)
	function(add_bcg729)
		set(BUILD_SHARED_LIBS ${BUILD_BCG729_SHARED_LIBS})
		add_subdirectory("bcg729")
	endfunction()
	add_bcg729()
endif()

if(ENABLE_MKV)
	function(add_bcmatroska2)
		set(BUILD_SHARED_LIBS OFF) # Always build statically, otherwise linking is failing on Windows
		add_subdirectory("bcmatroska2")
	endfunction()
	add_bcmatroska2()
endif()

if(BUILD_MEDIASTREAMER2)
	function(add_mediastreamer2)
		set(BUILD_SHARED_LIBS ${BUILD_MEDIASTREAMER2_SHARED_LIBS})
		add_subdirectory("mediastreamer2")
	endfunction()
	add_mediastreamer2()

	if(ENABLE_AAUDIO)
		add_subdirectory("msaaudio")
	endif()
	if(ENABLE_AMRNB OR ENABLE_AMRWB)
		function(add_msamr)
			if(ENABLE_AMRNB)
				set(ENABLE_NARROWBAND ON)
			else()
				set(ENABLE_NARROWBAND OFF)
			endif()
			if(ENABLE_AMRWB)
				set(ENABLE_WIDEBAND ON)
			else()
				set(ENABLE_WIDEBAND OFF)
			endif()

			add_subdirectory("msamr")
		endfunction()
		add_msamr()
	endif()
	if(ENABLE_CAMERA2)
		add_subdirectory("msandroidcamera2")
	endif()
	if(ENABLE_CODEC2)
		add_subdirectory("mscodec2")
	endif()
	if(ENABLE_SILK)
		add_subdirectory("mssilk")
	endif()
	if(ENABLE_OBOE)
		add_subdirectory("msoboe")
	endif()
	if(ENABLE_OPENH264)
		add_subdirectory("msopenh264")
	endif()
	if(ENABLE_WASAPI)
		add_subdirectory("mswasapi")
	endif()
	if(ENABLE_WEBRTC_AEC OR ENABLE_WEBRTC_AECM OR ENABLE_WEBRTC_VAD OR ENABLE_ILBC OR ENABLE_ISAC)
		function(add_mswebrtc)
			if(ENABLE_WEBRTC_AEC)
				set(ENABLE_AEC ON)
			else()
				set(ENABLE_AEC OFF)
			endif()
			if(ENABLE_WEBRTC_AECM)
				set(ENABLE_AECM ON)
			else()
				set(ENABLE_AECM OFF)
			endif()
			if(ENABLE_WEBRTC_VAD)
				set(ENABLE_VAD ON)
			else()
				set(ENABLE_VAD OFF)
			endif()

			add_subdirectory("mswebrtc")
		endfunction()
		add_mswebrtc()
	endif()
	if(ENABLE_MSWINRTVIDEO)
		add_subdirectory("mswinrtvid")
	endif()
endif()

function(add_belr)
	set(BUILD_SHARED_LIBS ${BUILD_BELR_SHARED_LIBS})
	add_subdirectory("belr")
endfunction()
add_belr()

if(ENABLE_TUNNEL)
	function(add_tunnel)
		set(BUILD_SHARED_LIBS ${BUILD_TUNNEL_SHARED_LIBS})
		add_subdirectory("tunnel")
	endfunction()
	add_tunnel()
endif()

if(BUILD_BELLESIP)
	function(add_bellesip)
		set(BUILD_SHARED_LIBS ${BUILD_BELLESIP_SHARED_LIBS})
		add_subdirectory("belle-sip")
	endfunction()
	add_bellesip()
endif()

if(ENABLE_VCARD)
	function(add_belcard)
		set(BUILD_SHARED_LIBS ${BUILD_BELCARD_SHARED_LIBS})
		add_subdirectory("belcard")
	endfunction()
	add_belcard()
endif()

if(ENABLE_LIME_X3DH)
	function(add_lime)
		set(BUILD_SHARED_LIBS ${BUILD_LIME_SHARED_LIBS})
		add_subdirectory("lime")
	endfunction()
	add_lime()
endif()

if(BUILD_LIBLINPHONE)
	function(add_liblinphone)
		set(BUILD_SHARED_LIBS ${BUILD_LIBLINPHONE_SHARED_LIBS})
		add_subdirectory("liblinphone")
	endfunction()
	add_liblinphone()
endif()

if(ENABLE_EKT_SERVER_PLUGIN)
	add_subdirectory("ekt-server")
endif()
