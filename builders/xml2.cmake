############################################################################
# xml2.cmake
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

set(EP_xml2_GIT_REPOSITORY "git://git.gnome.org/libxml2")
set(EP_xml2_GIT_TAG "v2.8.0")
set(EP_xml2_BUILD_METHOD "autotools")
set(EP_xml2_USE_AUTOGEN "yes")
set(EP_xml2_CONFIGURE_OPTIONS_PASSED_TO_AUTOGEN "yes")
set(EP_xml2_CROSS_COMPILATION_OPTIONS
	"--prefix=${CMAKE_INSTALL_PREFIX}"
	"--host=${LINPHONE_BUILDER_HOST}"
)
set(EP_xml2_CONFIGURE_OPTIONS
	"--with-minimum"
	"--with-xpath"
	"--with-tree"
	"--with-schemas"
	"--with-reader"
	"--with-writer"
	"--with-sax1"
	"--enable-rebuild-docs=no"
)
set(EP_xml2_LINKING_TYPE "--disable-static" "--enable-shared")

if(WIN32)
	set(EP_xml2_EXTRA_LDFLAGS "-static-libgcc")
endif(WIN32)
