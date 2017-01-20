/*
AndroidVideoWindowImpl.java
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
package org.linphone.mediastream.video;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import org.linphone.mediastream.Log;
import org.linphone.mediastream.video.display.OpenGLESDisplay;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.opengl.GLSurfaceView;
import android.view.Surface;
import android.view.Surface.OutOfResourcesException;
import android.view.SurfaceHolder;
import android.view.SurfaceHolder.Callback;
import android.view.SurfaceView;

public class AndroidVideoWindowImpl {
	private SurfaceView mVideoRenderingView;
	private SurfaceView mVideoPreviewView;
	
	private boolean useGLrendering;
	private Bitmap mBitmap = null; 

	private Surface mSurface = null; 
	private VideoWindowListener mListener = null;
	private Renderer renderer;
	
	/**
	 * Utility listener interface providing callback for Android events
	 * useful to Mediastreamer.
	 */
	public static interface VideoWindowListener{
		void onVideoRenderingSurfaceReady(AndroidVideoWindowImpl vw, SurfaceView surface);
		void onVideoRenderingSurfaceDestroyed(AndroidVideoWindowImpl vw);
		
		void onVideoPreviewSurfaceReady(AndroidVideoWindowImpl vw, SurfaceView surface);
		void onVideoPreviewSurfaceDestroyed(AndroidVideoWindowImpl vw);
	};
	
	/**
	 * Constructor
	 * @param renderingSurface Surface created by the application that will be used to render decoded video stream
	 * @param previewSurface Surface created by the application used by Android's Camera preview framework
	 * @param listener Specified a listener. null is accepted
	 */
	public AndroidVideoWindowImpl(SurfaceView renderingSurface, SurfaceView previewSurface, VideoWindowListener listener) {
		mVideoRenderingView = renderingSurface;
		mVideoPreviewView = previewSurface;
		useGLrendering = (renderingSurface instanceof GLSurfaceView);
		mListener = listener;
		init();
	}
	
	/**
	 * @param renderingSurface Surface created by the application that will be used to render decoded video stream
	 * @param previewSurface Surface created by the application used by Android's Camera preview framework
	 * @deprecated Use the new constructor instead
	 */
	public AndroidVideoWindowImpl(SurfaceView renderingSurface, SurfaceView previewSurface) {
		mVideoRenderingView = renderingSurface;
		mVideoPreviewView = previewSurface;
		useGLrendering = (renderingSurface instanceof GLSurfaceView);
	}

	/**
	 * @deprecated It is now automatically called by the new constructor
	 */
	public void init() {
		// register callback for rendering surface events
		mVideoRenderingView.getHolder().addCallback(new Callback(){
			public void surfaceChanged(SurfaceHolder holder, int format,
					int width, int height) {
				Log.i("Video display surface is being changed.");
				if (!useGLrendering) {
					synchronized(AndroidVideoWindowImpl.this){
						mBitmap=Bitmap.createBitmap(width,height,Config.RGB_565);
						mSurface=holder.getSurface();
					}
				}
				if (mListener!=null) mListener.onVideoRenderingSurfaceReady(AndroidVideoWindowImpl.this, mVideoRenderingView);
				Log.w("Video display surface changed");
			}

			public void surfaceCreated(SurfaceHolder holder) {
				Log.w("Video display surface created");
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
				Log.d("Video display surface destroyed"); 
			}
		});
		// register callback for preview surface events
		if (mVideoPreviewView != null) {
			mVideoPreviewView.getHolder().addCallback(new Callback(){
				public void surfaceChanged(SurfaceHolder holder, int format,
						int width, int height) {
					Log.i("Video preview surface is being changed.");
					if (mListener!=null) 
						mListener.onVideoPreviewSurfaceReady(AndroidVideoWindowImpl.this, mVideoPreviewView);
					Log.w("Video preview surface changed");
				}

				public void surfaceCreated(SurfaceHolder holder) {
					Log.w("Video preview surface created");
				}

				public void surfaceDestroyed(SurfaceHolder holder) {
					if (mListener!=null)
						mListener.onVideoPreviewSurfaceDestroyed(AndroidVideoWindowImpl.this);
					Log.d("Video preview surface destroyed"); 
				}
			});
		}
		
		if (useGLrendering) {
			renderer = new Renderer();
			((GLSurfaceView)mVideoRenderingView).setRenderer(renderer);
			((GLSurfaceView)mVideoRenderingView).setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
		}
	}
	
	public void release() {
		//mSensorMgr.unregisterListener(this);
	}

	/**
	 * Set a listener
	 * @param l A listener
	 * @deprecated Specify pass a listener to the constructor instead. 
	 */
	public void setListener(VideoWindowListener l){
		mListener=l; 
	}
	public Surface getSurface(){
		if (useGLrendering)
			Log.e("View class does not match Video display filter used (you must use a non-GL View)");
		return mVideoRenderingView.getHolder().getSurface();
	}
	public Bitmap getBitmap(){
		if (useGLrendering)
			Log.e( "View class does not match Video display filter used (you must use a non-GL View)");
		return mBitmap;
	}
	 
	public void setOpenGLESDisplay(long ptr) {
		if (!useGLrendering)
			Log.e("View class does not match Video display filter used (you must use a GL View)");
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
    	long ptr;
    	boolean initPending;
    	int width, height;
    	
    	public Renderer() {
    		ptr = 0;
    		initPending = false;
    	}
    	 
    	public void setOpenGLESDisplay(long ptr) {
    		/* 
    		 * Synchronize this with onDrawFrame:
    		 * - they are called from different threads (Rendering thread and Linphone's one)
    		 * - setOpenGLESDisplay can modify ptr while onDrawFrame is using it
    		 */
    		synchronized (this) {
	    		if (this.ptr != 0 && ptr != this.ptr) {
	    			initPending = true;
	    		}
	    		this.ptr = ptr;
    		}
    	}

        public void onDrawFrame(GL10 gl) {
        	/*
        	 * See comment in setOpenGLESDisplay
        	 */
        	synchronized (this) {
	        	if (ptr == 0)
	        		return;
	        	if (initPending) {
	            	OpenGLESDisplay.init(ptr, width, height);
	            	initPending = false;
	        	}
	            OpenGLESDisplay.render(ptr);
        	}
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
    
	public static int rotationToAngle(int r) {
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


