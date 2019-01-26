/*
	LimePeerDeviceStatus.java
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

/** @brief Enumeration remapping the lime::PeerDeviceStatus
 *  - java enumeration is mapped to an int
 *  - jni code map the same int to the original lime::PeerDeviceStatus
 */
public enum LimePeerDeviceStatus {
	UNTRUSTED(0), /**< we know this device but do not trust it, that information shall be displayed to the end user, a colour code shall be enough */
	TRUSTED(1), /**< this peer device already got its public identity key validated, that information shall be displayed to the end user too */
	UNSAFE(2), /**< this status is a helper for the library user. It is used only by the peerDeviceStatus accessor functions */
	FAIL(3), /**< when returned by decrypt : we could not decrypt the incoming message\n
			when returned by encrypt in the peerStatus: we could not encrypt to this recipient(probably because it does not published keys on the X3DH server) */
	UNKNOWN(4); /**< when returned after encryption or decryption, means it is the first time we communicate with this device (and thus create a DR session with it)\n
			when returned by a get_peerDeviceStatus: this device is not in localStorage */


	private int native_val; /* Store the native(used by jni) integer value */

	/**
	 * @brief get the native value (used to give input parameter values)
	 * @return the native value associated
	 */
	protected int getNative() {return native_val;}

	/**
	 * @brief static method to get an enum value from the native jni integer(used as returned value by native function)
	 *
	 * @param[in]	val	integer value mapped to the jni enum
	 *
	 * @return one of the enumeration value, unknown input will silently default to UNKNOWN
	 */
	protected static LimePeerDeviceStatus fromNative(int val) {
		switch (val) {
			case 0:
				return LimePeerDeviceStatus.UNTRUSTED;
			case 1:
				return LimePeerDeviceStatus.TRUSTED;
			case 2:
				return LimePeerDeviceStatus.UNSAFE;
			case 3:
				return LimePeerDeviceStatus.FAIL;
			case 4:
			default:
				return LimePeerDeviceStatus.UNKNOWN;
		}
	}

	LimePeerDeviceStatus(int val) {
		native_val = val;
	}
}
