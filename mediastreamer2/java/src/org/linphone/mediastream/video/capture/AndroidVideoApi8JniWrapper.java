package org.linphone.mediastream.video.capture;

import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.util.Log;
 
public class AndroidVideoApi8JniWrapper {
	static public int detectCameras(int[] indexes, int[] frontFacing, int[] orientation) {
		return AndroidVideoApi5JniWrapper.detectCameras(indexes, frontFacing, orientation);
	}
	
	static public int[] selectNearestResolutionAvailable(int cameraId, int requestedW, int requestedH) {
		return AndroidVideoApi5JniWrapper.selectNearestResolutionAvailable(cameraId, requestedW, requestedH);
	}
	
	public static Object startRecording(int cameraId, int width, int height, int fps, int rotation, final long nativePtr) {
		Log.d("mediastreamer", "startRecording(" + cameraId + ", " + width + ", " + height + ", " + fps + ", " + rotation + ", " + nativePtr + ")");
		Camera camera = Camera.open(); 

		AndroidVideoApi5JniWrapper.applyCameraParameters(camera, width, height, fps);
		  
		int bufferSize = (width * height * ImageFormat.getBitsPerPixel(camera.getParameters().getPreviewFormat())) / 8;
		camera.addCallbackBuffer(new byte[bufferSize]);
		camera.addCallbackBuffer(new byte[bufferSize]);
		
		camera.setPreviewCallbackWithBuffer(new Camera.PreviewCallback() {
			@Override
			public void onPreviewFrame(byte[] data, Camera camera) {
				// forward image data to JNI
				AndroidVideoApi5JniWrapper.putImage(nativePtr, data);
				
				camera.addCallbackBuffer(data);
			}
		});
		 
		camera.startPreview();
		Log.d("mediastreamer", "Returning camera object: " + camera);
		return camera; 
	} 
	
	public static void stopRecording(Object cam) {
		Log.d("mediastreamer", "stopRecording(" + cam + ")"); 
		Camera camera = (Camera) cam;
		 
		if (camera != null) {
			camera.setPreviewCallbackWithBuffer(null);
			camera.stopPreview();
			camera.release(); 
		} else {
			Log.i("mediastreamer", "Cannot stop recording ('camera' is null)");
		}
	} 
	
	public static void setPreviewDisplaySurface(Object cam, Object surf) {
		AndroidVideoApi5JniWrapper.setPreviewDisplaySurface(cam, surf);
	}
}
