############################################################################
# Options.cmake
# Copyright (C) 2010-2023  Belledonne Communications, Grenoble France
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

include(CMakeDependentOption)
include(FeatureSummary)


macro(linphonesdk_option NAME DESCRIPTION VALUE)
	string(TOUPPER ${NAME} UPPERCASE_NAME)
	string(REGEX REPLACE  " " "_" UPPERCASE_NAME ${UPPERCASE_NAME})
	string(REGEX REPLACE  "\\+" "P" UPPERCASE_NAME ${UPPERCASE_NAME})
	option(ENABLE_${UPPERCASE_NAME} "Enable ${NAME}: ${DESCRIPTION}" "${VALUE}")
	add_feature_info("${NAME}" "ENABLE_${UPPERCASE_NAME}" "${DESCRIPTION}")
endmacro()

# This macro can be used to add an option. Give it option name and description,
# default value and optionally dependent predicate and value
macro(linphonesdk_dependent_option NAME DESCRIPTION VALUE DEPENDS FORCE)
	string(TOUPPER ${NAME} UPPERCASE_NAME)
	string(REGEX REPLACE  " " "_" UPPERCASE_NAME ${UPPERCASE_NAME})
	string(REGEX REPLACE  "\\+" "P" UPPERCASE_NAME ${UPPERCASE_NAME})
	string(PREPEND UPPERCASE_NAME "ENABLE_")

	# CMake option() command does nothing when the according local or cache variable is already defined (See CMP0077).
	# Sadly, cmake_dependent_option() hasn't the same behavior and redefines the variable even if it has already
	# been set, which prevents the upper scope to masquerade Linphone SDK options.
	if (NOT DEFINED ${UPPERCASE_NAME})
		message("${UPPERCASE_NAME} not defined")
		cmake_dependent_option(${UPPERCASE_NAME} "${DESCRIPTION}" "${VALUE}" "${DEPENDS}" "${FORCE}")
		add_feature_info("${NAME}" "${UPPERCASE_NAME}" "${DESCRIPTION}")
	else()
		message("${UPPERCASE_NAME} already defined")
	endif()
endmacro()

macro(linphonesdk_strict_dependent_option NAME DESCRIPTION VALUE CONDITION FORCE STRICT_CONDITION ERROR_MSG)
	linphonesdk_dependent_option("${NAME}" "${DESCRIPTION}" "${VALUE}" "${CONDITION}" "${FORCE}")
	string(TOUPPER ${NAME} UPPERCASE_NAME)
	string(REGEX REPLACE  " " "_" UPPERCASE_NAME ${UPPERCASE_NAME})
	string(REGEX REPLACE  "\\+" "P" UPPERCASE_NAME ${UPPERCASE_NAME})
	set(_ERROR FALSE)
	if(${ENABLE_${UPPERCASE_NAME}})
		foreach(_COND ${STRICT_CONDITION})
			if(_COND MATCHES "^NOT (.+)$")
				if(${CMAKE_MATCH_1})
					set(_ERROR TRUE)
				endif()
			elseif(NOT ${_COND})
				set(_ERROR TRUE)
			endif()
		endforeach()
	endif()
	if(_ERROR)
		message(FATAL_ERROR "Trying to enable ${NAME} but ${ERROR_MSG}")
	endif()
endmacro()


if(UNIX AND NOT APPLE AND NOT ANDROID)
	set(LINUX_OR_BSD 1)
endif()


# Hidden options to (de)activate building of some bc components (mainly used by flexisip)
option(BUILD_BELLESIP "Build belle-sip component." ON)
option(BUILD_LIBLINPHONE "Build liblinphone component." ON)
option(BUILD_MEDIASTREAMER2 "Build mediastreamer2 component." ON)
mark_as_advanced(FORCE BUILD_BELLESIP BUILD_LIBLINPHONE BUILD_MEDIASTREAMER2)

# Global compilation options
option(BUILD_SHARED_LIBS "Build using shared libraries." ON)
linphonesdk_dependent_option("HW Sanitizer" "Enable Android HW sanitizer" OFF "ANDROID" OFF)
linphonesdk_option("Sanitizer" "Enable Clang sanitizer" OFF)
linphonesdk_option("Strict" "Pass strict flags to the compiler." ON)

