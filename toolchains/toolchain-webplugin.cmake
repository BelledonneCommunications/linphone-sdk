############################################################################
# toolchain-webplugin.cmake
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

# Restrict pkg-config to search in the install directory
set(LINPHONE_BUILDER_PKG_CONFIG_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)

if (UNIX)
	if(APPLE)
		set(CMAKE_OSX_DEPLOYMENT_TARGET "10.6")
		set(CMAKE_OSX_ARCHITECTURES "i386")
		set(LINPHONE_BUILDER_TOOLCHAIN_HOST "i686-apple-darwin")
		set(LINPHONE_BUILDER_TOOLCHAIN_CPPFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
		set(LINPHONE_BUILDER_TOOLCHAIN_CFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
		set(LINPHONE_BUILDER_TOOLCHAIN_LDFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
	else(APPLE)
		set(BUILD_V4L "yes")
	endif(APPLE)

	set(LINPHONE_BUILDER_TOOLCHAIN_CFLAGS "-fPIC")
endif(UNIX)

if(WIN32)
	set(LINPHONE_BUILDER_TOOLCHAIN_CPPFLAGS "-D_WIN32_WINNT=0x0501 -D_ALLOW_KEYWORD_MACROS")
endif(WIN32)
