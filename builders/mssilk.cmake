############################################################################
# mssilk.cmake
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

set(EP_mssilk_GIT_REPOSITORY "git://git.linphone.org/mssilk.git")
if(${LINPHONE_BUILDER_LATEST})
	set(EP_mssilk_GIT_TAG "master")
else()
	set(EP_mssilk_GIT_TAG "07b55ce81adca894bf6bd1ed86eb27f4578562f1")
endif()
set(EP_mssilk_BUILD_METHOD "autotools")
set(EP_mssilk_USE_AUTOGEN "yes")
set(EP_mssilk_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)
set(EP_mssilk_LINKING_TYPE "--disable-static" "--enable-shared")
set(EP_mssilk_DEPENDENCIES EP_ms2)

