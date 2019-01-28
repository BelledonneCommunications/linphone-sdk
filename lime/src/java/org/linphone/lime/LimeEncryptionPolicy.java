/*
	LimeEncryptionPolicy.java
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

public enum LimeEncryptionPolicy {
	DRMESSAGE(0), /**< the plaintext input is encrypted inside the Double Ratchet message (each recipient get a different encryption): not optimal for messages with numerous recipient */
	CIPHERMESSAGE(1), /**< the plaintext input is encrypted with a random key and this random key is encrypted to each participant inside the Double Ratchet message(for a single recipient the overhead is 48 bytes) */
	OPTIMIZEUPLOADSIZE(2), /**< optimize upload size: encrypt in DR message if plaintext is short enougth to beat the overhead introduced by cipher message scheme, otherwise use cipher message. Selection is made on upload size only. This is the default policy used */
	OPTIMIZEGLOBALBANDWIDTH(3); /**< optimize bandwith usage: encrypt in DR message if plaintext is short enougth to beat the overhead introduced by cipher message scheme, otherwise use cipher message. Selection is made on uploadand download (from server to recipients) sizes added. */

	private int native_val; /* Store the native(used by jni) integer value */

	/**
	 * @brief get the native value (used to give input parameter values)
	 * @return the native value associated
	 */
	protected int getNative() {return native_val;}

	private LimeEncryptionPolicy(int val) {
		native_val = val;
	}
}