# Activation of tools/tests...
linphonesdk_option("Assets" "Enable packaging of assets (ringtones) when building the SDK." ON)
linphonesdk_option("Doc" "Enable documentation generation with Doxygen." ON)
linphonesdk_option("Tools" "Enable tools binary compilation." ON)
linphonesdk_option("Tests component" "Enable compilation of tests helper library." ON)
linphonesdk_option("Deprecated tests" "Enable execution of deprecated tests" OFF)
linphonesdk_strict_dependent_option("Unit tests" "Enable unit tests support with BCUnit library (needs ENABLE_TESTS_COMPONENT=ON)." ON ON OFF "ENABLE_TESTS_COMPONENT" "Compilation of tests helper library not enabled (ENABLE_TESTS_COMPONENT)")
linphonesdk_dependent_option("Windows Tools Check" "Enable windows tools check." ON "WIN32" OFF)

# Activation of meta features allowing the activation of other ones
linphonesdk_option("GPL third parties" "Usage of GPL third-party code (FFmpeg)." OFF)
linphonesdk_option("Non free features" "Allow inclusion of non-free features (mainly codecs) in the build." OFF)

# Activation of audio related features
linphonesdk_strict_dependent_option("AMRNB" "AMR narrow-band audio encoding/decoding support (require license) (needs ENABLE_NON_FREE_FEATURES=ON)." OFF ON OFF "ENABLE_NON_FREE_FEATURES" "Inclusion of non-free features is not allowed (ENABLE_NON_FREE_FEATURES)")
linphonesdk_strict_dependent_option("AMRWB" "AMR wide-band audio encoding/decoding support (require license) (needs ENABLE_NON_FREE_FEATURES=ON)." OFF ON OFF "ENABLE_NON_FREE_FEATURES" "Inclusion of non-free features is not allowed (ENABLE_NON_FREE_FEATURES)")
linphonesdk_option("Baudot" "Baudot text transmission support." ON)
linphonesdk_option("BV16" "BroadVoice 16 audio encoding/decoding support." ON)
linphonesdk_option("Codec2" "Codec2 audio encoding/decoding support." OFF)
linphonesdk_option("G726" "G726 audio encoding/decoding support." OFF)
linphonesdk_option("G729" "G.729 audio encoding/decoding support." OFF)
linphonesdk_option("G729B CNG" "G.729 annex B confort noise generation." OFF)
linphonesdk_option("GSM" "GSM audio encoding/decoding support." ON)
linphonesdk_option("iLBC" "iLBC audio encoding/decoding support." ON)
linphonesdk_option("ISAC" "ISAC audio encoding/decoding support." OFF)
linphonesdk_option("MKV" "MKV playing and recording support." ON)
linphonesdk_option("OPUS" "OPUS audio encoding/decoding support." ON)
linphonesdk_option("Silk" "Silk audio encoding/decoding support." OFF)
linphonesdk_option("Speex" "Speex audio encoding/decoding and DSP support." ON)
linphonesdk_option("Theora" "Theora video encoding/decoding support." OFF)
linphonesdk_dependent_option("WASAPI" "Windows Audio Session API (WASAPI) sound card support." ON "WIN32" OFF)
linphonesdk_option("WebRTC AEC" "WebRTC echo canceller support." ON)
linphonesdk_option("WebRTC AECM" "WebRTC echo canceller for mobile support." OFF)
linphonesdk_option("WebRTC VAD" "WebRTC voice activation detector support." ON)

