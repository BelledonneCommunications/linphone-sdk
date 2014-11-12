############################################################################
# ortp.cmake
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

set(EP_ortp_GIT_REPOSITORY "git://git.linphone.org/ortp.git")
if(LINPHONE_BUILDER_LATEST)
	set(EP_ortp_GIT_TAG "master")
else()
	set(EP_ortp_GIT_TAG "a8dcd5b3ee563b5746199a605b94459308f5d009")
endif()

if("${EP_ortp_FORCE_AUTOTOOLS}" STREQUAL "yes")
	set(EP_ortp_LINKING_TYPE "--enable-static")
	set(EP_ortp_USE_AUTOGEN "yes")
else()
	set(EP_ortp_LINKING_TYPE "-DENABLE_STATIC=0")
endif()

set(EP_ortp_CMAKE_OPTIONS )
set(EP_ortp_DEPENDENCIES )
if(ENABLE_SRTP)
	list(APPEND EP_ortp_CMAKE_OPTIONS "-DENABLE_SRTP=1")
	list(APPEND EP_ortp_CONFIGURE_OPTIONS "--enable-srtp")
	list(APPEND EP_ortp_DEPENDENCIES EP_srtp)
else()
	list(APPEND EP_ortp_CMAKE_OPTIONS "-DENABLE_SRTP=0")
	list(APPEND EP_ortp_CONFIGURE_OPTIONS "--disable-srtp")
endif()
if(ENABLE_ZRTP)
	list(APPEND EP_ortp_CMAKE_OPTIONS "-DENABLE_ZRTP=1")
	list(APPEND EP_ortp_CONFIGURE_OPTIONS "--enable-zrtp")
	list(APPEND EP_ortp_DEPENDENCIES EP_bzrtp)
else()
	list(APPEND EP_ortp_CMAKE_OPTIONS "-DENABLE_ZRTP=0")
	list(APPEND EP_ortp_CONFIGURE_OPTIONS "--with-srtp=none")
endif()
if(MSVC)
	set(EP_ortp_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()


set(EP_ortp_SPEC_FILE "ortp.spec")
