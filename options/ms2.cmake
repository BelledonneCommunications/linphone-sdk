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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

# Mediastreamer2 build options

add_option("GPL third parties" "Usage of GPL third-party code (FFmpeg and x264)." "${DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES}")
add_option("Non free codecs" "Allow inclusion of non-free codecs in the build." OFF)

#security options
add_option("SRTP" "SRTP media encryption support." "${DEFAULT_VALUE_ENABLE_SRTP}")
add_dependent_option("ZRTP" "ZRTP media encryption support." "${DEFAULT_VALUE_ENABLE_ZRTP}" "ENABLE_SRTP" OFF)

#audio options and codecs
add_option("WebRTC AEC" "WebRTC echo canceller support." "${DEFAULT_VALUE_ENABLE_WEBRTC_AEC}")
add_dependent_option("WASAPI" "Windows Audio Session API (WASAPI) sound card support." "${DEFAULT_VALUE_ENABLE_WASAPI}" "MSVC" OFF)
add_strict_dependent_option("AMRNB" "AMR narrow-band audio encoding/decoding support (require license)." OFF ON OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
add_strict_dependent_option("AMRWB" "AMR wide-band audio encoding/decoding support (require license)." OFF ON OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
if(ENABLE_AMRNB OR ENABLE_AMRWB)
	set(ENABLE_AMR ON CACHE BOOL "" FORCE)
endif()
add_option("Codec2" "Codec2 audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_CODEC2}")
add_strict_dependent_option("G729" "G.729 audio encoding/decoding support (require license)." OFF ON OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
add_strict_dependent_option("G729B CNG" "G.729 annex B confort noise generation (require license)." OFF ON OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
add_option("G726" "G.726 audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_G726}")
add_option("GSM"  "GSM audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_GSM}")
add_option("iLBC"  "iLBC audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_ILBC}")
add_option("ISAC"  "ISAC audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_ISAC}")
add_option("OPUS"  "OPUS audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_OPUS}")
add_option("Silk"  "Silk audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_SILK}")
add_option("Speex"  "Speex audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_SPEEX}")
add_option("BV16"  "BroadVoice 16 audio encoding/decoding support." "${DEFAULT_VALUE_ENABLE_BV16}")
#video options and codecs
add_option("Video" "Ability to capture and display video." "${DEFAULT_VALUE_ENABLE_VIDEO}")
# FFMpeg is LGPL which is an issue only for iOS applications; otherwise it can be used in proprietary software as well
if (IOS)
	add_strict_dependent_option("FFmpeg" "Some video processing features via FFmpeg: JPEG encoding/decoding, video scaling, H264 decoding..." "${DEFAULT_VALUE_ENABLE_FFMPEG}" "ENABLE_VIDEO" OFF "ENABLE_GPL_THIRD_PARTIES" "GPL third parties not enabled (ENABLE_GPL_THIRD_PARTIES).")
else()
	add_dependent_option("FFmpeg" "Some video processing features via FFmpeg: JPEG encoding/decoding, video scaling, H264 decoding..." "${DEFAULT_VALUE_ENABLE_FFMPEG}" "ENABLE_VIDEO" OFF)
endif()
add_strict_dependent_option("H263" "H263 video encoding/decoding support (require license)." OFF "ENABLE_FFMPEG" OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
add_strict_dependent_option("H263p" "H263+ video encoding/decoding support (require license)." OFF "ENABLE_FFMPEG" OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
add_option("MKV" "MKV playing and recording support." "${DEFAULT_VALUE_ENABLE_MKV}")
add_strict_dependent_option("MPEG4" "MPEG4 video encoding/decoding support (require license)." OFF "ENABLE_FFMPEG" OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
add_strict_dependent_option("OpenH264" "H.264 video encoding/decoding support with the openh264 library (require license)." OFF "ENABLE_VIDEO" OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")
add_dependent_option("V4L" "V4L camera driver." ON "ENABLE_VIDEO;UNIX;NOT APPLE;NOT QNX;NOT ANDROID" OFF)
add_dependent_option("VPX" "VPX (VP8) video encoding/decoding support." "${DEFAULT_VALUE_ENABLE_VPX}" "ENABLE_VIDEO" OFF)
add_strict_dependent_option("X264" "H.264 video encoding support with the x264 library (require license)." OFF "ENABLE_VIDEO;ENABLE_GPL_THIRD_PARTIES" OFF "ENABLE_NON_FREE_CODECS" "non free codecs option not enabled (ENABLE_NON_FREE_CODECS).")

# Other options
add_option("PCAP" "PCAP support." "${DEFAULT_VALUE_ENABLE_PCAP}")
