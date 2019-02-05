/*
	LimeCallbackReturn.java
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

/** @brief Enumeration remapping the lime::CallbackReturn
 *  - java enumeration is mapped to an int
 *  - jni code map the same int to the original lime::CallbackReturn
 */
public enum LimeCallbackReturn {
	SUCCESS(0),
	FAIL(1);

	private int native_val; /* Store the native(used by jni) integer value */

	private LimeCallbackReturn(int val) {
		native_val = val;
	}

	/**
	 * @brief static method to get an enum value from the native jni integer(used as returned value by native function)
	 *
	 * @param[in]	val	integer value mapped to the jni enum
	 *
	 * @return one of the enumeration value, unknown input will silently default to FAIL
	 */
	public static LimeCallbackReturn fromNative(int val) {
		switch (val) {
			case 0:
				return LimeCallbackReturn.SUCCESS;
			case 1:
			default:
				return LimeCallbackReturn.FAIL;
		}
	}

}
