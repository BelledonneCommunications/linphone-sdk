/*
LinphoneCallParameters.java
Copyright (C) 2010  Belledonne Communications, Grenoble, France

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
package org.linphone.core;
import org.linphone.core.LinphoneCore.MediaEncryption;
/**
 * The LinphoneCallParams is an object containing various call related parameters.
 * It can be used to retrieve parameters from a currently running call or modify the call's characteristics
 * dynamically.
 * @author Guillaume Beraudo
 *
 */
public interface LinphoneCallParams {
	void setVideoEnabled(boolean b);
	boolean getVideoEnabled();
	
	/**
	 * set audio bandwidth in kbits/s
	 * @param value 0 to disable limitation
	 */
	void setAudioBandwidth(int value);

	/**
	 * return selected media encryption
	 * @return MediaEncryption.None MediaEncryption.SRTP or MediaEncryption.ZRTP
	 */
	MediaEncryption getMediaEncryption();
	/**
	 * set media encryption (rtp) to use
	 * @params menc: MediaEncryption.None, MediaEncryption.SRTP or MediaEncryption.ZRTP
	 */
	void setMediaEnctyption(MediaEncryption menc);

	/**
	 * Get the currently used audio codec
	 * @return PayloadType or null
	 */
	PayloadType getUsedAudioCodec();

	/**
	 * Get the currently used video codec
	 * @return PayloadType or null
	 */
	PayloadType getUsedVideoCodec();
}
