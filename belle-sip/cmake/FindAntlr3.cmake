############################################################################
# FindAntlr3.txt
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
# - Find the antlr3c include file and library and antlr.jar
#
#  ANTLR3_FOUND - system has antlr3c
#  ANTLR3C_INCLUDE_DIR - the antlr3c include directory
#  ANTLR3C_LIBRARIES - The libraries needed to use antlr3c
#  ANTLR3_COMMAND - The command to run the antlr jar

find_package(Java COMPONENTS Runtime REQUIRED)

set(_ANTLR3C_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
)
set(_ANTLR3_JAR_ROOT_PATHS
	${CMAKE_INSTALL_PREFIX}
	/usr/local
	/usr
	/opt/local
)


find_path(ANTLR3C_INCLUDE_DIRS
	NAMES antlr3.h
	HINTS _ANTLR3C_ROOT_PATHS
	PATH_SUFFIXES include
)
if(ANTLR3C_INCLUDE_DIRS)
	set(HAVE_ANTLR3_H 1)
endif()

find_library(ANTLR3C_LIBRARIES
	NAMES antlr3c
	HINTS _ANTLR3C_ROOT_PATHS
	PATH_SUFFIXES bin lib
)

find_file(ANTLR3_COMMAND
	NAMES antlr3
	HINTS ${_ANTLR3_JAR_ROOT_PATHS}
	PATH_SUFFIXES bin
)

if(NOT ANTLR3_COMMAND)
	# antlr3 command not found, search for the jar file
	find_file(ANTLR3_JAR_PATH
		NAMES antlr3.jar antlr.jar
		HINTS ${_ANTLR3_JAR_ROOT_PATHS}
		PATH_SUFFIXES share/java
	)

	if(ANTLR3_JAR_PATH)
		set(ANTLR3_COMMAND ${Java_JAVA_EXECUTABLE} -Xmx256m -jar ${ANTLR3_JAR_PATH})
	endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Antlr3
	DEFAULT_MSG
	ANTLR3C_INCLUDE_DIRS ANTLR3C_LIBRARIES ANTLR3_COMMAND
)

mark_as_advanced(ANTLR3C_INCLUDE_DIRS ANTLR3C_LIBRARIES ANTLR3_COMMAND)
