/*
	LimePostToX3DH.java
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

/** @brief Define an interface to communicate with the X3DH server
 */
public interface LimePostToX3DH {
	/**
	 * @brief Function called by native code to post a message to the X3DH server
	 *
	 * @param[in]	ptr 	a native object pointer (stored in java long), must be returned along the server response to process it
	 * @param[in]	url	the X3DH server's URL
	 * @param[in]	from	shall be included in the from field of HTTPS packet sent to the server(holds the local device Id of message sender)
	 * @param[in]	message	the binary content of the message to be sent
	 *
	 * @note: To forward the server's response, call the LimeManager.process_response static method giving back the native object pointer
	 */
	public void postToX3DHServer(long ptr, String url, String from, byte[] message);
}
