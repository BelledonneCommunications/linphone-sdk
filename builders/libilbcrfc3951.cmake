############################################################################
# libilbcrfc3951.cmake
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

set(EP_libilbcrfc3951_GIT_REPOSITORY "git://git.linphone.org/libilbc-rfc3951.git" CACHE STRING "libilbcrfc3951 repository URL")
set(EP_libilbcrfc3951_GIT_TAG_LATEST "master" CACHE STRING "libilbcrfc3951 tag to use when compiling latest version")
set(EP_libilbcrfc3951_GIT_TAG "91b61e39fb9c5d3dc78691f3d6e4f1d65c8b0d2a" CACHE STRING "libilbcrfc3951 tag to use")
set(EP_libilbcrfc3951_EXTERNAL_SOURCE_PATHS "libilbc-rfc3951")

set(EP_libilbcrfc3951_LINKING_TYPE "-DENABLE_STATIC=YES")
