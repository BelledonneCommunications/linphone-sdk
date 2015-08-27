############################################################################
# msamr.cmake
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

set(EP_msamr_GIT_REPOSITORY "git://git.linphone.org/msamr.git")
set(EP_msamr_GIT_TAG_LATEST "master")
set(EP_msamr_GIT_TAG "076adbf96b8c369eb82817802819444fdc119139")
set(EP_msamr_EXTERNAL_SOURCE_PATHS "msamr")

set(EP_msamr_LINKING_TYPE "${DEFAULT_VALUE_CMAKE_LINKING_TYPE}")
set(EP_msamr_DEPENDENCIES EP_ms2 EP_opencoreamr)

set(EP_msamr_CMAKE_OPTIONS "-DENABLE_NARROWBAND=${ENABLE_AMRNB}")
list(APPEND EP_msamr_CMAKE_OPTIONS "-DENABLE_WIDEBAND=${ENABLE_AMRWB}")
if(ENABLE_AMRWB)
	list(APPEND EP_msamr_DEPENDENCIES EP_voamrwbenc)
endif()
