/*
CaptureTextureView.java
Copyright (C) 2019 Belledonne Communications, Grenoble, France

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

package org.linphone.mediastream.video.capture;

import org.linphone.mediastream.Log;

import android.content.Context;
import android.graphics.Matrix;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.TextureView;
import android.view.WindowManager;

public class CaptureTextureView extends TextureView {
    private Context mContext;
    private double mCapturedVideoWidth = 0;
    private double mCapturedVideoHeight = 0;

    public CaptureTextureView(Context context) {
        this(context, null);
        mContext = context;
    }

    public CaptureTextureView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
        mContext = context;
    }

    public CaptureTextureView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mContext = context;
    }

    public void rotateToMatchDisplayOrientation() {
        WindowManager windowManager = (WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE);
        int rotation = windowManager.getDefaultDisplay().getRotation() * 90;

        Matrix matrix = new Matrix();
        int width = getWidth();
        int height = getHeight();

        Log.d("[CaptureTextureView] Rotating preview texture by " + rotation);
        if (rotation % 180 == 90) {
            float[] src = new float[] { 0.f, 0.f, width, 0.f, 0.f, height, width, height, };
            float[] dst = new float[] { 0.f, height, 0.f, 0.f, width, height, width, 0.f, };
            if (rotation == 270) {
                dst = new float[] { width, 0.f, width, height, 0.f, 0.f, 0.f, height, };
            }
            matrix.setPolyToPoly(src, 0, dst, 0, 4);
        } else if (rotation == 180) {
            matrix.postRotate(180, width / 2, height / 2);
        }

        if (mCapturedVideoWidth != 0 && mCapturedVideoHeight != 0) {
            float ratioX = 1.f;
            float ratioY = 1.f;
            float widthTranslation = 0;
            float heightTranslation = 0;

            if (mCapturedVideoWidth > mCapturedVideoHeight) {
                ratioY = (float) (mCapturedVideoHeight / mCapturedVideoWidth);
                heightTranslation = height - (height * ratioY);
            } else if (mCapturedVideoHeight > mCapturedVideoWidth) {
                ratioX = (float) (mCapturedVideoWidth / mCapturedVideoHeight);
                widthTranslation = width - (width * ratioX);
            }

            Log.d("[CaptureTextureView] Video preview size is " + mCapturedVideoWidth + "x" + mCapturedVideoHeight + ", applying ratio " + ratioX + "x" + ratioY);
            matrix.postScale(ratioX, ratioY);
            matrix.postTranslate(widthTranslation, heightTranslation);
        }

        setTransform(matrix);
    }

    public void setAspectRatio(int width, int height) {
        if (width < 0 || height < 0) {
            throw new IllegalArgumentException("Size cannot be negative.");
        }
        
        Log.i("[CaptureTextureView] Changing preview texture ratio to match " + width + "x" + height);
        mCapturedVideoWidth = width;
        mCapturedVideoHeight = height;

        rotateToMatchDisplayOrientation();
    }
}