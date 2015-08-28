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

option(ENABLE_GPL_THIRD_PARTIES "Enable GPL third-parties (FFmpeg)." ${DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES})
add_feature_info("GPL third parties" ENABLE_GPL_THIRD_PARTIES "Usage of GPL third-party code (FFmpeg and x264).")
option(ENABLE_NON_FREE_CODECS "Allow inclusion of non-free codecs in the build." YES)

option(ENABLE_SRTP "Enable SRTP support." ${DEFAULT_VALUE_ENABLE_SRTP})
add_feature_info("SRTP" ENABLE_SRTP "SRTP media encryption support.")
cmake_dependent_option(ENABLE_ZRTP "Enable ZRTP support." ${DEFAULT_VALUE_ENABLE_ZRTP} "ENABLE_SRTP" OFF)
add_feature_info("ZRTP" ENABLE_ZRTP "ZRTP media encryption support.")
cmake_dependent_option(ENABLE_DTLS "Enable DTLS support." ${DEFAULT_VALUE_ENABLE_DTLS} "ENABLE_SRTP" OFF)
add_feature_info("DTLS" ENABLE_DTLS "DTLS media encryption support.")

cmake_dependent_option(ENABLE_AMRNB "Enable AMR narrow-band audio codec support." ${DEFAULT_VALUE_ENABLE_AMRNB} "ENABLE_NON_FREE_CODECS" OFF)
add_feature_info("AMR-NB" ENABLE_AMRNB "AMR narrow-band audio encoding/decoding support (require license).")
cmake_dependent_option(ENABLE_AMRWB "Enable AMR wide-band audio codec support." ${DEFAULT_VALUE_ENABLE_AMRWB} "ENABLE_NON_FREE_CODECS" OFF)
add_feature_info("AMR-WB" ENABLE_AMRWB "AMR wide-band audio encoding/decoding support (require license).")
if(ENABLE_AMRNB OR ENABLE_AMRWB)
	set(ENABLE_AMR ON CACHE BOOL "" FORCE)
endif()
cmake_dependent_option(ENABLE_G729 "Enable G.729 audio codec support." ${DEFAULT_VALUE_ENABLE_G729} "ENABLE_NON_FREE_CODECS" OFF)
add_feature_info("G.729" ENABLE_G729 "G.729 audio encoding/decoding support (require license).")
option(ENABLE_GSM "Enable GSM audio codec support." ${DEFAULT_VALUE_ENABLE_GSM})
add_feature_info("GSM" ENABLE_GSM "GSM audio encoding/decoding support.")
option(ENABLE_ILBC "Enable iLBC audio codec support." ${DEFAULT_VALUE_ENABLE_ILBC})
add_feature_info("iLBC" ENABLE_ILBC "iLBC audio encoding/decoding support.")
option(ENABLE_ISAC "Enable ISAC audio codec support." ${DEFAULT_VALUE_ENABLE_ISAC})
add_feature_info("ISAC" ENABLE_ISAC "ISAC audio encoding/decoding support.")
option(ENABLE_OPUS "Enable OPUS audio codec support." ${DEFAULT_VALUE_ENABLE_OPUS})
add_feature_info("OPUS" ENABLE_OPUS "OPUS audio encoding/decoding support.")
option(ENABLE_SILK "Enable SILK audio codec support." ${DEFAULT_VALUE_ENABLE_SILK})
add_feature_info("Silk" ENABLE_SILK "Silk audio encoding/decoding support.")
option(ENABLE_SPEEX "Enable speex audio codec support." ${DEFAULT_VALUE_ENABLE_SPEEX})
add_feature_info("Speex" ENABLE_SPEEX "Speex audio encoding/decoding support.")

