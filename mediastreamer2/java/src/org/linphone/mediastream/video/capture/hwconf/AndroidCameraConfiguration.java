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

import java.util.ArrayList;
import java.util.List;

import org.linphone.mediastream.Log;
import org.linphone.mediastream.Version;


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
		// cache already filled and valid ?
		if (camerasCache != null && camerasCache.length!=0)
			return;
		
		try {
			if (Version.sdk() < 9) {
				camerasCache = AndroidCameraConfiguration.probeCamerasSDK5();
			} else {
				if (Version.sdk() >= 21) {
					camerasCache = AndroidCameraConfiguration.probeCamerasSDK21();
				} else {
					camerasCache = AndroidCameraConfiguration.probeCamerasSDK9();
				}
			}
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

	static AndroidCamera[] probeCamerasSDK21() {
		return AndroidCameraConfigurationReader21.probeCameras();
	}

	/**
	 * Default: no front; rear=0; default=rear
	 * @author Guillaume Beraudo
	 *
	 */
	static public  class AndroidCamera {
		static public class Size{
			public final int width;
			public final int height;

			public Size(int w, int h){
				this.width = w;
				this.height = h;
			}
		}

		public AndroidCamera(int i, boolean f, List<Size> r, int o) {
			this.id = i;
			this.frontFacing = f;
			this.orientation = o;
			this.resolutions = r;
		}
		public AndroidCamera(int i, boolean f, int o, List<android.hardware.Camera.Size> origin) {
			this.resolutions = new ArrayList<Size>(origin.size());
			for (android.hardware.Camera.Size s : origin) {
				this.resolutions.add(new AndroidCamera.Size(s.width,s.height));
			}
			this.id = i;
			this.frontFacing = f;
			this.orientation = o;
		}
		public int id;
		public boolean frontFacing; // false => rear facing
		public int orientation;
		public List<Size> resolutions;

	}
}