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

option(ENABLE_VIDEO "Enable video support." ${DEFAULT_VALUE_ENABLE_VIDEO})
option(ENABLE_GPL_THIRD_PARTIES "Enable GPL third-parties (ffmpeg and ZRTP)." ${DEFAULT_VALUE_ENABLE_GPL_THIRD_PARTIES})

cmake_dependent_option(ENABLE_FFMPEG "Enable ffmpeg support." ${DEFAULT_VALUE_ENABLE_FFMPEG} "ENABLE_VIDEO;ENABLE_GPL_THIRD_PARTIES" OFF)
cmake_dependent_option(ENABLE_ZRTP "Enable ZRTP support." ${DEFAULT_VALUE_ENABLE_ZRTP} "ENABLE_GPL_THIRD_PARTIES" OFF)

option(ENABLE_SRTP "Enable SRTP support." ${DEFAULT_VALUE_ENABLE_SRTP})

option(ENABLE_AMR "Enable AMR audio codec support." ${DEFAULT_VALUE_ENABLE_AMR})
option(ENABLE_G729 "Enable G.729 audio codec support." ${DEFAULT_VALUE_ENABLE_G729})
option(ENABLE_GSM "Enable GSM audio codec support." ${DEFAULT_VALUE_ENABLE_GSM})
option(ENABLE_ILBC "Enable iLBC audio codec support." ${DEFAULT_VALUE_ENABLE_ILBC})
option(ENABLE_ISAC "Enable ISAC audio codec support." ${DEFAULT_VALUE_ENABLE_ISAC})
option(ENABLE_OPUS "Enable OPUS audio codec support." ${DEFAULT_VALUE_ENABLE_OPUS})
option(ENABLE_SILK "Enable SILK audio codec support." ${DEFAULT_VALUE_ENABLE_SILK})
option(ENABLE_SPEEX "Enable speex audio codec support." ${DEFAULT_VALUE_ENABLE_SPEEX})

cmake_dependent_option(ENABLE_VPX "Enable VPX video codec support." ${DEFAULT_VALUE_ENABLE_VPX} "ENABLE_VIDEO" OFF)
cmake_dependent_option(ENABLE_X264 "Enable H.264 audio encoder support with the x264 library." ${DEFAULT_VALUE_ENABLE_X264} "ENABLE_VIDEO" OFF)

option(ENABLE_TUNNEL "Enable tunnel support." ${DEFAULT_VALUE_ENABLE_TUNNEL})
option(ENABLE_UNIT_TESTS "Enable unit tests support with CUnit library." ${DEFAULT_VALUE_ENABLE_UNIT_TESTS})
