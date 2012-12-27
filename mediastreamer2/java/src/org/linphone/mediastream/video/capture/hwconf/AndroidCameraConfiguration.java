/*
AndroidCameraConf.java
Copyright (C) 2011  Belledonne Communications, Grenoble, France

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
package org.linphone.mediastream.video.capture.hwconf;

import java.util.List;

import org.linphone.mediastream.Log;
import org.linphone.mediastream.Version;

import android.hardware.Camera.Size;



/**
 * Entry point for accessing hardware cameras configuration.
 * Different SDK implementations are delegated to specific AndroidCameraConfN versions.
 *
 */
public class AndroidCameraConfiguration {
	public static AndroidCamera[] retrieveCameras() {
		initCamerasCache();
		return camerasCache;
	}
	
	public static boolean hasSeveralCameras() {
		initCamerasCache();
		return camerasCache.length > 1;
	}

	public static boolean hasFrontCamera() {
		initCamerasCache();
		for (AndroidCamera cam:  camerasCache) {
			if (cam.frontFacing) 
				return true;
		}
		return false;
	}
	
	private static AndroidCameraConfiguration.AndroidCamera[] camerasCache;
	
	private static void initCamerasCache() {
		// cache already filled ?
		if (camerasCache != null)
			return;
		
		try {
			if (Version.sdk() < 9)
				camerasCache = AndroidCameraConfiguration.probeCamerasSDK5();
			else
				camerasCache = AndroidCameraConfiguration.probeCamerasSDK9();
		} catch (Exception exc) {
			Log.e("Error: cannot retrieve cameras information (busy ?)", exc);
			exc.printStackTrace();
			camerasCache = new AndroidCamera[0];
		}
	}
	
	static AndroidCamera[] probeCamerasSDK5() {
		return AndroidCameraConfigurationReader5.probeCameras();
	}
	
	static  AndroidCamera[] probeCamerasSDK9() {
		return AndroidCameraConfigurationReader9.probeCameras();
	}

	/**
	 * Default: no front; rear=0; default=rear
	 * @author Guillaume Beraudo
	 *
	 */
	static public  class AndroidCamera {
		public AndroidCamera(int i, boolean f, int o, List<Size> r) {
			this.id = i;
			this.frontFacing = f;
			this.orientation = o;
			this.resolutions = r;
		}
		public int id;
		public boolean frontFacing; // false => rear facing
		public int orientation;
		public List<Size> resolutions;
	}
}