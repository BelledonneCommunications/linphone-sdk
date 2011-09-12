package org.linphone.mediastream.video.capture;

import java.util.List;

import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.hardware.Camera.Parameters;
import android.util.Log;
 
public class AndroidVideoApi9JniWrapper {	
	static public int detectCameras(int[] indexes, int[] frontFacing, int[] orientation) {
		Log.d("mediastreamer", "detectCameras\n");
		/* SDK >= 9 */
		int count = Camera.getNumberOfCameras();
		if (count > indexes.length) {
			Log.w("mediastreamer", "Returning only the " + indexes.length + " first cameras (increase buffer size to retrieve all)");
			count = indexes.length;
		}
		
		CameraInfo cameraInfo = new CameraInfo();
		for(int i=0; i<count; i++) {
			Camera.getCameraInfo(i, cameraInfo);
			
			indexes[i] = i;
			frontFacing[i] = (cameraInfo.facing == CameraInfo.CAMERA_FACING_FRONT)?1:0;
			orientation[i] = cameraInfo.orientation;
		}
		
		return count;
	}
	
	/**
	 * Return the hw-available available resolution best matching the requested one.
	 * Best matching meaning :
	 * - try to find the same one
	 * - try to find one just a little bigger (ex: CIF when asked QVGA)
	 * - as a fallback the nearest smaller one
	 * @param requestedW Requested video size width
	 * @param requestedH Requested video size height
	 * @return int[width, height] of the chosen resolution, may be null if no
	 * resolution can possibly match the requested one
	 */
	static public int[] selectNearestResolutionAvailable(int cameraId, int requestedW, int requestedH) {
		Log.d("mediastreamer", "selectNearestResolutionAvailable: " + cameraId + ", " + requestedW + "x" + requestedH);
		return AndroidVideoApi5JniWrapper.selectNearestResolutionAvailableForCamera(Camera.open(cameraId), requestedW, requestedH);
	}
	
	public static Object startRecording(int cameraId, int width, int height, int fps, int rotation, final long nativePtr) {
		Log.d("mediastreamer", "startRecording(" + cameraId + ", " + width + ", " + height + ", " + fps + ", " + rotation + ", " + nativePtr + ")");
		Camera camera = Camera.open(cameraId); 
		
		Parameters params = camera.getParameters();
		 
		params.setPreviewSize(width, height); 
		
		int[] chosenFps = findClosestEnclosingFpsRange(fps*1000, params.getSupportedPreviewFpsRange());
		params.setPreviewFpsRange(chosenFps[0], chosenFps[1]);
		
		camera.setParameters(params);
		  
		int bufferSize = (width * height * ImageFormat.getBitsPerPixel(params.getPreviewFormat())) / 8;
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
		
		setCameraDisplayOrientation(rotation, cameraId, camera);
		 
		camera.startPreview();
		Log.d("mediastreamer", "Returning camera object: " + camera);
		return camera; 
	} 
	
	public static void stopRecording(Object cam) {
		AndroidVideoApi8JniWrapper.stopRecording(cam);
	} 
	
	public static void setPreviewDisplaySurface(Object cam, Object surf) {
		AndroidVideoApi5JniWrapper.setPreviewDisplaySurface(cam, surf);
	}
	
	private static void setCameraDisplayOrientation(int rotationDegrees, int cameraId, Camera camera) {
		android.hardware.Camera.CameraInfo info = new android.hardware.Camera.CameraInfo();
		android.hardware.Camera.getCameraInfo(cameraId, info);
		
		int result;
		if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {
			result = (info.orientation + rotationDegrees) % 360;
			result = (360 - result) % 360; // compensate the mirror
		} else { // back-facing
			result = (info.orientation - rotationDegrees + 360) % 360;
		}
		
		Log.w("mediastreamer", "Camera preview orientation: "+ result);
		try {
			camera.setDisplayOrientation(result);
		} catch (Exception exc) {
			Log.e("mediastreamer", "Failed to execute: camera[" + camera + "].setDisplayOrientation(" + result + ")");
			exc.printStackTrace();
		}
	}
	
	private static int[] findClosestEnclosingFpsRange(int expectedFps, List<int[]> fpsRanges) {
		Log.d("mediastreamer", "Searching for closest fps range from " + expectedFps);
		// init with first element
		int[] closestRange = fpsRanges.get(0);
		int measure = Math.abs(closestRange[0] - expectedFps)
				+ Math.abs(closestRange[1] - expectedFps);
		for (int[] curRange : fpsRanges) {
			if (curRange[0] > expectedFps || curRange[1] < expectedFps) continue;
			int curMeasure = Math.abs(curRange[0] - expectedFps)
					+ Math.abs(curRange[1] - expectedFps);
			if (curMeasure < measure) {
				closestRange=curRange;
				measure = curMeasure;
				Log.d("mediastreamer", "a better range has been found: w="+closestRange[0]+",h="+closestRange[1]);
			}
		}
		Log.d("mediastreamer", "The closest fps range is w="+closestRange[0]+",h="+closestRange[1]);
		return closestRange;
	}
}
