############################################################################
# postinstall.cmake
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

if(EXISTS ${INSTALL_PREFIX}/bin/openh264.dll AND NOT EXISTS ${INSTALL_PREFIX}/lib/openh264_dll.lib)
	execute_process(COMMAND "${PYTHON_EXECUTABLE}" "${SOURCE_DIR}/cmake/importlib.py" "${INSTALL_PREFIX}/bin/openh264.dll" "${INSTALL_PREFIX}/lib/openh264.lib")
endif()

if(EXISTS ${INSTALL_PREFIX}/lib/libopenh264.1.dylib)
	execute_process(COMMAND install_name_tool -id @rpath/libopenh264.1.dylib ${INSTALL_PREFIX}/lib/libopenh264.1.dylib)
endif()

file(GLOB OPENH264_LIBS "${INSTALL_PREFIX}/lib/libopenh264.*.dylib")
foreach(OPENH264_LIB IN LISTS ${OPENH264_LIBS})
	string(REGEX REPLACE ".+/(libopenh264\..\.dylib)$"
		"@rpath/\1"
		OPENH264_LIB_ID
		${OPENH264_LIB}
	)
	execute_process(COMMAND install_name_tool -id ${OPENH264_LIB_ID} ${OPENH264_LIB})
endforeach()

if(EXISTS ${INSTALL_PREFIX}/lib/libopenh264.a AND TOOLCHAIN_RANLIB)
	execute_process(COMMAND "${TOOLCHAIN_RANLIB}" "${INSTALL_PREFIX}/lib/libopenh264.a")
endif()
