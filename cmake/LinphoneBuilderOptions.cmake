############################################################################
# LinphoneBuilderOptions.cmake
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

# Define the build options

include(CMakeDependentOption)
include(FeatureSummary)

macro(linphone_builder_add_feature_info FEATURE_NAME FEATURE_OPTION FEATURE_DESCRIPTION)
	if("${CMAKE_VERSION}" VERSION_GREATER "2.8.2")
		add_feature_info("${FEATURE_NAME}" ${FEATURE_OPTION} "${FEATURE_DESCRIPTION}")
	endif()
endmacro()

option(ENABLE_VIDEO "Enable video support." ${DEFAULT_VALUE_ENABLE_VIDEO})
linphone_builder_add_feature_info("Video" ENABLE_VIDEO "Ability to capture and display video.")
option(ENABLE_GPL_THIRD_PARTIES "Enable GPL third-parties (FFmpeg)." ${DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES})
linphone_builder_add_feature_info("GPL third parties" ENABLE_GPL_THIRD_PARTIES "Usage of GPL third-party code (FFmpeg).")

option(ENABLE_SRTP "Enable SRTP support." ${DEFAULT_VALUE_ENABLE_SRTP})
linphone_builder_add_feature_info("SRTP" ENABLE_SRTP "SRTP media encryption support.")
cmake_dependent_option(ENABLE_ZRTP "Enable ZRTP support." ${DEFAULT_VALUE_ENABLE_ZRTP} "ENABLE_SRTP" OFF)
linphone_builder_add_feature_info("ZRTP" ENABLE_ZRTP "ZRTP media encryption support.")
cmake_dependent_option(ENABLE_DTLS "Enable DTLS support." ${DEFAULT_VALUE_ENABLE_DTLS} "ENABLE_SRTP" OFF)
linphone_builder_add_feature_info("DTLS" ENABLE_DTLS "DTLS media encryption support.")

option(ENABLE_NON_FREE_CODECS "Allow inclusion of non-free codecs in the build." YES)
option(ENABLE_AMRNB "Enable AMR narrow-band audio codec support." ${DEFAULT_VALUE_ENABLE_AMRNB})
linphone_builder_add_feature_info("AMR-NB" ENABLE_AMRNB "AMR narrow-band audio encoding/decoding support.")
option(ENABLE_AMRWB "Enable AMR wide-band audio codec support." ${DEFAULT_VALUE_ENABLE_AMRWB})
linphone_builder_add_feature_info("AMR-WB" ENABLE_AMRWB "AMR wide-band audio encoding/decoding support.")
if(ENABLE_AMRNB OR ENABLE_AMRWB)
	set(ENABLE_AMR ON CACHE BOOL "" FORCE)
endif()
option(ENABLE_G729 "Enable G.729 audio codec support." ${DEFAULT_VALUE_ENABLE_G729})
linphone_builder_add_feature_info("G.729" ENABLE_G729 "G.729 audio encoding/decoding support.")
option(ENABLE_GSM "Enable GSM audio codec support." ${DEFAULT_VALUE_ENABLE_GSM})
linphone_builder_add_feature_info("GSM" ENABLE_GSM "GSM audio encoding/decoding support.")
option(ENABLE_ILBC "Enable iLBC audio codec support." ${DEFAULT_VALUE_ENABLE_ILBC})
linphone_builder_add_feature_info("iLBC" ENABLE_ILBC "iLBC audio encoding/decoding support.")
option(ENABLE_ISAC "Enable ISAC audio codec support." ${DEFAULT_VALUE_ENABLE_ISAC})
linphone_builder_add_feature_info("ISAC" ENABLE_ISAC "ISAC audio encoding/decoding support.")
option(ENABLE_OPUS "Enable OPUS audio codec support." ${DEFAULT_VALUE_ENABLE_OPUS})
linphone_builder_add_feature_info("OPUS" ENABLE_OPUS "OPUS audio encoding/decoding support.")
option(ENABLE_SILK "Enable SILK audio codec support." ${DEFAULT_VALUE_ENABLE_SILK})
linphone_builder_add_feature_info("Silk" ENABLE_SILK "Silk audio encoding/decoding support.")
option(ENABLE_SPEEX "Enable speex audio codec support." ${DEFAULT_VALUE_ENABLE_SPEEX})
linphone_builder_add_feature_info("Speex" ENABLE_SPEEX "Speex audio encoding/decoding support.")

