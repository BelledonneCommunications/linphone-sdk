/*
AndroidCameraRecordManager.java
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
package org.linphone.mediastream.video.capture;

import org.linphone.LinphoneManager;
import org.linphone.core.Log;
import org.linphone.mediastream.video.capture.hwconf.AndroidCameraConfiguration;
import org.linphone.mediastream.video.capture.hwconf.AndroidCameraConfiguration.AndroidCamera;

/**
 * Provide simple API to the applications to manage camera selection
 *
 * @author Guillaume Beraudo
 *
 */
public class AndroidCameraPreferences {
	// current camera
	public static int cameraId = 0;
	// send video ?
	public static boolean muted = false;
	
	
	public static void setUseFrontCamera(boolean value) {
		AndroidCamera[] cameras = AndroidCameraConfiguration.retrieveCameras();
		for (AndroidCamera cam: cameras) {
			if (cam.frontFacing == value) {
				if (cameraId != cam.id) {  
					cameraId = cam.id;
					
					if (LinphoneManager.getLc().getCurrentCall() != null) {
						Log.w("Should we restart video ??");
					}
				}
				return;
			}
		}
		Log.e("setUseFrontCamera(true) while no front camera detected on device: using rear");
	}
	
	public static void useNextCamera() {
		AndroidCamera[] cameras = AndroidCameraConfiguration.retrieveCameras();
		int newid = (cameraId + 1) % cameras.length;
		if (newid != cameraId) {
			Log.d("Change camera to use : ", cameraId, " -> ", newid);
			cameraId = newid;
		}
	}
}
