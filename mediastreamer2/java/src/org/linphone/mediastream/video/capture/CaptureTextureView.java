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
import android.util.AttributeSet;
import android.view.TextureView;
import android.view.WindowManager;

public class CaptureTextureView extends TextureView {
    private Context mContext;
    private int mRatioWidth = 0;
    private int mRatioHeight = 0;

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
        Log.i("[CaptureTextureView] Rotating preview texture by " + rotation);

        Matrix matrix = new Matrix();
        int width = getWidth();
        int height = getHeight();

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

        setTransform(matrix);
    }

    public void setAspectRatio(int width, int height) {
        if (width < 0 || height < 0) {
            throw new IllegalArgumentException("Size cannot be negative.");
        }
        mRatioWidth = width;
        mRatioHeight = height;
        Log.i("[CaptureTextureView] Changing preview texture ratio to match " + mRatioWidth + "x" + mRatioHeight);
        requestLayout();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        int width = MeasureSpec.getSize(widthMeasureSpec);
        int height = MeasureSpec.getSize(heightMeasureSpec);
        if (0 == mRatioWidth || 0 == mRatioHeight) {
            setMeasuredDimension(width, height);
        } else {
            if (width < height * mRatioWidth / mRatioHeight) {
                setMeasuredDimension(width, width * mRatioHeight / mRatioWidth);
            } else {
                setMeasuredDimension(height * mRatioWidth / mRatioHeight, height);
            }
        }
    }

}