cmake_dependent_option(ENABLE_WASAPI "Enable Windows Audio Session API (WASAPI) sound card support." ${DEFAULT_VALUE_ENABLE_WASAPI} "MSVC" OFF)
linphone_builder_add_feature_info("WASAPI" ENABLE_WASAPI "Windows Audio Session API (WASAPI) sound card support.")
option(ENABLE_WEBRTC_AEC "Enable WebRTC echo canceller support." ${DEFAULT_VALUE_ENABLE_WEBRTC_AEC})
linphone_builder_add_feature_info("WebRTC AEC" ENABLE_WEBRTC_AEC "WebRTC echo canceller support.")

cmake_dependent_option(ENABLE_FFMPEG "Enable ffmpeg support." ${DEFAULT_VALUE_ENABLE_FFMPEG} "ENABLE_VIDEO;ENABLE_GPL_THIRD_PARTIES" OFF)
linphone_builder_add_feature_info("FFmpeg" ENABLE_FFMPEG "Some video processing features via FFmpeg: MPEG4 encoding/decoding, video scaling...")
cmake_dependent_option(ENABLE_H263 "Enable H263 video codec support." ${DEFAULT_VALUE_ENABLE_H263} "ENABLE_FFMPEG" OFF)
linphone_builder_add_feature_info("H263" ENABLE_H263 "H263 video encoding/decoding support.")
cmake_dependent_option(ENABLE_H263P "Enable H263+ video codec support." ${DEFAULT_VALUE_ENABLE_H263P} "ENABLE_FFMPEG" OFF)
linphone_builder_add_feature_info("H263+" ENABLE_H263P "H263+ video encoding/decoding support.")
cmake_dependent_option(ENABLE_MPEG4 "Enable MPEG4 video codec support." ${DEFAULT_VALUE_ENABLE_MPEG4} "ENABLE_FFMPEG" OFF)
linphone_builder_add_feature_info("MPEG4" ENABLE_MPEG4 "MPEG4 video encoding/decoding support.")
cmake_dependent_option(ENABLE_VPX "Enable VPX video codec support." ${DEFAULT_VALUE_ENABLE_VPX} "ENABLE_VIDEO" OFF)
linphone_builder_add_feature_info("VPX" ENABLE_VPX "VPX video encoding/decoding support.")
cmake_dependent_option(ENABLE_X264 "Enable H.264 video encoder support with the x264 library." ${DEFAULT_VALUE_ENABLE_X264} "ENABLE_FFMPEG; NOT ENABLE_OPENH264" OFF)
linphone_builder_add_feature_info("x264" ENABLE_X264 "H.264 video encoding support with the x264 library.")
cmake_dependent_option(ENABLE_OPENH264 "Enable H.264 video encoder support with the openh264 library." ${DEFAULT_VALUE_ENABLE_OPENH264} "ENABLE_VIDEO; NOT ENABLE_X264" OFF)
linphone_builder_add_feature_info("openh264" ENABLE_OPENH264 "H.264 video encoding support with the openh264 library.")
cmake_dependent_option(ENABLE_V4L "Enable V4L camera driver." ON "ENABLE_VIDEO; UNIX; NOT APPLE" OFF)
linphone_builder_add_feature_info("v4l" ENABLE_V4L "V4L camera driver.")
option(ENABLE_MKV "Enable MKV playing and recording support" ${DEFAULT_VALUE_ENABLE_MKV})
linphone_builder_add_feature_info("MKV" ENABLE_MKV "MKV playing and recording support.")

option(ENABLE_TUNNEL "Enable tunnel support." ${DEFAULT_VALUE_ENABLE_TUNNEL})
linphone_builder_add_feature_info("Tunnel" ENABLE_TUNNEL "Secure tunnel for SIP/RTP.")
option(ENABLE_UNIT_TESTS "Enable unit tests support with CUnit library." ${DEFAULT_VALUE_ENABLE_UNIT_TESTS})
linphone_builder_add_feature_info("Unit tests" ENABLE_UNIT_TESTS "Build unit tests programs for belle-sip, mediastreamer2 and linphone.")
option(ENABLE_DEBUG_LOGS "Enable debug level logs in libinphone and mediastreamer2." NO)

option(ENABLE_PACKAGING "Enable packaging" ${DEFAULT_VALUE_ENABLE_PACKAGING})

option(ENABLE_DOC "Enable documentation generation with Doxygen." YES)
linphone_builder_add_feature_info("Documentation" ENABLE_DOC "Enable documentation generation with Doxygen.")
