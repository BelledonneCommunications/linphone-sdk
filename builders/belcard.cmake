############################################################################
# belcard.cmake
# Copyright (C) 2015  Belledonne Communications, Grenoble France
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

set(EP_belcard_GIT_REPOSITORY "git://git.linphone.org/belcard.git" CACHE STRING "belcard repository URL")
set(EP_belcard_GIT_TAG_LATEST "master" CACHE STRING "belcard tag to use when compiling latest version")
set(EP_belcard_EXTERNAL_SOURCE_PATHS "belcard")
set(EP_belcard_GROUPABLE YES)
set(EP_belcard_DEPENDENCIES EP_bctoolbox EP_belr)

if(EP_belcard_FORCE_AUTOTOOLS)
	set(EP_belcard_LINKING_TYPE "--enable-static")
	set(EP_belcard_USE_AUTOGEN True)
else()
	set(EP_belcard_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_LINKING_TYPE})
endif()

# TODO: Activate strict compilation options on IOS
if(IOS)
	list(APPEND EP_belcard_CMAKE_OPTIONS "-DENABLE_STRICT=NO")
endif()

