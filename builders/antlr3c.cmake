############################################################################
# antlr3c.cmake
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
############################################################################

set(EP_antlr3c_GIT_REPOSITORY "git://git.linphone.org/antlr3.git" CACHE STRING "antlr3c repository URL")
set(EP_antlr3c_GIT_TAG_LATEST "linphone" CACHE STRING "antlr3c tag to use when compiling latest version")
set(EP_antlr3c_GIT_TAG "52075ffb35975c6901e924b4a763b6fb23abd623" CACHE STRING "antlr3c tag to use")
set(EP_antlr3c_EXTERNAL_SOURCE_PATHS "antlr3c" "antlr3" "externals/antlr3")
set(EP_antlr3c_MAY_BE_FOUND_ON_SYSTEM TRUE)
set(EP_antlr3c_IGNORE_WARNINGS TRUE)

set(EP_antlr3c_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_LINKING_TYPE})

set(EP_antlr3c_CMAKE_OPTIONS "-DENABLE_DEBUGGER=NO")
