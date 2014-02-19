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

# Include linphone builder options definitions
include(cmake/LinphoneBuilderOptions.cmake)


# List of the projects to build. The order is important and must follow the dependencies.
set(LINPHONE_BUILDER_BUILDERS )
if(${ENABLE_UNIT_TESTS})
	list(APPEND LINPHONE_BUILDER_BUILDERS cunit)
endif(${ENABLE_UNIT_TESTS})
if(${ENABLE_SRTP})
	list(APPEND LINPHONE_BUILDER_BUILDERS srtp)
endif(${ENABLE_SRTP})
if(${ENABLE_GSM})
	list(APPEND LINPHONE_BUILDER_BUILDERS gsm)
endif(${ENABLE_GSM})
if(${ENABLE_OPUS})
	list(APPEND LINPHONE_BUILDER_BUILDERS opus)
endif(${ENABLE_OPUS})
if(${ENABLE_SPEEX})
	list(APPEND LINPHONE_BUILDER_BUILDERS speex)
endif(${ENABLE_SPEEX})
if(${ENABLE_FFMPEG})
	list(APPEND LINPHONE_BUILDER_BUILDERS ffmpeg)
endif(${ENABLE_FFMPEG})
if(${ENABLE_VPX})
	list(APPEND LINPHONE_BUILDER_BUILDERS vpx)
endif(${ENABLE_VPX})
if("${BUILD_V4L}" STREQUAL "yes")
	list(APPEND LINPHONE_BUILDER_BUILDERS v4l)
endif("${BUILD_V4L}" STREQUAL "yes")

list(APPEND LINPHONE_BUILDER_BUILDERS
	xml2
	antlr3c
	polarssl
	bellesip
	ortp
	ms2
	linphone
)


# Include builders
foreach(BUILDER ${LINPHONE_BUILDER_BUILDERS})
	message("-- Including builder ${BUILDER}")
	include(${CMAKE_CURRENT_SOURCE_DIR}/builders/${BUILDER}.cmake)
endforeach(BUILDER)
