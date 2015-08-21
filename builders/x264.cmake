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

set(EP_x264_GIT_REPOSITORY "git://git.videolan.org/x264.git")
set(EP_x264_GIT_TAG "adc99d17d8c1fbc164fae8319b40d7c45f30314e")
set(EP_x264_EXTERNAL_SOURCE_PATHS "externals/x264")

set(EP_x264_BUILD_METHOD "autotools")
set(EP_x264_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)
set(EP_x264_CONFIGURE_OPTIONS
	"--extra-cflags=${LINPHONE_BUILDER_CFLAGS}"
	"--extra-ldflags=${LINPHONE_BUILDER_LDFLAGS}"
)
set(EP_x264_LINKING_TYPE "--enable-shared")
set(EP_x264_CONFIGURE_ENV "CC=$CC")
set(EP_x264_INSTALL_TARGET "install-lib-shared")
set(EP_x264_DEPENDENCIES EP_ffmpeg)

