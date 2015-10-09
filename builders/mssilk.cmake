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

set(EP_mssilk_GIT_REPOSITORY "git://git.linphone.org/mssilk.git" CACHE STRING "mssilk repository URL")
set(EP_mssilk_GIT_TAG_LATEST "master" CACHE STRING "mssilk tag to use when compiling latest version")
set(EP_mssilk_GIT_TAG "60c53a1542f3c5cb93342c7fb16e00ce075dd8f7" CACHE STRING "mssilk tag to use")
set(EP_mssilk_EXTERNAL_SOURCE_PATHS "mssilk")
set(EP_mssilk_GROUPABLE YES)

set(EP_mssilk_LINKING_TYPE "${DEFAULT_VALUE_CMAKE_LINKING_TYPE}")
set(EP_mssilk_DEPENDENCIES EP_ms2)