# Activation of video related features
linphonesdk_option("Video" "Ability to capture and display video." ON)
linphonesdk_dependent_option("AV1" "Build mediastreamer2 with AV1 support through libaom and dav1d (needs ENABLE_VIDEO=ON)." ON "ENABLE_VIDEO" OFF)
linphonesdk_strict_dependent_option("FFMpeg" "Build mediastreamer2 with ffmpeg video support (needs ENABLE_VIDEO=ON and ENABLE_GPL_THIRD_PARTIES=ON) (not available on Android and Windows)." OFF "ENABLE_VIDEO" OFF "ENABLE_GPL_THIRD_PARTIES" "Usage of GPL third-party code is not allowed (ENABLE_GPL_THIRD_PARTIES)")
linphonesdk_strict_dependent_option("H263" "H263 video encoding/decoding support (require license) (needs ENABLE_FFMPEG=ON and ENABLE_NON_FREE_FEATURES=ON)." OFF "ENABLE_FFMPEG" OFF "ENABLE_NON_FREE_FEATURES" "Inclusion of non-free features is not allowed (ENABLE_NON_FREE_FEATURES)")
linphonesdk_strict_dependent_option("H263p" "H263+ video encoding/decoding support (require license) (needs ENABLE_FFMPEG=ON and ENABLE_NON_FREE_FEATURES=ON)." OFF "ENABLE_FFMPEG" OFF "ENABLE_NON_FREE_FEATURES" "Inclusion of non-free features is not allowed (ENABLE_NON_FREE_FEATURES)")
linphonesdk_strict_dependent_option("MPEG4" "MPEG4 video encoding/decoding support (require license) (needs ENABLE_FFMPEG=ON and ENABLE_NON_FREE_FEATURES=ON)." OFF "ENABLE_FFMPEG" OFF "ENABLE_NON_FREE_FEATURES" "Inclusion of non-free features is not allowed (ENABLE_NON_FREE_FEATURES)")
linphonesdk_dependent_option("jpeg" "JPEG support with libjpeg-turbo (needs ENABLE_VIDEO=ON)." ON "ENABLE_VIDEO" OFF)
linphonesdk_dependent_option("LibYUV" "Build mediastreamer2 with LibYUV support (needs ENABLE_VIDEO=ON)." ON "ENABLE_VIDEO" OFF)
linphonesdk_strict_dependent_option("OpenH264" "H.264 video encoding/decoding support with the openh264 library (require license) (needs ENABLE_VIDEO=ON and ENABLE_NON_FREE_FEATURES=ON)." OFF "ENABLE_VIDEO" OFF "ENABLE_NON_FREE_FEATURES" "Inclusion of non-free features is not allowed (ENABLE_NON_FREE_FEATURES)")
linphonesdk_dependent_option("Embedded OpenH264" "Embed the openh264 library instead of downloading it from Cisco (needs ENABLE_OPENH264=ON)." OFF "ENABLE_OPENH264" OFF)
linphonesdk_dependent_option("qrcode" "QRCode support with zxing (needs ENABLE_VIDEO=ON)." ON "ENABLE_VIDEO" OFF)
linphonesdk_option("Qt GL" "Enable Qt OpenGL rendering support." OFF)
linphonesdk_dependent_option("V4L" "Ability to capture and display video using libv4l2 (needs ENABLE_VIDEO=ON)." ON "ENABLE_VIDEO;LINUX_OR_BSD" OFF)
linphonesdk_dependent_option("VPX" "Build mediastreamer2 with VPX codec." ON "ENABLE_VIDEO" OFF)
linphonesdk_dependent_option("MSWinRTVideo" "Build mswinrtvid mediastreamer plugin (Deprecated)." OFF "ENABLE_VIDEO;UWP" OFF)

