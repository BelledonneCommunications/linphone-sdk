############################################################################
# speex.cmake
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

set(EP_speex_GIT_REPOSITORY "git://git.linphone.org/speex.git" CACHE STRING "speex repository URL")
set(EP_speex_GIT_TAG_LATEST "linphone" CACHE STRING "speex tag to use when compiling latest version")
set(EP_speex_GIT_TAG "fc1dd43c3c9d244bca1c300e408ce0373dbd5ed8" CACHE STRING "speex tag to use")
set(EP_speex_EXTERNAL_SOURCE_PATHS "speex" "externals/speex")
set(EP_speex_MAY_BE_FOUND_ON_SYSTEM TRUE)
set(EP_speex_IGNORE_WARNINGS TRUE)

set(EP_speex_LINKING_TYPE ${DEFAULT_VALUE_CMAKE_LINKING_TYPE})
set(EP_speex_CMAKE_OPTIONS "-DENABLE_SPEEX_DSP=YES" "-DENABLE_SPEEX_CODEC=${ENABLE_SPEEX}")
