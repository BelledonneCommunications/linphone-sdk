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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


/*
CAUTION: Rename this file to MediastreamerActivity.java before using it. It is currently
named MediastreamerActivity.java.ignored because build process of Linphone is
incompatible with two main activity declared in java source files.
*/



package org.linphone.mediastream;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

import org.linphone.mediastream.video.AndroidVideoWindowImpl;
import org.linphone.mediastream.video.capture.AndroidVideoApi5JniWrapper;

import android.app.Activity;
import android.content.Intent;
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
	native int initDefaultArgs();
	native boolean parseArgs(int argc, String[] argv, int msObj);
	native void setupMediaStreams(int msObj);
	native void runLoop(int msObj);
	native void clear(int msObj);
	native void stopMediaStream();

	native void setVideoWindowId(Object wid, int msObj);
	native void setVideoPreviewWindowId(Object wid, int msObj);
	native void setDeviceRotation(int rotation, int msObj);
	native void changeCamera(int newCameraId, int msObj);

	Thread msThread;
	int cameraId = 0;
	String videoCodec = VP8_MIME_TYPE;
	String remoteIp = "127.0.0.1";
	short remotePort = 4000, localPort = 4000;
	int bitrate = 256;

	boolean pleaseRestart;

	int nativeObj;

	AndroidVideoWindowImpl mVideoWindow;

	static String BUNDLE_CAMERA_ID_KEY = "CameraIdKey";
	static String BUNDLE_VIDEO_CODEC_KEY = "VideoCodecKey";
	static String BUNDLE_REMOTE_IP_KEY = "RemoteIpKey";
	static String BUNDLE_REMOTE_PORT_KEY = "RemotePortKey";
	static String BUNDLE_LOCAL_PORT_KEY = "LocalPortKey";
	static String BUNDLE_BITRATE_KEY = "BitrateKey";

	static String VP8_MIME_TYPE = "VP8";
	static String MPEG4_MIME_TYPE = "MP4V-ES";

	Object destroyMutex= new Object();

	static {
		String eabi = "armeabi";
		if (Version.isX86()) {
			eabi = "x86";
		} else if (Version.isArmv7()) {
			eabi = "armeabi-v7a";
		}

		// FFMPEG (audio/video)
		if (Version.isX86()) {
			loadOptionalLibrary("avutil-linphone-x86");
			loadOptionalLibrary("swscale-linphone-x86");
			loadOptionalLibrary("avcodec-linphone-x86");
		} else if (Version.isArmv7()) {
			loadOptionalLibrary("avutil-linphone-arm");
			loadOptionalLibrary("swscale-linphone-arm");
			loadOptionalLibrary("avcodec-linphone-arm");
		}

		// OPENSSL (cryptography)
		// linphone suffix avoids collision with libs in /system/lib
		loadOptionalLibrary("crypto-linphone-" + eabi);
		loadOptionalLibrary("ssl-linphone-" + eabi);

		// Secure RTP and key negotiation
		loadOptionalLibrary("srtp-" + eabi);
		loadOptionalLibrary("zrtpcpp-" + eabi); // GPLv3+

		// Main library
		System.loadLibrary("mediastreamer2-" + eabi);
	}

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		/* declare layout */
		setContentView(R.layout.main);

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

		// init args from bundle/default values
		Bundle bundleToUse = savedInstanceState;
		if (bundleToUse == null) {
			bundleToUse = getIntent().getExtras();

		}
		if (bundleToUse != null) {
			Set<String> keys = bundleToUse.keySet();
			for(String s: keys)
				Log.e("sm", "Key: " + s + ", value: " + bundleToUse.get(s));

			cameraId = bundleToUse.getInt(BUNDLE_CAMERA_ID_KEY, 0);
			if (bundleToUse.containsKey(BUNDLE_VIDEO_CODEC_KEY))
				videoCodec = bundleToUse.getString(BUNDLE_VIDEO_CODEC_KEY);
			if (bundleToUse.containsKey(BUNDLE_REMOTE_IP_KEY))
				remoteIp = bundleToUse.getString(BUNDLE_REMOTE_IP_KEY);
			remotePort = bundleToUse.getShort(BUNDLE_REMOTE_PORT_KEY, (short)4000);
			localPort = bundleToUse.getShort(BUNDLE_LOCAL_PORT_KEY, (short)4000);
			bitrate = bundleToUse.getInt(BUNDLE_BITRATE_KEY, 256);
		} else {
			Log.w("mediastreamer", "No bundle to restore params from");
		}

		pleaseRestart = false;
		// pass arguments to native code
		final List<String> args = new ArrayList<String>();
		args.add("prog_name");
		args.add("--local");
		args.add(Short.toString(localPort));
		args.add("--remote");
		args.add(remoteIp + ":" + remotePort);
		args.add("--payload");
		args.add("video/" + videoCodec + "/90000");
		args.add("--camera");
		args.add("Android" + cameraId);
		// we pass device rotation as an argument (so mediastream.c can tell the videostream about it BEFORE it's configured)
		args.add("--device-rotation");
		int rot = rotationToAngle(getWindowManager().getDefaultDisplay()
				.getRotation());
		args.add(Integer.toString(rot));
		args.add("--bitrate");
		args.add(Integer.toString(bitrate*1000));
		// override default value from mediastream.c (but the actual resolution is limited by the encoder + bitrate)
		args.add("--width");
		args.add("1920");
		args.add("--height");
		args.add("1080");

		nativeObj = initDefaultArgs();

		String[] _args = new String[args.size()];
		parseArgs(args.size(), args.toArray(_args), nativeObj);

		setupMediaStreams(nativeObj);

		/* Mediastreamer is ready, we can finish Java initialization, and start
		 * Mediastream in a separate thread
		 */
		msThread = new Thread() {
			public void run() {
				synchronized (destroyMutex) {
					Log.e("ms", "Starting mediastream !");
					runLoop(nativeObj);
					Log.e("ms", "Mediastreamer ended");
					clear(nativeObj);
					Log.e("ms", "Mediastreamer cleared");
				}
			};
		};

		/* start mediastream */
		msThread.start();

		/* instanciate object responsible of video rendering */
		mVideoWindow = new AndroidVideoWindowImpl(view, previewSurface);

		mVideoWindow
				.setListener(new AndroidVideoWindowImpl.VideoWindowListener() {
					public void onVideoPreviewSurfaceReady(AndroidVideoWindowImpl vw, SurfaceView sv) {
						setVideoPreviewWindowId(previewSurface, nativeObj);
					};

					@Override
					public void onVideoPreviewSurfaceDestroyed(
							AndroidVideoWindowImpl vw) {
					}

					public void onVideoRenderingSurfaceDestroyed(AndroidVideoWindowImpl vw) {};

					public void onVideoRenderingSurfaceReady(AndroidVideoWindowImpl vw, SurfaceView sv) {
						setVideoWindowId(vw, nativeObj);
						// set device rotation too
						setDeviceRotation(rotationToAngle(getWindowManager().getDefaultDisplay()
								.getRotation()), nativeObj);
					}
				});

		mVideoWindow.init();
	}

	@Override
	protected void onSaveInstanceState(Bundle outState) {
		super.onSaveInstanceState(outState);
		saveStateToBundle(outState);
	}

	void saveStateToBundle(Bundle b) {
		b.putInt(BUNDLE_CAMERA_ID_KEY, cameraId);
		b.putString(BUNDLE_VIDEO_CODEC_KEY, videoCodec);
		b.putString(BUNDLE_REMOTE_IP_KEY, remoteIp);
		b.putShort(BUNDLE_REMOTE_PORT_KEY, remotePort);
		b.putShort(BUNDLE_LOCAL_PORT_KEY, localPort);
		b.putInt(BUNDLE_BITRATE_KEY, bitrate);
	}

	@Override
	protected void onDestroy() {
		mVideoWindow.release();
		stopMediaStream();

		Log.d("ms", "Waiting for complete mediastremer destruction");
		synchronized (destroyMutex) {
			Log.d("ms", "MediastreamerActivity destroyed");
		}

		super.onDestroy();

		if (pleaseRestart) {
			Intent intent = getIntent();
			intent.putExtra(BUNDLE_CAMERA_ID_KEY, cameraId);
			intent.putExtra(BUNDLE_VIDEO_CODEC_KEY, videoCodec);
			intent.putExtra(BUNDLE_REMOTE_IP_KEY, remoteIp);
			intent.putExtra(BUNDLE_REMOTE_PORT_KEY, remotePort);
			intent.putExtra(BUNDLE_LOCAL_PORT_KEY, localPort);
			intent.putExtra(BUNDLE_BITRATE_KEY, bitrate);
			startActivity(intent);
		}
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

		// init UI
		if (videoCodec.equals(VP8_MIME_TYPE))
			menu.findItem(R.id.videocall_menu_codec_vp8).setChecked(true);
		else if (videoCodec.equals(MPEG4_MIME_TYPE))
			menu.findItem(R.id.videocall_menu_codec_mpeg4).setChecked(true);

		switch (bitrate) {
			case 64:
				menu.findItem(R.id.videocall_menu_bitrate_64_kbps).setChecked(true); break;
			case 128:
				menu.findItem(R.id.videocall_menu_bitrate_128_kbps).setChecked(true); break;
			case 256:
				menu.findItem(R.id.videocall_menu_bitrate_256_kbps).setChecked(true); break;
			case 512:
				menu.findItem(R.id.videocall_menu_bitrate_512_kbps).setChecked(true); break;
			case 1024:
				menu.findItem(R.id.videocall_menu_bitrate_1024_kbps).setChecked(true); break;
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
			setVideoPreviewWindowId(null, nativeObj);
			changeCamera(cameraId, nativeObj);
			setVideoPreviewWindowId(findViewById(R.id.video_capture_surface), nativeObj);
			break;
		case R.id.videocall_menu_codec_vp8:
			updateVideoCodec(VP8_MIME_TYPE);
			break;
		case R.id.videocall_menu_codec_mpeg4:
			updateVideoCodec(MPEG4_MIME_TYPE);
			break;
		case R.id.videocall_menu_bitrate_64_kbps:
			updateBitrate(64);
			break;
		case R.id.videocall_menu_bitrate_128_kbps:
			updateBitrate(128);
			break;
		case R.id.videocall_menu_bitrate_256_kbps:
			updateBitrate(256);
			break;
		case R.id.videocall_menu_bitrate_512_kbps:
			updateBitrate(512);
			break;
		case R.id.videocall_menu_bitrate_1024_kbps:
			updateBitrate(1024);
			break;
		}
		return true;
	}

	private void updateVideoCodec(String newCodec) {
		if (newCodec != videoCodec) {
			videoCodec = newCodec;
			// restart ourself
			pleaseRestart = true;
			finish();
		}
	}

	private void updateBitrate(int newBr) {
		if (newBr != bitrate) {
			bitrate = newBr;
			pleaseRestart = true;
			finish();
		}
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
