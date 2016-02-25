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

set(EP_ortp_GIT_REPOSITORY "git://git.linphone.org/ortp.git" CACHE STRING "ortp repository URL")
set(EP_ortp_GIT_TAG_LATEST "master" CACHE STRING "ortp tag to use when compiling latest version")
set(EP_ortp_GIT_TAG "0.25.0" CACHE STRING "ortp tag to use")
set(EP_ortp_EXTERNAL_SOURCE_PATHS "oRTP" "ortp" "linphone/oRTP")
set(EP_ortp_GROUPABLE YES)

if(EP_ortp_FORCE_AUTOTOOLS)
	set(EP_ortp_LINKING_TYPE "--enable-static")
	set(EP_ortp_USE_AUTOGEN True)
else()
	set(EP_ortp_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_LINKING_TYPE})
endif()
if(MSVC)
	set(EP_ortp_EXTRA_LDFLAGS "/SAFESEH:NO")
endif()
set(EP_ortp_SPEC_FILE "ortp.spec")

set(EP_ortp_CMAKE_OPTIONS "-DENABLE_DOC=${ENABLE_DOC}")

# TODO: Activate strict compilation options on IOS
if(IOS)
	list(APPEND EP_ortp_CMAKE_OPTIONS "-DENABLE_STRICT=NO")
endif()

