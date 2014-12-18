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

set(EP_libilbcrfc3951_GIT_REPOSITORY "git://git.linphone.org/libilbc-rfc3951.git")
set(EP_libilbcrfc3951_GIT_TAG_LATEST "master")
set(EP_libilbcrfc3951_GIT_TAG "488f2159378d2c1956135604736dfddaeb947ef5")

set(EP_libilbcrfc3951_BUILD_METHOD "autotools")
set(EP_libilbcrfc3951_USE_AUTOGEN "yes")
set(EP_libilbcrfc3951_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)
set(EP_libilbcrfc3951_LINKING_TYPE "--disable-static" "--enable-shared")