# Activation of other software features
linphonesdk_option("Advanced IM" "Enable advanced instant messaging such as group chat." ON)
linphonesdk_option("Assets" "Enable packaging of assets (ringtones) when building the SDK." ON)
linphonesdk_option("Daemon" "Enable Linphone Daemon." ON)
linphonesdk_option("DB Storage" "Enable the database storage." ON)
linphonesdk_dependent_option("DNS_SERVICE" "Enable Apple DNS Service. Available on Mac and iOS. Incompatible with ENABLE_MDNS=ON." ON "APPLE" OFF)
linphonesdk_option("Example Plugin" "Enable build of the liblinphone example plugin." OFF)
linphonesdk_option("FlexiAPI" "Enable the FlexiAPI support in Liblinphone." ON)
linphonesdk_option("LDAP" "Enable LDAP Liblinphone." OFF)
linphonesdk_option("mDNS" "Multicast DNS support." OFF)
linphonesdk_dependent_option("Microsoft Store App" "Enable build for Microsoft Store." OFF "WIN32" OFF)
linphonesdk_option("OpenSSL Export" "Enable OpenSSL deployment" OFF)
linphonesdk_option("PCAP" "PCAP support." OFF)
linphonesdk_option("Relative prefix" "liblinphone and mediastreamer will look for their respective ressources relatively to their location." OFF)
linphonesdk_option("RTP Map always in SDP" "Always include rtpmap in SDP." OFF)
linphonesdk_option("Tunnel" "Enable build of tunnel." OFF)
linphonesdk_dependent_option("Ekt Server Plugin" "Enable build of the EKT server plugin (needs ENABLE_CXX_WRAPPER)." OFF "BUILD_LIBLINPHONE" OFF "ENABLE_CXX_WRAPPER" OFF)
linphonesdk_option("VCARD" "Enable vCard 4 support in Linphone and Liblinphone." ON)
linphonesdk_option("Decaf" "Enable Elliptic Curve Cryptography support" ON)
linphonesdk_option("LIME" "Enable Linphone IM Encryption support in Liblinphone (Deprecated, use ENABLE_LIME_X3DH=ON instead)." OFF)
linphonesdk_dependent_option("LIME X3DH" "Enable Linphone IM Encryption version 2 support in Liblinphone." ON "ENABLE_ADVANCED_IM;ENABLE_DB_STORAGE" OFF)
linphonesdk_option("Mbedtls" "Crypto stack implementation based on mbedtls." ON)
linphonesdk_strict_dependent_option("Openssl" "Crypto stack implementation based on openssl." OFF ON OFF "NOT ENABLE_MBEDTLS" "Mbedtls is not disabled. To ENABLE_OPENSSL you must disable Mbdetls (ENABLE_MBEDTLS=OFF)")
linphonesdk_strict_dependent_option("PQCrypto" "Post Quantum Cryptography (require license) (needs ENABLE_NON_FREE_FEATURES=ON)." OFF ON OFF "ENABLE_NON_FREE_FEATURES" "Inclusion of non-free features is not allowed (ENABLE_NON_FREE_FEATURES)")
linphonesdk_option("SRTP" "SRTP media encryption support." ON)
linphonesdk_dependent_option("ZRTP" "Build with ZRTP support (needs ENABLE_SRTP=ON)." ON "ENABLE_SRTP" OFF)
linphonesdk_dependent_option("GoClear" "Build with ZRTP GoClear message support (RFC 6189 - section 5.11) (needs ENABLE_ZRTP=ON)." ON "ENABLE_ZRTP" OFF)

# Activation of wrappers
linphonesdk_option("CSharp wrapper" "Build the C# wrapper from Liblinphone." OFF)
linphonesdk_option("CXX wrapper" "Build the C++ wrapper for Liblinphone." OFF)
linphonesdk_option("Python wrapper" "Build the Python wrapper from Liblinphone." OFF)
linphonesdk_option("Java wrapper" "Build the Java wrapper from Liblinphone." OFF)
linphonesdk_option("Swift wrapper" "Build the Swift wrapper sources from Liblinphone." OFF)
linphonesdk_dependent_option("Swift wrapper compilation" "Compile and package the swift wrapper framework (needs ENABLE_SWIFT_WRAPPER=ON)." OFF "ENABLE_SWIFT_WRAPPER" OFF)
linphonesdk_dependent_option("Swift doc" "Build the Swift doc from Liblinphone (needs ENABLE_SWIFT_WRAPPER=ON)." OFF "ENABLE_SWIFT_WRAPPER" OFF)

# Activation of Android related features
linphonesdk_dependent_option("AAudio" "AAudio Android sound card for Android 8+." ON "ANDROID" OFF)
linphonesdk_dependent_option("Oboe" "Oboe Android sound card for Android 8+." ON "ANDROID" OFF)
linphonesdk_dependent_option("Camera2" "Android capture filter using Camera2 API for Android 8+ (needs ENABLE_VIDEO=ON)." ON "ANDROID;ENABLE_VIDEO" OFF)


