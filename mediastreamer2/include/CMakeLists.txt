############################################################################
# CMakeLists.txt
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

set(HEADER_FILES
	mediastreamer2/allfilters.h
	mediastreamer2/bitratecontrol.h
	mediastreamer2/bits_rw.h
	mediastreamer2/dsptools.h
	mediastreamer2/dtls_srtp.h
	mediastreamer2/dtmfgen.h
	mediastreamer2/flowcontrol.h
	mediastreamer2/formats.h
	mediastreamer2/ice.h
	mediastreamer2/mediastream.h
	mediastreamer2/ms_srtp.h
	mediastreamer2/msaudiomixer.h
	mediastreamer2/mschanadapter.h
	mediastreamer2/mscodecutils.h
	mediastreamer2/mscommon.h
	mediastreamer2/msconference.h
	mediastreamer2/msequalizer.h
	mediastreamer2/mseventqueue.h
	mediastreamer2/msextdisplay.h
	mediastreamer2/msfactory.h
	mediastreamer2/msfileplayer.h
	mediastreamer2/msfilerec.h
	mediastreamer2/msfilter.h
	mediastreamer2/msgenericplc.h
	mediastreamer2/msinterfaces.h
	mediastreamer2/msitc.h
	mediastreamer2/msjava.h
	mediastreamer2/msjpegwriter.h
	mediastreamer2/msmediaplayer.h
	mediastreamer2/msqueue.h
	mediastreamer2/msrtp.h
	mediastreamer2/mssndcard.h
	mediastreamer2/mstee.h
	mediastreamer2/msticker.h
	mediastreamer2/mstonedetector.h
	mediastreamer2/msutils.h
	mediastreamer2/msv4l.h
	mediastreamer2/msvaddtx.h
	mediastreamer2/msvideo.h
	mediastreamer2/msvideoout.h
	mediastreamer2/msvideopresets.h
	mediastreamer2/msvolume.h
	mediastreamer2/mswebcam.h
	mediastreamer2/qualityindicator.h
	mediastreamer2/rfc3984.h
	mediastreamer2/stun.h
	mediastreamer2/stun_udp.h
	mediastreamer2/upnp_igd.h
	mediastreamer2/videostarter.h
	mediastreamer2/x11_helper.h
	mediastreamer2/zrtp.h
)

install(FILES ${HEADER_FILES}
        DESTINATION include/mediastreamer2
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
