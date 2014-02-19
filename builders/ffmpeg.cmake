############################################################################
# ffmpeg.cmake
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

set(EP_ffmpeg_URL "http://ffmpeg.org/releases/ffmpeg-0.10.2.tar.gz")
set(EP_ffmpeg_AUTOTOOLS "yes")
# Disable video acceleration support for compatibility with older Mac OS X versions (vda, vaapi, vdpau).
set(EP_ffmpeg_CONFIGURE_OPTIONS "--enable-pic --disable-zlib --disable-bzlib --disable-mmx --disable-ffplay --disable-ffprobe --disable-ffserver --disable-avdevice --disable-avfilter --disable-network --disable-avformat --disable-everything --enable-decoder=mjpeg --enable-encoder=mjpeg --disable-vda --disable-vaapi --disable-vdpau")
set(EP_ffmpeg_ARCH "i386")

if(APPLE)
	set(EP_ffmpeg_TARGET_OS "darwin")
else(APPLE)
	set(EP_ffmpeg_TARGET_OS "linux")
endif(APPLE)
