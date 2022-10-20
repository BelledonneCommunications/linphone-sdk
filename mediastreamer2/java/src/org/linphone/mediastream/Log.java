/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
package org.linphone.mediastream;

public final class Log {
	private static Log logger;
	private static Log instance() {
		if (logger == null) {
			logger = new Log();
		}
		return logger;
	}
	
	private Log () {
		
	}
	
	@Deprecated
	public Log(String tag, boolean enable) {
	
	}
	
	private native void d(String msg);
	public static void d(Object...objects) {
		Log.instance().d(toString(objects));
	}
	
	private native void i(String msg);
	public static void i(Object...objects) {
		Log.instance().i(toString(objects));
	}
	
	private native void w(String msg);
	public static void w(Object...objects) {
		Log.instance().w(toString(objects));
	}
	
	private native void e(String msg);
	public static void e(Object...objects) {
		Log.instance().e(toString(objects));
	}

	private static String toString(Object...objects) {
		StringBuilder sb = new StringBuilder();
		for (Object o : objects) {
			sb.append(o);
		}
		return sb.toString();
	}
}

