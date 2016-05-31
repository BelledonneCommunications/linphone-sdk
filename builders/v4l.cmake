############################################################################
# v4l.cmake
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

set(EP_v4l_GIT_REPOSITORY "git://linuxtv.org/v4l-utils.git" CACHE STRING "v4l repository URL")
set(EP_v4l_GIT_TAG_LATEST "master" CACHE STRING "v4l tag to use when compiling latest version")
set(EP_v4l_GIT_TAG "v4l-utils-1.0.0" CACHE STRING "v4l tag to use")
set(EP_v4l_MAY_BE_FOUND_ON_SYSTEM TRUE)
set(EP_v4l_IGNORE_WARNINGS TRUE)
set(EP_v4l_EXTERNAL_SOURCE_PATHS "externals/v4l-utils")

set(EP_v4l_BUILD_METHOD "autotools")
set(EP_v4l_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)
set(EP_v4l_CONFIGURE_OPTIONS
	"--disable-v4l-utils"
	"--disable-libdvbv5"
	"--with-udevdir=${CMAKE_INSTALL_PREFIX}/etc"
	"--without-jpeg"
)
set(EP_v4l_LINKING_TYPE "--disable-static" "--enable-shared")