if(ENABLE_QRCODE)
	# Disable ZXing for old gcc version (need 7.0 for c++17)
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
		message(STATUS "ZXing doesn't support gcc below 7.0 [" ${CMAKE_CXX_COMPILER_VERSION} "]. Deactivate QRCode for default.")
		set(ENABLE_QRCODE OFF)
	endif()
	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
		message(STATUS "ZXing doesn't support clang below 5.0 [" ${CMAKE_CXX_COMPILER_VERSION} "]. Deactivate QRCode for default.")
		set(ENABLE_QRCODE OFF)
	endif()
endif()

if(ENABLE_SPEEX)
	set(ENABLE_SPEEX_CODEC ON CACHE BOOL "" FORCE)
	set(ENABLE_SPEEX_DSP ON CACHE BOOL "" FORCE)
else()
	set(ENABLE_SPEEX_CODEC OFF CACHE BOOL "" FORCE)
	set(ENABLE_SPEEX_DSP OFF CACHE BOOL "" FORCE)
endif()

if(ENABLE_AMRNB OR ENABLE_AMRWB)
	set(ENABLE_AMR ON CACHE BOOL "" FORCE)
else()
	set(ENABLE_AMR OFF CACHE BOOL "" FORCE)
endif()

if(ENABLE_LIME_X3DH OR ENABLE_DB_STORAGE)
	set(ENABLE_SOCI ON)
else()
	set(ENABLE_SOCI OFF)
endif()

set(ENABLE_JSONCPP ON CACHE BOOL "" FORCE)

if(ENABLE_G729 OR ENABLE_G729B_CNG)
	set(BUILD_BCG729 ON CACHE BOOL "" FORCE)
else()
	set(BUILD_BC729 OFF CACHE BOOL "" FORCE)
endif()


# Options to choose between shared and static libraries for BC projects
cmake_dependent_option(BUILD_BCG729_SHARED_LIBS "Build shared bcg729 library." ${BUILD_SHARED_LIBS} "BUILD_BCG729" OFF)
option(BUILD_BCTOOLBOX_SHARED_LIBS "Build shared bctoolbox library." ${BUILD_SHARED_LIBS})
cmake_dependent_option(BUILD_BCUNIT_SHARED_LIBS "Build shared bcunit library." ${BUILD_SHARED_LIBS} "ENABLE_TESTS_COMPONENT" OFF)
cmake_dependent_option(BUILD_BELCARD_SHARED_LIBS "Build shared belcard library." ${BUILD_SHARED_LIBS} "ENABLE_VCARD" OFF)
cmake_dependent_option(BUILD_BELLESIP_SHARED_LIBS "Build shared belle-sip library." ${BUILD_SHARED_LIBS} "BUILD_BELLESIP" OFF)
option(BUILD_BELR_SHARED_LIBS "Build shared belr library." ${BUILD_SHARED_LIBS})
cmake_dependent_option(BUILD_BZRTP_SHARED_LIBS "Build shared bzrtp library." ${BUILD_SHARED_LIBS} "ENABLE_ZRTP" OFF)
cmake_dependent_option(BUILD_LIBLINPHONE_SHARED_LIBS "Build shared liblinphone library." ${BUILD_SHARED_LIBS} "BUILD_LIBLINPHONE" OFF)
cmake_dependent_option(BUILD_LIME_SHARED_LIBS "Build shared lime library." ${BUILD_SHARED_LIBS} "ENABLE_LIME_X3DH" OFF)
cmake_dependent_option(BUILD_MEDIASTREAMER2_SHARED_LIBS "Build shared mediastreamer2 library." ${BUILD_SHARED_LIBS} "BUILD_MEDIASTREAMER2" OFF)
cmake_dependent_option(BUILD_PQCRYPTO_SHARED_LIBS "Build shared postquantumcryptoengine library." ${BUILD_SHARED_LIBS} "ENABLE_PQCRYPTO" OFF)
option(BUILD_ORTP_SHARED_LIBS "Build shared ortp library." ${BUILD_SHARED_LIBS})
cmake_dependent_option(BUILD_TUNNEL_SHARED_LIBS "Build shared tunnel library." ${BUILD_SHARED_LIBS} "ENABLE_TUNNEL" OFF)
