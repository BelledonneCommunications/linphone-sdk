############################################################################
# FindPostQuantumCryptoEngine.cmake
# Copyright (C) 2023  Belledonne Communications, Grenoble France
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
# Find the postquantumcryptoengine library.
#
# Targets
# ^^^^^^^
#
# The following targets may be defined:
#
#  postquantumcryptoengine - If the postquantumcryptoengine library has been found
#
#
# Result variables
# ^^^^^^^^^^^^^^^^
#
# This module will set the following variables in your project:
#
#  PostQuantumCryptoEngine_FOUND - The postquantumcryptoengine library has been found
#  PostQuantumCryptoEngine_TARGET - The name of the CMake target for the postquantumcryptoengine library


if(TARGET postquantumcryptoengine)

	include(FindPackageHandleStandardArgs)
	set(PostQuantumCryptoEngine_TARGET postquantumcryptoengine)
	set(_PostQuantumCryptoEngine_REQUIRED_VARS PostQuantumCryptoEngine_TARGET)
	set(_PostQuantumCryptoEngine_CACHE_VARS ${_PostQuantumCryptoEngine_REQUIRED_VARS})
	find_package_handle_standard_args(PostQuantumCryptoEngine
		REQUIRED_VARS ${_PostQuantumCryptoEngine_REQUIRED_VARS}
	)
	mark_as_advanced(${_PostQuantumCryptoEngine_CACHE_VARS})

else()

	set(_OPTIONS CONFIG)
	if(PostQuantumCryptoEngine_FIND_REQUIRED)
		list(APPEND _OPTIONS REQUIRED)
	endif()
	if(PostQuantumCryptoEngine_FIND_QUIETLY)
		list(APPEND _OPTIONS QUIET)
	endif()
	if(PostQuantumCryptoEngine_FIND_VERSION)
		list(PREPEND _OPTIONS "${PostQuantumCryptoEngine_FIND_VERSION}")
	endif()
	if(PostQuantumCryptoEngine_FIND_EXACT)
		list(APPEND _OPTIONS EXACT)
	endif()

	find_package(PostQuantumCryptoEngine ${_OPTIONS})

endif()
