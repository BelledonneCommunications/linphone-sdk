/*
	RecipientData.java
	@author Johan Pascal
	@copyright	Copyright (C) 2019  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
package org.linphone.lime;

/** @brief The encrypt function input/output data structure
 *
 * give a recipient GRUU and get it back with the header which must be sent to recipient with the cipher text
 */
public class RecipientData {
	public String deviceId; /**< The recipient device id(shall be its GRUU)*/
	private int peerStatus; /**< peer Status is stored in native enumeration, so native code can access it easily, it is thus access through methods translating to LimePeerDeviceStatus java enum\n
					input: if set to FAIL, this entry will be ignored by the encrypt function\n
					output: after encrypt calls back, it will hold the status of this peer device:\n
						     - UNKNOWN: first interaction with this device)
						     - UNTRUSTED: device is kown but we never confirmed its identity public key
						     - TRUSTED: we already confirmed this device identity public key
						     - FAIL: we could not encrypt for this device, probably because it never published its keys on the X3DH server */
	public byte[] DRmessage; /**< after encrypt calls back, it will hold the Double Ratchet message targeted to the specified recipient. */

	public LimePeerDeviceStatus getPeerStatus() {
		return LimePeerDeviceStatus.fromNative(peerStatus);
	}

	public void setPeerStatus(LimePeerDeviceStatus status) {
		peerStatus = status.getNative();
	}

	public RecipientData(String p_deviceId) {
		deviceId = p_deviceId;
		peerStatus = LimePeerDeviceStatus.UNKNOWN.getNative(); // default to unknown
	}

	public RecipientData(String p_deviceId, LimePeerDeviceStatus status) {
		deviceId = p_deviceId;
		peerStatus = status.getNative();
	}
}
