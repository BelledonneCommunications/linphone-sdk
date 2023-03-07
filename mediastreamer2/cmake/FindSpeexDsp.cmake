############################################################################
# FindSpeexDsp.txt
# Copyright (C) 2014-2023  Belledonne Communications, Grenoble France
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
# - Find the speexdsp include file and library
#
#  SPEEXDSP_FOUND - system has speexdsp
#  SPEEXDSP_INCLUDE_DIRS - the speexdsp include directory
#  SPEEXDSP_LIBRARIES - The libraries needed to use speexdsp

if(TARGET speexdsp)

	set(SPEEXDSP_LIBRARIES speexdsp)
	get_target_property(SPEEXDSP_INCLUDE_DIRS speexdsp INTERFACE_INCLUDE_DIRECTORIES)
	set(HAVE_SPEEX_SPEEX_RESAMPLER_H 1)

else()

	find_path(SPEEXDSP_INCLUDE_DIRS
		NAMES speex/speex_resampler.h
		PATH_SUFFIXES include
	)
	if(SPEEXDSP_INCLUDE_DIRS)
		set(HAVE_SPEEX_SPEEX_RESAMPLER_H 1)
	endif()

	find_library(SPEEXDSP_LIBRARIES
		NAMES speexdsp
	)

endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SpeexDsp
	DEFAULT_MSG
	SPEEXDSP_INCLUDE_DIRS SPEEXDSP_LIBRARIES HAVE_SPEEX_SPEEX_RESAMPLER_H
)

mark_as_advanced(SPEEXDSP_INCLUDE_DIRS SPEEXDSP_LIBRARIES HAVE_SPEEX_SPEEX_RESAMPLER_H)
