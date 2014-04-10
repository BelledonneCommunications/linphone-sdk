############################################################################
# bzrtp.cmake
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

set(EP_bzrtp_GIT_REPOSITORY "git://git.linphone.org/bzrtp.git")
if(${LINPHONE_BUILDER_LATEST})
	set(EP_bzrtp_GIT_TAG "master")
else()
	set(EP_bzrtp_GIT_TAG "1983008781bba8c001795948c1462be2670f45e0")
endif()
set(EP_bzrtp_BUILD_METHOD "autotools")
set(EP_bzrtp_USE_AUTOGEN "yes")
set(EP_bzrtp_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)
set(EP_bzrtp_LINKING_TYPE "--disable-static" "--enable-shared")
set(EP_bzrtp_DEPENDENCIES EP_polarssl EP_xml2)

if(${ENABLE_UNIT_TESTS})
	list(APPEND EP_bzrtp_DEPENDENCIES EP_cunit)
else()
	list(APPEND EP_bzrtp_CONFIGURE_OPTIONS "--disable-tests")
endif()
