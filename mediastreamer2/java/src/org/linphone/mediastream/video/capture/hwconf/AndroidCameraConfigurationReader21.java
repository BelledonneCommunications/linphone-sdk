package org.linphone.mediastream.video.capture.hwconf;

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.ImageFormat;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.os.Build;

import org.linphone.mediastream.MediastreamerAndroidContext;
import org.linphone.mediastream.video.capture.hwconf.AndroidCameraConfiguration.AndroidCamera;

import org.linphone.mediastream.Log;

import java.util.ArrayList;
import java.util.List;

/**
 * Android cameras detection, using SDK superior or egal to 21.
 */
public class AndroidCameraConfigurationReader21 {
	@TargetApi(Build.VERSION_CODES.LOLLIPOP)
	static public AndroidCameraConfiguration.AndroidCamera[] probeCameras() {
		Context context = MediastreamerAndroidContext.getContext();
		if (context != null) {
			CameraManager manager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
			int numOfAvailableCameras = 0;
			String[] cameraList;

			try {
				cameraList = manager.getCameraIdList();
				final List<AndroidCamera> cam = new ArrayList<AndroidCamera>(cameraList.length);
				for (int i = 0; i < cameraList.length; i++) {
					String cameraId = cameraList[i];
					CameraCharacteristics characteristics = manager.getCameraCharacteristics(cameraId);
					int camFacing = characteristics.get(CameraCharacteristics.LENS_FACING);
					boolean frontFacing = false;
					if (camFacing == CameraCharacteristics.LENS_FACING_FRONT) {
						frontFacing = true;
					}
					int camOrientation = characteristics.get(CameraCharacteristics.SENSOR_ORIENTATION);
					StreamConfigurationMap configs = characteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
					android.util.Size[] supportedSizes = configs.getOutputSizes(ImageFormat.JPEG);
					List<AndroidCamera.Size> supportedPreviewSizes = new ArrayList<AndroidCamera.Size>(supportedSizes.length);
					for (int j = 0; j < supportedSizes.length; j++) {
						android.util.Size size = supportedSizes[j];
						supportedPreviewSizes.add(new AndroidCamera.Size(size.getWidth(), size.getHeight()));
					}

					cam.add(new AndroidCamera(i, frontFacing, supportedPreviewSizes, camOrientation));
					numOfAvailableCameras++;
				}

				AndroidCamera[] result = new AndroidCamera[numOfAvailableCameras];
				result = cam.toArray(result);

				return result;

			} catch (CameraAccessException exp) {
				Log.e(exp);
			}
		}
		return new AndroidCamera[0]; //empty array
	}
}
