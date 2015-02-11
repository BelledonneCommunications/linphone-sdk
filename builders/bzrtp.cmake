############################################################################
# bzrtp.cmake
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

set(EP_bzrtp_GIT_REPOSITORY "git://git.linphone.org/bzrtp.git")
set(EP_bzrtp_GIT_TAG_LATEST "master")
set(EP_bzrtp_GIT_TAG "c794ff88379f8b49bd6390e002e0504794507a7f")

set(EP_bzrtp_CMAKE_OPTIONS )
set(EP_bzrtp_LINKING_TYPE "-DENABLE_STATIC=0")
set(EP_bzrtp_DEPENDENCIES EP_polarssl EP_xml2)
if(MSVC)
	set(EP_bzrtp_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()
if(MINGW)
	set(EP_bzrtp_EXTRA_CPPFLAGS "-D__USE_MINGW_ANSI_STDIO")
endif()

if(ENABLE_UNIT_TESTS)
	list(APPEND EP_bzrtp_CMAKE_OPTIONS "-DENABLE_TESTS=1")
	list(APPEND EP_bzrtp_DEPENDENCIES EP_cunit)
else()
	list(APPEND EP_bzrtp_CMAKE_OPTIONS "-DENABLE_TESTS=0")
endif()
