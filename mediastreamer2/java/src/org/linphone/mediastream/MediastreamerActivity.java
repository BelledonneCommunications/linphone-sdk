/*
MediastreamerActivity.java
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
package org.linphone.mediastream;

import java.util.ArrayList;
import java.util.List;

import org.linphone.mediastream.video.AndroidVideoWindowImpl;
import org.linphone.mediastream.video.capture.AndroidVideoApi5JniWrapper;

import android.app.Activity;
import android.hardware.Camera;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * Mediastreamer test activity.
 *
 */
public class MediastreamerActivity extends Activity {
	/** native methods from {MEDIASTREAMER2}/test/mediastreamer.c */
	native int runMediaStream(int argc, String[] argv);
	native int stopMediaStream();
	native void setVideoWindowId(Object wid);
	native void setVideoPreviewWindowId(Object wid);
	native void setDeviceRotation(int rotation);
	native void changeCamera(int newCameraId);

	Thread msThread;
	int cameraId = 0;
	String videoCodec = VP8_MIME_TYPE;
	AndroidVideoWindowImpl mVideoWindow;
	
	static String VP8_MIME_TYPE = "VP8-DRAFT-0-3-2";
	static String H264_MIME_TYPE = "H264";
	static String MPEG4_MIME_TYPE = "MP4V-ES";
	
	static {
		// FFMPEG (audio/video)
		loadOptionalLibrary("avutil");
		loadOptionalLibrary("swscale");
		loadOptionalLibrary("avcore");
		loadOptionalLibrary("avcodec");
 
		// Main library
		System.loadLibrary("mediastreamer2");
	}

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		/* declare layout */
		setContentView(R.layout.main);

		AndroidVideoApi5JniWrapper.setAndroidSdkVersion(Build.VERSION.SDK_INT);
		
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

		/* instanciate object responsible of video rendering */
		mVideoWindow = new AndroidVideoWindowImpl(this, view, previewSurface);
	
		mVideoWindow
				.setListener(new AndroidVideoWindowImpl.VideoWindowListener() {					
					public void onVideoPreviewSurfaceReady(AndroidVideoWindowImpl vw) {
						setVideoPreviewWindowId(previewSurface);
					};
					
					@Override
					public void onVideoPreviewSurfaceDestroyed(
							AndroidVideoWindowImpl vw) {	
					}
					
					public void onVideoRenderingSurfaceDestroyed(AndroidVideoWindowImpl vw) {};
					
					public void onVideoRenderingSurfaceReady(AndroidVideoWindowImpl vw) {
						setVideoWindowId(vw);
						// set device rotation too
						setDeviceRotation(rotationToAngle(getWindowManager().getDefaultDisplay()
								.getRotation()));
					}
					
					@Override
					public void onDeviceOrientationChanged(
							int rotationDegrees) {
						setDeviceRotation(rotationDegrees);
					}
				});
		
		mVideoWindow.init();

		final List<String> args = new ArrayList<String>();
		args.add("prog_name");
		args.add("--local");
		args.add("4000");
		args.add("--remote");
		args.add("127.0.0.1:4000");
		args.add("--payload");
		args.add("video/" + videoCodec + "/90000");
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
	protected void onDestroy() {
		mVideoWindow.release();
		stopMediaStream();
		try {
			msThread.join(100000);
		} catch (Exception exc) {

		}
		Log.d("ms", "MediastreamerActivity destroyed");
		super.onDestroy();
	}
	
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the currently selected menu XML resource.
		MenuInflater inflater = getMenuInflater();
		inflater.inflate(R.menu.videocall_activity_menu, menu);
		
		if (Build.VERSION.SDK_INT < 9)
			menu.findItem(R.id.videocall_menu_change_camera).setVisible(false);
		else {
			if (Camera.getNumberOfCameras() == 1)
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
	
	private static void loadOptionalLibrary(String s) {
		try {
			System.loadLibrary(s);
		} catch (Throwable e) {
			Log.w("Unable to load optional library lib", s);
		}
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