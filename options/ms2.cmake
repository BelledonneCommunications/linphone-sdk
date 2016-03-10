############################################################################
# ms2.cmake
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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

# Mediastreamer2 build options

# This function can be used to add an option. Give it option name and description,
# default value and optionally dependent predicate and value
macro (ms2_add_dependent_option NAME DESCRIPTION DEFAULT_VALUE CONDITION CONDITION_VALUE)
	string(TOUPPER ${NAME} UPPERCASE_NAME)
	string(REGEX REPLACE  " " "_" UPPERCASE_NAME ${UPPERCASE_NAME})
	string(REGEX REPLACE  "\\+" "P" UPPERCASE_NAME ${UPPERCASE_NAME})
	cmake_dependent_option(ENABLE_${UPPERCASE_NAME} "${DESCRIPTION}" "${DEFAULT_VALUE}" "${CONDITION}" "${CONDITION_VALUE}")
	add_feature_info("${NAME}" "ENABLE_${UPPERCASE_NAME}" "${DESCRIPTION}")
endmacro()

macro (ms2_add_strict_dependent_option NAME DESCRIPTION DEFAULT_VALUE CONDITION CONDITION_VALUE STRICT_CONDITION ERROR_MSG)
	ms2_add_dependent_option("${NAME}" "${DESCRIPTION}" "${DEFAULT_VALUE}" "${CONDITION}" "${CONDITION_VALUE}")
	string(TOUPPER ${NAME} UPPERCASE_NAME)
	string(REGEX REPLACE  " " "_" UPPERCASE_NAME ${UPPERCASE_NAME})
	string(REGEX REPLACE  "\\+" "P" UPPERCASE_NAME ${UPPERCASE_NAME})
	if(${ENABLE_${UPPERCASE_NAME}} AND NOT ${STRICT_CONDITION})
		message(FATAL_ERROR "Trying to enable ${NAME} but ${ERROR_MSG}")
	endif()
endmacro()

macro (ms2_add_option NAME DESCRIPTION DEFAULT_VALUE)
	string(TOUPPER ${NAME} UPPERCASE_NAME)
	string(REGEX REPLACE  " " "_" UPPERCASE_NAME ${UPPERCASE_NAME})
	string(REGEX REPLACE  "\\+" "P" UPPERCASE_NAME ${UPPERCASE_NAME})
	option(ENABLE_${UPPERCASE_NAME} "Enable ${NAME}: ${DESCRIPTION}" "${DEFAULT_VALUE}")
	add_feature_info("${NAME}" "ENABLE_${UPPERCASE_NAME}" "${DESCRIPTION}")
endmacro()

ms2_add_option("GPL third parties" "Usage of GPL third-party code (FFmpeg and x264)." "${DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES}")
ms2_add_option("Non free codecs" "Allow inclusion of non-free codecs in the build." OFF)

#security options
ms2_add_option("SRTP" "SRTP media encryption support." "${DEFAULT_VALUE_ENABLE_SRTP}")
ms2_add_dependent_option("ZRTP" "ZRTP media encryption support." "${DEFAULT_VALUE_ENABLE_ZRTP}" "ENABLE_SRTP" OFF)
ms2_add_dependent_option("DTLS" "DTLS media encryption support." "${DEFAULT_VALUE_ENABLE_DTLS}" "ENABLE_SRTP" OFF)

#audio options and codecs
ms2_add_option("WebRTC AEC" "WebRTC echo canceller support." "${DEFAULT_VALUE_ENABLE_WEBRTC_AEC}")
ms2_add_dependent_option("WASAPI" "Windows Audio Session API (WASAPI) sound card support." "${DEFAULT_VALUE_ENABLE_WASAPI}" "MSVC" OFF)
ms2_add_strict_dependent_option("AMRNB" "AMR narrow-band audio encoding/decoding support (require license)." OFF ON OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
ms2_add_strict_dependent_option("AMRWB" "AMR wide-band audio encoding/decoding support (require license)." OFF ON OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
if(ENABLE_AMRNB OR ENABLE_AMRWB)
	set(ENABLE_AMR ON CACHE BOOL "" FORCE)
endif()
ms2_add_option("Codec2" "Codec2 audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_CODEC2}")
ms2_add_strict_dependent_option("G729" "G.729 audio encoding/decoding support (require license)." OFF ON OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
ms2_add_option("GSM"  "GSM audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_GSM}")
ms2_add_option("iLBC"  "iLBC audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_ILBC}")
ms2_add_option("ISAC"  "ISAC audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_ISAC}")
ms2_add_option("OPUS"  "OPUS audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_OPUS}")
ms2_add_option("Silk"  "Silk audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_SILK}")
ms2_add_option("Speex"  "Speex audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_SPEEX}")
ms2_add_option("BV16"  "BroadVoice 16 audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_BV16}")
#video options and codecs
ms2_add_option("Video" "Ability to capture and display video." "${DEFAULT_VALUE_ENABLE_VIDEO}")
# FFMpeg is LGPL which is an issue only for iOS applications; otherwise it can be used in proprietary software as well
if (IOS)
	ms2_add_strict_dependent_option("FFmpeg" "Some video processing features via FFmpeg: JPEG encoding/decoding, video scaling, H264 decoding..." "${DEFAULT_VALUE_ENABLE_FFMPEG}" "ENABLE_VIDEO" OFF "ENABLE_GPL_THIRD_PARTIES" "GPL third parties not enabled (ENABLE_GPL_THIRD_PARTIES).")
else()
	ms2_add_dependent_option("FFmpeg" "Some video processing features via FFmpeg: JPEG encoding/decoding, video scaling, H264 decoding..." "${DEFAULT_VALUE_ENABLE_FFMPEG}" "ENABLE_VIDEO" OFF)
endif()
ms2_add_strict_dependent_option("H263" "H263 video encoding/decoding support (require license)." OFF "ENABLE_FFMPEG" OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
ms2_add_strict_dependent_option("H263p" "H263+ video encoding/decoding support (require license)." OFF "ENABLE_FFMPEG" OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
ms2_add_option("MKV" "MKV playing and recording support." "${DEFAULT_VALUE_ENABLE_MKV}")
ms2_add_strict_dependent_option("MPEG4" "MPEG4 video encoding/decoding support (require license)." OFF "ENABLE_FFMPEG" OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
ms2_add_strict_dependent_option("OpenH264" "H.264 video encoding/decoding support with the openh264 library (require license)." OFF "ENABLE_VIDEO" OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
ms2_add_dependent_option("V4L" "V4L camera driver." ON "ENABLE_VIDEO;UNIX;NOT APPLE;NOT QNX;NOT ANDROID" OFF)
ms2_add_dependent_option("VPX" "VPX (VP8) video encoding/decoding support." "${DEFAULT_VALUE_ENABLE_VPX}" "ENABLE_VIDEO" OFF)
ms2_add_strict_dependent_option("X264" "H.264 video encoding support with the x264 library (require license)." OFF "ENABLE_VIDEO;ENABLE_GPL_THIRD_PARTIES" OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")

# Other options
ms2_add_option("PCAP" "PCAP support." "${DEFAULT_VALUE_ENABLE_PCAP}")
