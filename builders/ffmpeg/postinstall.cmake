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

if(EXISTS ${INSTALL_PREFIX}/lib/avcodec-53.def)
	execute_process(COMMAND "lib" "/def:${INSTALL_PREFIX}/lib/avcodec-53.def" "/out:${INSTALL_PREFIX}/lib/avcodec.lib" "/machine:X86")
endif()
if(EXISTS ${INSTALL_PREFIX}/lib/avutil-51.def)
	execute_process(COMMAND "lib" "/def:${INSTALL_PREFIX}/lib/avutil-51.def" "/out:${INSTALL_PREFIX}/lib/avutil.lib" "/machine:X86")
endif()
if(EXISTS ${INSTALL_PREFIX}/lib/swresample-0.def)
	execute_process(COMMAND "lib" "/def:${INSTALL_PREFIX}/lib/swresample-0.def" "/out:${INSTALL_PREFIX}/lib/swresample.lib" "/machine:X86")
endif()
if(EXISTS ${INSTALL_PREFIX}/lib/swscale-2.def)
	execute_process(COMMAND "lib" "/def:${INSTALL_PREFIX}/lib/swscale-2.def" "/out:${INSTALL_PREFIX}/lib/swscale.lib" "/machine:X86")
endif()

if(APPLE AND NOT IOS)
	execute_process(COMMAND install_name_tool
		-id @rpath/libavcodec.53.61.100.dylib
		-change ${INSTALL_PREFIX}/lib/libavutil.51.35.100.dylib @rpath/libavutil.51.35.100.dylib
		${INSTALL_PREFIX}/lib/libavcodec.53.61.100.dylib
	)
	execute_process(COMMAND install_name_tool
		-id @rpath/libavutil.51.35.100.dylib
		${INSTALL_PREFIX}/lib/libavutil.51.35.100.dylib
	)
	execute_process(COMMAND install_name_tool
		-id @rpath/libswresample.0.6.100.dylib
		-change ${INSTALL_PREFIX}/lib/libavutil.51.35.100.dylib @rpath/libavutil.51.35.100.dylib
		${INSTALL_PREFIX}/lib/libswresample.0.6.100.dylib
	)
	execute_process(COMMAND install_name_tool
		-id @rpath/libswscale.2.1.100.dylib
		-change ${INSTALL_PREFIX}/lib/libavutil.51.35.100.dylib @rpath/libavutil.51.35.100.dylib
		${INSTALL_PREFIX}/lib/libswscale.2.1.100.dylib
	)
endif()
