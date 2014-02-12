############################################################################
# config-desktop.cmake
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

set(AUTOTOOLS_SHARED_LIBRARIES "--disable-static --enable-shared")
set(CMAKE_SHARED_LIBRARIES "-DENABLE_STATIC=0")

# cunit
set(EP_cunit_CMAKE_OPTIONS "${CMAKE_SHARED_LIBRARIES} ${EP_cunit_CMAKE_OPTIONS} -DENABLE_CURSES=1")

# xml2
set(EP_xml2_CONFIGURE_OPTIONS "${AUTOTOOLS_SHARED_LIBRARIES} ${EP_xml2_CONFIGURE_OPTIONS}")

# antlr3
set(EP_antlr3c_CMAKE_OPTIONS "${CMAKE_SHARED_LIBRARIES} ${EP_antlr3c_CMAKE_OPTIONS}")

# polarssl
set(EP_polarssl_CMAKE_OPTIONS "-DUSE_SHARED_POLARSSL_LIBRARY=1 ${EP_polarssl_CMAKE_OPTIONS}")

# bellesip
set(EP_bellesip_CMAKE_OPTIONS "${CMAKE_SHARED_LIBRARIES} ${EP_bellesip_CMAKE_OPTIONS}")

# srtp
set(EP_srtp_CMAKE_OPTIONS "${CMAKE_SHARED_LIBRARIES} ${EP_srtp_CMAKE_OPTIONS}")

# speex
set(EP_speex_CMAKE_OPTIONS "${CMAKE_SHARED_LIBRARIES} ${EP_speex_CMAKE_OPTIONS}")

# opus
set(EP_opus_CONFIGURE_OPTIONS "${AUTOTOOLS_SHARED_LIBRARIES} ${EP_opus_CONFIGURE_OPTIONS}")

# oRTP
set(EP_ortp_CONFIGURE_OPTIONS "${AUTOTOOLS_SHARED_LIBRARIES} ${EP_ortp_CONFIGURE_OPTIONS}")

# mediastreamer2
set(EP_ms2_CONFIGURE_OPTIONS "${AUTOTOOLS_SHARED_LIBRARIES} ${EP_ms2_CONFIGURE_OPTIONS}")

# linphone
set(EP_linphone_CONFIGURE_OPTIONS "${AUTOTOOLS_SHARED_LIBRARIES} ${EP_linphone_CONFIGURE_OPTIONS}")
