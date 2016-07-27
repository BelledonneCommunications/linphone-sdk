############################################################################
# FindPCAP.txt
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
#
# - Find the pcap include file and library
#
#  PCAP_FOUND - system has pcap
#  PCAP_INCLUDE_DIRS - the pcap include directory
#  PCAP_LIBRARIES - The libraries needed to use pcap

set(_PCAP_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)

find_path(PCAP_INCLUDE_DIRS
	NAMES pcap/pcap.h pcap.h
	HINTS _PCAP_ROOT_PATHS
	PATH_SUFFIXES include
)
if(PCAP_INCLUDE_DIRS)
	set(HAVE_PCAP_PCAP_H 1)
endif()

find_library(PCAP_LIBRARIES
	NAMES pcap
	HINTS _PCAP_ROOT_PATHS
	PATH_SUFFIXES lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PCAP
	DEFAULT_MSG
	PCAP_INCLUDE_DIRS PCAP_LIBRARIES
)

mark_as_advanced(PCAP_INCLUDE_DIRS PCAP_LIBRARIES)
