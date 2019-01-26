/*
	LimeCurveId.java
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

public enum LimeCurveId {
	C25519(1), /**< Curve 25519 */
	C448(2); /**< Curve 448-goldilocks */

	private int native_val; /* Store the native(used by jni) integer value */

	/**
	 * @brief get the native value (used to give input parameter values)
	 * @return the native value associated
	 */
	protected int getNative() {return native_val;}

	private LimeCurveId(int val) {
		native_val = val;
	}
}
