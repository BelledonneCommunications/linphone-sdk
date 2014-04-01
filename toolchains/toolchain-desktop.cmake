############################################################################
# toolchain-desktop.cmake
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

# Default toolchain, use native environment.

set(LINPHONE_BUILDER_TOOLCHAIN_HOST "")
set(LINPHONE_BUILDER_TOOLCHAIN_CC "${CMAKE_C_COMPILER}")
set(LINPHONE_BUILDER_TOOLCHAIN_CXX "${CMAKE_CXX_COMPILER}")
set(LINPHONE_BUILDER_TOOLCHAIN_OBJC "${CMAKE_C_COMPILER}")
set(LINPHONE_BUILDER_TOOLCHAIN_LD "${CMAKE_LINKER}")
set(LINPHONE_BUILDER_TOOLCHAIN_AR "${CMAKE_AR}")
set(LINPHONE_BUILDER_TOOLCHAIN_RANLIB "${CMAKE_RANLIB}")
set(LINPHONE_BUILDER_TOOLCHAIN_STRIP "${CMAKE_STRIP}")
set(LINPHONE_BUILDER_TOOLCHAIN_NM "${CMAKE_NM}")

# Adjust PKG_CONFIG_PATH to include install directory
if(UNIX)
	set(LINPHONE_BUILDER_PKG_CONFIG_PATH "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/:$ENV{PKG_CONFIG_PATH}:/usr/lib/pkgconfig/:/usr/lib/x86_64-linux-gnu/pkgconfig/:/usr/share/pkgconfig/:/usr/local/lib/pkgconfig/:/opt/local/lib/pkgconfig/")
endif(UNIX)

if(APPLE)
	set(CMAKE_OSX_DEPLOYMENT_TARGET "10.6")
	set(CMAKE_OSX_ARCHITECTURES "i386")
	set(LINPHONE_BUILDER_TOOLCHAIN_HOST "i686-apple-darwin")
	set(LINPHONE_BUILDER_TOOLCHAIN_CPPFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
	set(LINPHONE_BUILDER_TOOLCHAIN_CFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
	set(LINPHONE_BUILDER_TOOLCHAIN_LDFLAGS "-mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET} -arch ${CMAKE_OSX_ARCHITECTURES}")
endif(APPLE)