cmake_dependent_option(ENABLE_WASAPI "Enable Windows Audio Session API (WASAPI) sound card support." ${DEFAULT_VALUE_ENABLE_WASAPI} "MSVC" OFF)
add_feature_info("WASAPI" ENABLE_WASAPI "Windows Audio Session API (WASAPI) sound card support.")
option(ENABLE_WEBRTC_AEC "Enable WebRTC echo canceller support." ${DEFAULT_VALUE_ENABLE_WEBRTC_AEC})
add_feature_info("WebRTC AEC" ENABLE_WEBRTC_AEC "WebRTC echo canceller support.")

option(ENABLE_VIDEO "Enable video support." ${DEFAULT_VALUE_ENABLE_VIDEO})
add_feature_info("Video" ENABLE_VIDEO "Ability to capture and display video.")
# FFMpeg is LGPL which is an issue only for iOS applications
if (IOS)
	cmake_dependent_option(ENABLE_FFMPEG "Enable ffmpeg support." ${DEFAULT_VALUE_ENABLE_FFMPEG} "ENABLE_VIDEO; ENABLE_GPL_THIRD_PARTIES" OFF)
else()
	cmake_dependent_option(ENABLE_FFMPEG "Enable ffmpeg support." ${DEFAULT_VALUE_ENABLE_FFMPEG} "ENABLE_VIDEO" OFF)
endif()
add_feature_info("FFmpeg" ENABLE_FFMPEG "Some video processing features via FFmpeg: MPEG4 encoding/decoding, video scaling...")
cmake_dependent_option(ENABLE_H263 "Enable H263 video codec support." ${DEFAULT_VALUE_ENABLE_H263} "ENABLE_FFMPEG; ENABLE_NON_FREE_CODECS" OFF)
add_feature_info("H263" ENABLE_H263 "H263 video encoding/decoding support (require license).")
cmake_dependent_option(ENABLE_H263P "Enable H263+ video codec support." ${DEFAULT_VALUE_ENABLE_H263P} "ENABLE_FFMPEG; ENABLE_NON_FREE_CODECS" OFF)
add_feature_info("H263+" ENABLE_H263P "H263+ video encoding/decoding support (require license).")
cmake_dependent_option(ENABLE_MPEG4 "Enable MPEG4 video codec support." ${DEFAULT_VALUE_ENABLE_MPEG4} "ENABLE_FFMPEG; ENABLE_NON_FREE_CODECS" OFF)
add_feature_info("MPEG4" ENABLE_MPEG4 "MPEG4 video encoding/decoding support (require license).")
cmake_dependent_option(ENABLE_VPX "Enable VPX (VP8) video codec support." ${DEFAULT_VALUE_ENABLE_VPX} "ENABLE_VIDEO" OFF)
add_feature_info("VPX" ENABLE_VPX "VPX (VP8) video encoding/decoding support.")
cmake_dependent_option(ENABLE_X264 "Enable H.264 video encoder support with the x264 library." ${DEFAULT_VALUE_ENABLE_X264} "ENABLE_FFMPEG; ENABLE_GPL_THIRD_PARTIES; ENABLE_NON_FREE_CODECS; NOT ENABLE_OPENH264" OFF)
add_feature_info("x264" ENABLE_X264 "H.264 video encoding support with the x264 library (require license).")
cmake_dependent_option(ENABLE_OPENH264 "Enable H.264 video encoder support with the openh264 library." ${DEFAULT_VALUE_ENABLE_OPENH264} "ENABLE_VIDEO; ENABLE_NON_FREE_CODECS; NOT ENABLE_X264" OFF)
add_feature_info("openh264" ENABLE_OPENH264 "H.264 video encoding support with the openh264 library (require license).")
cmake_dependent_option(ENABLE_V4L "Enable V4L camera driver." ${DEFAULT_VALUE_ENABLE_V4L} "ENABLE_VIDEO; UNIX; NOT APPLE" OFF)
add_feature_info("v4l" ENABLE_V4L "V4L camera driver.")
option(ENABLE_MKV "Enable MKV playing and recording support" ${DEFAULT_VALUE_ENABLE_MKV})
add_feature_info("MKV" ENABLE_MKV "MKV playing and recording support.")
