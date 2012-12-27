/*
AndroidVideoApi8JniWrapper.java
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

import org.linphone.mediastream.Log;

import android.graphics.ImageFormat;
import android.hardware.Camera;
 
public class AndroidVideoApi8JniWrapper {
	static public int detectCameras(int[] indexes, int[] frontFacing, int[] orientation) {
		return AndroidVideoApi5JniWrapper.detectCameras(indexes, frontFacing, orientation);
	}
	
	static public int[] selectNearestResolutionAvailable(int cameraId, int requestedW, int requestedH) {
		return AndroidVideoApi5JniWrapper.selectNearestResolutionAvailable(cameraId, requestedW, requestedH);
	}
	
	public static Object startRecording(int cameraId, int width, int height, int fps, int rotation, final long nativePtr) {
		Log.d("startRecording(" + cameraId + ", " + width + ", " + height + ", " + fps + ", " + rotation + ", " + nativePtr + ")");
		Camera camera = Camera.open(); 

		AndroidVideoApi5JniWrapper.applyCameraParameters(camera, width, height, fps);
		  
		int bufferSize = (width * height * ImageFormat.getBitsPerPixel(camera.getParameters().getPreviewFormat())) / 8;
		camera.addCallbackBuffer(new byte[bufferSize]);
		camera.addCallbackBuffer(new byte[bufferSize]);
		
		camera.setPreviewCallbackWithBuffer(new Camera.PreviewCallback() {
			public void onPreviewFrame(byte[] data, Camera camera) {
				if (AndroidVideoApi5JniWrapper.isRecording) {
					// forward image data to JNI
					AndroidVideoApi5JniWrapper.putImage(nativePtr, data);
					camera.addCallbackBuffer(data);
				}
			}
		});
		 
		camera.startPreview();
		AndroidVideoApi5JniWrapper.isRecording = true;
		Log.d("Returning camera object: " + camera);
		return camera; 
	} 
	
	public static void stopRecording(Object cam) {
		AndroidVideoApi5JniWrapper.isRecording = false;
		Log.d("stopRecording(" + cam + ")"); 
		Camera camera = (Camera) cam;
		 
		if (camera != null) {
			camera.setPreviewCallbackWithBuffer(null);
			camera.stopPreview();
			camera.release(); 
		} else {
			Log.i("Cannot stop recording ('camera' is null)");
		}
	} 
	
	public static void setPreviewDisplaySurface(Object cam, Object surf) {
		AndroidVideoApi5JniWrapper.setPreviewDisplaySurface(cam, surf);
	}
}
