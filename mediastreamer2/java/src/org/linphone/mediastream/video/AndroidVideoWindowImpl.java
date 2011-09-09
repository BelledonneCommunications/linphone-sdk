package org.linphone.mediastream.video;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import org.linphone.mediastream.video.display.OpenGLESDisplay;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Bitmap.Config;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.Surface.OutOfResourcesException;
import android.view.SurfaceHolder.Callback;

public class AndroidVideoWindowImpl  {
	private SurfaceView mVideoRenderingView;
	private SurfaceView mVideoPreviewView;
	
	private boolean useGLrendering;
	private Bitmap mBitmap; 

	private Surface mSurface; 
	private VideoWindowListener mListener;
	private Renderer renderer;
	
	/**
	 * Utility listener interface providing callback for Android events
	 * useful to Mediastreamer.
	 */
	public static interface VideoWindowListener{
		void onVideoRenderingSurfaceReady(AndroidVideoWindowImpl vw);
		void onVideoRenderingSurfaceDestroyed(AndroidVideoWindowImpl vw);
		
		void onVideoPreviewSurfaceReady(AndroidVideoWindowImpl vw);
		void onVideoPreviewSurfaceDestroyed(AndroidVideoWindowImpl vw);
		
		void onDeviceOrientationChanged(int newRotationDegrees);
	};
	
	/**
	 * @param renderingSurface Surface created by the application that will be used to render decoded video stream
	 * @param previewSurface Surface created by the application used by Android's Camera preview framework
	 */
	public AndroidVideoWindowImpl(final Activity activity, SurfaceView renderingSurface, SurfaceView previewSurface) {
		mVideoRenderingView = renderingSurface;
		mVideoPreviewView = previewSurface;
		
		useGLrendering = (renderingSurface instanceof GLSurfaceView);
		
		mBitmap = null;
		mSurface = null;
		mListener = null;
		
		// register callback for rendering surface events
		mVideoRenderingView.getHolder().addCallback(new Callback(){
			public void surfaceChanged(SurfaceHolder holder, int format,
					int width, int height) {
				Log.i("mediastream", "Video display surface is being changed.");
				if (!useGLrendering) {
					synchronized(AndroidVideoWindowImpl.this){
						mBitmap=Bitmap.createBitmap(width,height,Config.RGB_565);
						mSurface=holder.getSurface();
					}
				}
				if (mListener!=null) mListener.onVideoRenderingSurfaceReady(AndroidVideoWindowImpl.this);
				Log.w("mediastream", "Video display surface changed");
			}

			public void surfaceCreated(SurfaceHolder holder) {
				Log.w("mediastream", "Video display surface created");
			}

			public void surfaceDestroyed(SurfaceHolder holder) {
				if (!useGLrendering) {
					synchronized(AndroidVideoWindowImpl.this){
						mSurface=null;
						mBitmap=null;
					}
				}
				if (mListener!=null)
					mListener.onVideoRenderingSurfaceDestroyed(AndroidVideoWindowImpl.this);
				Log.d("mediastream", "Video display surface destroyed"); 
			}
		});
		// register callback for preview surface events
		mVideoPreviewView.getHolder().addCallback(new Callback(){
			public void surfaceChanged(SurfaceHolder holder, int format,
					int width, int height) {
				Log.i("mediastream", "Video preview surface is being changed.");
				if (mListener!=null) 
					mListener.onVideoPreviewSurfaceReady(AndroidVideoWindowImpl.this);
				Log.w("mediastream", "Video preview surface changed");
			}

			public void surfaceCreated(SurfaceHolder holder) {
				Log.w("mediastream", "Video preview surface created");
			}

			public void surfaceDestroyed(SurfaceHolder holder) {
				if (mListener!=null)
					mListener.onVideoPreviewSurfaceDestroyed(AndroidVideoWindowImpl.this);
				Log.d("mediastream", "Video preview surface destroyed"); 
			}
		});
		
		// register for orientation event
		SensorManager sensorMgr = (SensorManager) activity.getSystemService(Activity.SENSOR_SERVICE);
		Sensor mAccelerometer = sensorMgr.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
		sensorMgr.registerListener(new SensorEventListener() {
			@Override
			public void onSensorChanged(SensorEvent event) {
				int rot = rotationToAngle(activity.getWindowManager().getDefaultDisplay()
						.getRotation());
				// Returning rotation FROM ITS NATURAL ORIENTATION
				if (mListener != null)
					mListener.onDeviceOrientationChanged(rot);
			}
			
			@Override
			public void onAccuracyChanged(Sensor sensor, int accuracy) { }
		}, mAccelerometer, SensorManager.SENSOR_DELAY_NORMAL);
		
		
		if (useGLrendering) {
			renderer = new Renderer();
			((GLSurfaceView)mVideoRenderingView).setRenderer(renderer);
			((GLSurfaceView)mVideoRenderingView).setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
		}
	}

	public void setListener(VideoWindowListener l){
		mListener=l; 
	}
	public Surface getSurface(){
		if (useGLrendering)
			Log.e("mediastream", "View class does not match Video display filter used (you must use a non-GL View)");
		return mVideoRenderingView.getHolder().getSurface();
	}
	public Bitmap getBitmap(){
		if (useGLrendering)
			Log.e("mediastream", "View class does not match Video display filter used (you must use a non-GL View)");
		return mBitmap;
	}
	 
	public void setOpenGLESDisplay(int ptr) {
		if (!useGLrendering)
			Log.e("mediastream", "View class does not match Video display filter used (you must use a GL View)");
		renderer.setOpenGLESDisplay(ptr);
	}
	
	public void requestRender() {
		((GLSurfaceView)mVideoRenderingView).requestRender();
	}
	
	//Called by the mediastreamer2 android display filter 
	public synchronized void update(){
		if (mSurface!=null){
			try {
				Canvas canvas=mSurface.lockCanvas(null); 
				canvas.drawBitmap(mBitmap, 0, 0, null);
				mSurface.unlockCanvasAndPost(canvas);
				 
			} catch (IllegalArgumentException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			} catch (OutOfResourcesException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}  
		} 
	}
	
    private static class Renderer implements GLSurfaceView.Renderer {
    	int ptr;
    	boolean initPending;
    	int width, height;
    	
    	public Renderer() {
    		ptr = 0;
    		initPending = false;
    	}
    	 
    	public void setOpenGLESDisplay(int ptr) {
    		this.ptr = ptr;
    	}

        public void onDrawFrame(GL10 gl) {
        	if (ptr == 0)
        		return;
        	if (initPending) {
            	OpenGLESDisplay.init(ptr, width, height);
            	initPending = false;
        	}
            OpenGLESDisplay.render(ptr);
        }
        
        public void onSurfaceChanged(GL10 gl, int width, int height) {
        	/* delay init until ptr is set */
        	this.width = width;
        	this.height = height;
        	initPending = true;
        }

        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
           
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


