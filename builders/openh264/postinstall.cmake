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
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
############################################################################

if(EXISTS ${INSTALL_PREFIX}/lib/libopenh264.a)
	execute_process(COMMAND "ranlib" "${INSTALL_PREFIX}/lib/libopenh264.a")
endif()
if(EXISTS ${INSTALL_PREFIX}/bin/openh264.dll)
	execute_process(COMMAND "${PYTHON_EXECUTABLE}" "${SOURCE_DIR}/cmake/importlib.py" "${INSTALL_PREFIX}/bin/openh264.dll" "${INSTALL_PREFIX}/lib/openh264.lib")
endif()

if(APPLE AND NOT IOS)
	execute_process(COMMAND install_name_tool -id @rpath/libopenh264.0.dylib ${INSTALL_PREFIX}/lib/libopenh264.0.dylib)
endif()

