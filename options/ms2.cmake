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
function (ms2_add_option NAME DESCRIPTION DEFAULT_VALUE)
	string(TOUPPER ${NAME} UPPERCASE_NAME)
	string(REGEX REPLACE  " " "_" UPPERCASE_NAME ${UPPERCASE_NAME})
	string(REGEX REPLACE  "\\+" "P" UPPERCASE_NAME ${UPPERCASE_NAME})
	if (ARGC EQUAL 5)
		cmake_dependent_option(ENABLE_${UPPERCASE_NAME} "${DESCRIPTION}" "${DEFAULT_VALUE}" "${ARGV3}" "${ARGV4}")
	elseif (ARGC EQUAL 3)
		option(ENABLE_${UPPERCASE_NAME} "Enable ${NAME}: ${DESCRIPTION}" "${DEFAULT_VALUE}")
	else()
		message(FATAL_ERROR "Invalid arguments count: ${ARGC}. Expected 3 or 5")
	endif()
	add_feature_info(${NAME} ENABLE_${UPPERCASE_NAME} ${DESCRIPTION})
endfunction()

ms2_add_option("GPL third parties" "Usage of GPL third-party code (FFmpeg and x264)." ${DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES})
ms2_add_option("Non free codecs" "Allow inclusion of non-free codecs in the build." OFF)

# by default, non free codecs must be disabled BUT they can be enabled one by one
# using command line -DENABLE_CODEC=YES for instance
if (NOT ENABLE_NON_FREE_CODECS)
	set(DEFAULT_VALUE_ENABLE_AMRNB OFF)
	set(DEFAULT_VALUE_ENABLE_AMRWB OFF)
	set(DEFAULT_VALUE_ENABLE_G729 OFF)
	set(DEFAULT_VALUE_ENABLE_MPEG4 OFF)
	set(DEFAULT_VALUE_ENABLE_H263 OFF)
	set(DEFAULT_VALUE_ENABLE_H263P OFF)
	set(DEFAULT_VALUE_ENABLE_OPENH264 OFF)
	if (NOT ENABLE_GPL_THIRD_PARTIES)
		set(DEFAULT_VALUE_ENABLE_X264 OFF)
	endif()
endif()

#security options
ms2_add_option("SRTP" "SRTP media encryption support." ${DEFAULT_VALUE_ENABLE_SRTP})
ms2_add_option("ZRTP" "ZRTP media encryption support." ${DEFAULT_VALUE_ENABLE_ZRTP} "ENABLE_SRTP" OFF)
ms2_add_option("DTLS" "DTLS media encryption support." ${DEFAULT_VALUE_ENABLE_DTLS} "ENABLE_SRTP" OFF)

#audio codecs
ms2_add_option("AMR-NB" "AMR narrow-band audio encoding/decoding support (require license)." ${DEFAULT_VALUE_ENABLE_AMRNB})
ms2_add_option("AMR-WB" "AMR wide-band audio encoding/decoding support (require license)." ${DEFAULT_VALUE_ENABLE_AMRWB})
if(ENABLE_AMRNB OR ENABLE_AMRWB)
	set(ENABLE_AMR ON CACHE BOOL "" FORCE)
endif()
ms2_add_option("G.729" "G.729 audio encoding/decoding support (require license)." ${DEFAULT_VALUE_ENABLE_G729})
ms2_add_option("GSM"  "GSM audio encoding/decoding support." ${DEFAULT_VALUE_ENABLE_GSM})
ms2_add_option("iLBC"  "iLBC audio encoding/decoding support." ${DEFAULT_VALUE_ENABLE_ILBC})
ms2_add_option("ISAC"  "ISAC audio encoding/decoding support." ${DEFAULT_VALUE_ENABLE_ISAC})
ms2_add_option("OPUS"  "OPUS audio encoding/decoding support." ${DEFAULT_VALUE_ENABLE_OPUS})
ms2_add_option("Silk"  "Silk audio encoding/decoding support." ${DEFAULT_VALUE_ENABLE_SILK})
ms2_add_option("Speex"  "Speex audio encoding/decoding support." ${DEFAULT_VALUE_ENABLE_SPEEX})

ms2_add_option("WASAPI" "Windows Audio Session API (WASAPI) sound card support." ${DEFAULT_VALUE_ENABLE_WASAPI} "MSVC" OFF)
ms2_add_option("WebRTC AEC" "WebRTC echo canceller support." ${DEFAULT_VALUE_ENABLE_WEBRTC_AEC})

#video option and codecs
ms2_add_option("Video" "Ability to capture and display video." ${DEFAULT_VALUE_ENABLE_VIDEO})
# FFMpeg is LGPL which is an issue only for iOS applications; otherwise it can be used in proprietary software as well
set(FFMPEG_DEPENDENT_PRED "ENABLE_VIDEO")
if (IOS)
	set(FFMPEG_DEPENDENT_PRED "${FFMPEG_DEPENDENT_PRED}; ENABLE_GPL_THIRD_PARTIES")
endif()
ms2_add_option("FFmpeg" "Some video processing features via FFmpeg: JPEG encoding/decoding, video scaling, H264 decoding..." ${DEFAULT_VALUE_ENABLE_FFMPEG} "${FFMPEG_DEPENDENT_PRED}" OFF)
ms2_add_option("H263" "H263 video encoding/decoding support (require license)." ${DEFAULT_VALUE_ENABLE_H263} "ENABLE_FFMPEG" OFF)
ms2_add_option("H263+" "H263+ video encoding/decoding support (require license)." ${DEFAULT_VALUE_ENABLE_H263P} "ENABLE_FFMPEG" OFF)
ms2_add_option("VPX" "VPX (VP8) video encoding/decoding support." ${DEFAULT_VALUE_ENABLE_VPX} "ENABLE_VIDEO" OFF)
ms2_add_option("MPEG4" "MPEG4 video encoding/decoding support (require license)." ${DEFAULT_VALUE_ENABLE_MPEG4} "ENABLE_FFMPEG" OFF)
ms2_add_option("OpenH264" "H.264 video encoding/decoding support with the openh264 library (require license)." ${DEFAULT_VALUE_ENABLE_OPENH264} "ENABLE_VIDEO" OFF)
ms2_add_option("X264" "H.264 video encoding support with the x264 library (require license)." ${DEFAULT_VALUE_ENABLE_X264} "ENABLE_VIDEO" OFF)
ms2_add_option("V4L" "V4L camera driver." ON "ENABLE_VIDEO; UNIX; NOT APPLE" OFF)
ms2_add_option("MKV" "MKV playing and recording support." ${DEFAULT_VALUE_ENABLE_MKV})
