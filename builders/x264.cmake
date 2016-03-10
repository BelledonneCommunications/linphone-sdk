############################################################################
# x264.cmake
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

set(EP_x264_GIT_REPOSITORY "git://git.videolan.org/x264.git" CACHE STRING "x264 repository URL")
set(EP_x264_GIT_TAG "adc99d17d8c1fbc164fae8319b40d7c45f30314e" CACHE STRING "x264 tag to use")
set(EP_x264_EXTERNAL_SOURCE_PATHS "externals/x264")

set(EP_x264_BUILD_METHOD "autotools")
set(EP_x264_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)
set(EP_x264_CONFIGURE_OPTIONS
	"--extra-cflags=$CFLAGS"
	"--extra-ldflags=$LDFLAGS"
)
if(IOS)
	list(APPEND EP_x264_CONFIGURE_OPTIONS "--sysroot=${CMAKE_OSX_SYSROOT}")
	string(REGEX MATCH "^(arm*|aarch64)" ARM_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
	if(ARM_ARCH AND NOT ${XCODE_VERSION} VERSION_LESS 7)
		list(APPEND EP_x264_CONFIGURE_OPTIONS "--extra-asflags=-fembed-bitcode")
	endif()
elseif(ANDROID)
	list(APPEND EP_x264_CONFIGURE_OPTIONS "--sysroot=${CMAKE_SYSROOT}")
	set(EP_x264_CROSS_COMPILATION_OPTIONS "--prefix=${CMAKE_INSTALL_PREFIX}")
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
		list(APPEND EP_x264_CROSS_COMPILATION_OPTIONS "--host=arm-none-linux-gnueabi")
	else()
		list(APPEND EP_x264_CROSS_COMPILATION_OPTIONS "--host=i686-linux-gnueabi")
	endif()
	set(EP_x264_USE_C_COMPILER_FOR_ASSEMBLER TRUE)
endif()
set(EP_x264_LINKING_TYPE "--enable-shared")
if(IOS)
	set(EP_x264_CONFIGURE_ENV "CC=\"$CC -arch ${LINPHONE_BUILDER_OSX_ARCHITECTURES}\"")
else()
	set(EP_x264_CONFIGURE_ENV "CC=$CC")
endif()
set(EP_x264_INSTALL_TARGET "install-lib-shared")
set(EP_x264_DEPENDENCIES )
