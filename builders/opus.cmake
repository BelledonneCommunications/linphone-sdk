############################################################################
# opus.cmake
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

set(EP_opus_URL "http://downloads.xiph.org/releases/opus/opus-1.0.3.tar.gz")
set(EP_opus_URL_HASH "MD5=86eedbd3c5a0171d2437850435e6edff")
set(EP_opus_BUILD_METHOD "autotools")
set(EP_opus_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)
set(EP_opus_CONFIGURE_OPTIONS
	"--disable-doc"
)
set(EP_opus_LINKING_TYPE "--disable-static" "--enable-shared")
