package org.linphone;

import java.util.ArrayList;
import java.util.List;

import org.linphone.core.AndroidVideoWindowImpl;

import android.app.Activity;
import android.hardware.Camera;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class MediastreamerActivity extends Activity implements
		SensorEventListener {
	native int runMediaStream(int argc, String[] argv);

	native int stopMediaStream();

	native void setVideoWindowId(Object wid);

	native void setVideoPreviewWindowId(Object wid);

	native void setDeviceRotation(int rotation);
 
	native void changeCamera(int newCameraId);

	Thread msThread;
	int cameraId;

	private static void loadOptionalLibrary(String s) {
		try {
			System.loadLibrary(s);
		} catch (Throwable e) {
			Log.w("Unable to load optional library lib", s);
		}
	}

	static {
		// FFMPEG (audio/video)
		loadOptionalLibrary("avutil");
		loadOptionalLibrary("swscale");
		loadOptionalLibrary("avcore");
		loadOptionalLibrary("avcodec");

		// Main library
		System.loadLibrary("mediastreamer2");
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the currently selected menu XML resource.
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.videocall_activity_menu, menu);

		if (Camera.getNumberOfCameras() == 1) {
			menu.findItem(R.id.videocall_menu_change_camera).setVisible(false);
		}
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		switch (item.getItemId()) {
		case R.id.videocall_menu_exit:
			this.finish();
			break;
		case R.id.videocall_menu_change_camera:
			cameraId = (cameraId + 1) % Camera.getNumberOfCameras();
			changeCamera(cameraId);
			setVideoPreviewWindowId(findViewById(R.id.video_capture_surface));
			break;
		}

		return true;
	}

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		/* declare layout */
		setContentView(R.layout.main);

		cameraId = 0;

		Log.i("ms", "Mediastreamer starting !");

		/* retrieve preview surface */
		final SurfaceView previewSurface = (SurfaceView) findViewById(R.id.video_capture_surface);
		final SurfaceHolder holder = previewSurface.getHolder();
		holder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);

		/* retrieve rendering surface */
		GLSurfaceView view = (GLSurfaceView) findViewById(R.id.video_surface);

		/* force surfaces Z ordering */
		view.setZOrderOnTop(false);
		previewSurface.setZOrderOnTop(true);

		/* register callback, allowing us to use preview surface when ready */
		holder.addCallback(new SurfaceHolder.Callback() {
			@Override
			public void surfaceDestroyed(SurfaceHolder holder) {
				}

			@Override
			public void surfaceCreated(SurfaceHolder holder) {
				setVideoPreviewWindowId(previewSurface);
			}

			@Override
			public void surfaceChanged(SurfaceHolder holder, int format,
					int width, int height) {
				// ...
			}
		});

		/* instanciate object responsible of video rendering */
		AndroidVideoWindowImpl mVideoWindow = new AndroidVideoWindowImpl(view);
		mVideoWindow
				.setListener(new AndroidVideoWindowImpl.VideoWindowListener() {
					public void onSurfaceDestroyed(AndroidVideoWindowImpl vw) {
						// setVideoWindowId(null);
					}

					public void onSurfaceReady(AndroidVideoWindowImpl vw) {
						setVideoWindowId(vw);
						// set device rotation too
						onSensorChanged(null);
					}
				});

		final List<String> args = new ArrayList<String>();
		args.add("prog_name");
		args.add("--local");
		args.add("4000");
		args.add("--remote");
		args.add("127.0.0.1:4000");
		args.add("--payload");
		args.add("103");
		args.add("--camera");
		args.add("Android0");

		// if the phone is vertical => supply portrait mode resolution
		int rot = rotationToAngle(getWindowManager().getDefaultDisplay()
				.getRotation());
		if (rot % 180 == 0) {
			args.add("--width");
			args.add("240");
			args.add("--height");
			args.add("320");
		}
		msThread = new Thread() {
			public void run() {
				Log.e("ms", "Starting mediastream !");
				String[] _args = new String[args.size()];
				int ret = runMediaStream(args.size(), args.toArray(_args));
				Log.e("ms", "Mediastreamer ended (return code:" + ret + ")");
			};
		};

		/* start mediastream */
		msThread.start();
	}

	@Override
	protected void onResume() {
		super.onResume();
		SensorManager mSensorManager = (SensorManager) getSystemService(SENSOR_SERVICE);
		Sensor mAccelerometer = mSensorManager
				.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);

		mSensorManager.registerListener(this, mAccelerometer,
				SensorManager.SENSOR_DELAY_NORMAL);
	}

	@Override
	protected void onPause() {
		super.onPause();
		((SensorManager) getSystemService(SENSOR_SERVICE))
				.unregisterListener(this);
	}

	@Override
	public void onAccuracyChanged(Sensor sensor, int accuracy) {

	}

	@Override
	public void onSensorChanged(SensorEvent event) {
		int rot = rotationToAngle(getWindowManager().getDefaultDisplay()
				.getRotation());
		// Returning rotation FROM ITS NATURAL ORIENTATION
		setDeviceRotation(rot);
	}

	@Override
	protected void onDestroy() {
		stopMediaStream();
		try {
			msThread.join(100000);
		} catch (Exception exc) {

		}
		Log.d("ms", "MediastreamerActivity destroyed");
		super.onDestroy();
	}

	static int rotationToAngle(int r) {
		switch (r) {
		case Surface.ROTATION_0:
			return 0;
		case Surface.ROTATION_90:
			return 90;
		case Surface.ROTATION_180:
			return 180;
		case Surface.ROTATION_270:
			return 270;
		}
		return 0;
	}
}