############################################################################
# linphone.cmake
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

set(EP_linphone_GIT_REPOSITORY "git://git.linphone.org/linphone.git")
if(${LINPHONE_BUILDER_LATEST})
	set(EP_linphone_GIT_TAG "master")
else()
	set(EP_linphone_GIT_TAG "af6678a973e77f6b0bb2fb66e1ba08987c5be3a0")
endif()

if(MSVC)
	# Use temporary CMake build scripts for Windows. TODO: Port fully to CMake.
	set(EP_linphone_DEPENDENCIES EP_bellesip EP_ortp EP_ms2 EP_xml2)
	set(EP_linphone_EXTRA_LDFLAGS "/SAFESEH:NO")
else()
	set(EP_linphone_BUILD_METHOD "autotools")
	set(EP_linphone_USE_AUTOGEN "yes")
	set(EP_linphone_CROSS_COMPILATION_OPTIONS
		"--prefix=${CMAKE_INSTALL_PREFIX}"
		"--host=${LINPHONE_BUILDER_HOST}"
	)
	set(EP_linphone_CONFIGURE_OPTIONS
		"--disable-strict"
		"--enable-bellesip"
		"--enable-external-ortp"
		"--enable-external-mediastreamer"
	)
	set(EP_linphone_LINKING_TYPE "--disable-static" "--enable-shared")
	set(EP_linphone_DEPENDENCIES EP_bellesip EP_ortp EP_ms2 EP_xml2)

	if(${ENABLE_SPEEX})
		list(APPEND EP_linphone_CONFIGURE_OPTIONS "--enable-speex")
	else(${ENABLE_SPEEX})
		list(APPEND EP_linphone_CONFIGURE_OPTIONS "--disable-speex")
	endif(${ENABLE_SPEEX})
	if(${ENABLE_VIDEO})
		list(APPEND EP_linphone_CONFIGURE_OPTIONS "--enable-video")
	else(${ENABLE_VIDEO})
		list(APPEND EP_linphone_CONFIGURE_OPTIONS "--disable-video")
	endif(${ENABLE_VIDEO})

	if(${ENABLE_ZRTP})
		list(APPEND EP_linphone_CONFIGURE_OPTIONS "--enable-zrtp")
	else(${ENABLE_ZRTP})
		list(APPEND EP_linphone_CONFIGURE_OPTIONS "--disable-zrtp")
	endif(${ENABLE_ZRTP})

	if(${ENABLE_TUNNEL})
		list(APPEND EP_linphone_CONFIGURE_OPTIONS "--enable-tunnel")
		list(APPEND EP_linphone_DEPENDENCIES EP_tunnel)
	else(${ENABLE_TUNNEL})
		list(APPEND EP_linphone_CONFIGURE_OPTIONS "--disable-tunnel")
	endif(${ENABLE_TUNNEL})

	if(${ENABLE_UNIT_TESTS})
		list(APPEND EP_linphone_DEPENDENCIES EP_cunit)
	else(${ENABLE_UNIT_TESTS})
		list(APPEND EP_linphone_CONFIGURE_OPTIONS "--disable-tests")
	endif(${ENABLE_UNIT_TESTS})
endif()
