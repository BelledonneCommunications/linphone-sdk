/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
package org.linphone.mediastream.video.capture;

import org.linphone.mediastream.Log;

import android.content.Context;
import android.graphics.Matrix;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.util.Size;
import android.view.TextureView;

public class CaptureTextureView extends TextureView {
    public enum DisplayMode {
        BLACK_BARS, OCCUPY_ALL_SPACE, HYBRID  
    }

    private int mCapturedVideoWidth = 0;
    private int mCapturedVideoHeight = 0;
    private int mRotation = -1;
    private DisplayMode mActualMode = DisplayMode.BLACK_BARS;
    private RectF mPreviewRect = null;

    protected boolean mAlignTopRight = true; // Legacy behavior, not used when display mode is OCCUPY_ALL_SPACE (obviously)
    protected DisplayMode mDisplayMode = DisplayMode.BLACK_BARS; // Legacy behavior

    public CaptureTextureView(Context context) {
        this(context, null);
    }

    public CaptureTextureView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public CaptureTextureView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
    }

    public DisplayMode getActualDisplayMode() {
        return mActualMode;
    }

    public Size getPreviewVideoSize() {
        return new Size(mCapturedVideoWidth, mCapturedVideoHeight);
    }

    public RectF getPreviewRectF() {
        return mPreviewRect;
    }

    public void setRotation(int rotation) {
        if (rotation != mRotation) {
            mRotation = rotation;
            Log.i("[Capture TextureView] Changing preview texture rotation to [" + rotation + "]°");
            rotateToMatchDisplayOrientation();
        } else {
            Log.w("[Capture TextureView] Rotation is already set to [" + mRotation + "]°, skipping");
        }
    }

    public void rotateToMatchDisplayOrientation() {
        int rotation = mRotation;
        int width = getMeasuredWidth();
        int height = getMeasuredHeight();

        Matrix matrix = new Matrix();
        RectF textureViewRect = new RectF(0, 0, width, height);
        matrix.mapRect(textureViewRect);

        if (rotation != -1) {
            if (rotation % 180 == 90) {
                Log.i("[Capture TextureView] Rotating preview texture by [" + rotation + "]°");
                float[] src = new float[] { 0.f, 0.f, width, 0.f, 0.f, height, width, height, };
                float[] dst = new float[] { 0.f, height, 0.f, 0.f, width, height, width, 0.f, };
                if (rotation == 270) {
                    dst = new float[] { width, 0.f, width, height, 0.f, 0.f, 0.f, height, };
                }
                matrix.setPolyToPoly(src, 0, dst, 0, 4);
            } else if (rotation == 180) {
                Log.i("[Capture TextureView] Rotating preview texture by 180°");
                matrix.postRotate(180, width / 2, height / 2);
            }
        }

        if (mCapturedVideoWidth != 0 && mCapturedVideoHeight != 0) {
            Log.i("[Capture TextureView] TextureView size is " + width + "x" + height + ", captured video size is " + mCapturedVideoWidth + "x" + mCapturedVideoHeight);
            
            if (mDisplayMode == DisplayMode.HYBRID) {
                // If the image has the same orientation as the screen then use OccupyAllSpace mode
                if ((width >= height && mCapturedVideoWidth >= mCapturedVideoHeight)
                    || (height >= width && mCapturedVideoHeight >= mCapturedVideoWidth)
                ) {
                    Log.i("[Capture TextureView] Hybrid mode enabled, display mode will be 'occupy all space'");
                    mActualMode = DisplayMode.OCCUPY_ALL_SPACE;
                } else {
                    Log.i("[Capture TextureView] Hybrid mode enabled, display mode will be 'black bars'");
                    mActualMode = DisplayMode.BLACK_BARS;
                }
            } else {
                if (mDisplayMode == DisplayMode.BLACK_BARS) {
                    Log.i("[Capture TextureView] Hybrid mode disabled, display mode will be 'black bars'");
                } else {
                    Log.i("[Capture TextureView] Hybrid mode disabled, display mode will be 'occupy all space'");
                }
                mActualMode = mDisplayMode;
            }

            Matrix addtionalTransform = new Matrix();
            float ratioWidth = width;
            float ratioHeight = height;

            if (mActualMode == DisplayMode.BLACK_BARS) {
                if (width < height * mCapturedVideoWidth / mCapturedVideoHeight) {
                    ratioHeight = width * mCapturedVideoHeight / mCapturedVideoWidth;
                } else {
                    ratioWidth = height * mCapturedVideoWidth / mCapturedVideoHeight;
                }

                RectF capturedVideoRect = new RectF(0, 0, ratioWidth, ratioHeight);
                if (mAlignTopRight) {
                    Log.i("[Capture TextureView] Aligning the video in the rop-right corner");
                    capturedVideoRect.offset(width - ratioWidth, 0);
                } else {
                    capturedVideoRect.offset(textureViewRect.centerX() - capturedVideoRect.centerX(), textureViewRect.centerY() - capturedVideoRect.centerY());
                }
                addtionalTransform.setRectToRect(textureViewRect, capturedVideoRect, Matrix.ScaleToFit.FILL);

                mPreviewRect = capturedVideoRect;
            } else {
                RectF capturedVideoRect = new RectF(0, 0, mCapturedVideoWidth, mCapturedVideoHeight);
                float centerX = textureViewRect.centerX() - capturedVideoRect.centerX();
                float centerY = textureViewRect.centerY() - capturedVideoRect.centerY();
                capturedVideoRect.offset(centerX, centerY);
                addtionalTransform.setRectToRect(textureViewRect, capturedVideoRect, Matrix.ScaleToFit.FILL);

                float scale = Math.max((float) height / mCapturedVideoHeight, (float) width / mCapturedVideoWidth);
                addtionalTransform.postScale(scale, scale, textureViewRect.centerX(), textureViewRect.centerY());

                mPreviewRect = textureViewRect;
            }
            matrix.postConcat(addtionalTransform);
        }

        setTransform(matrix);
    }

    public void setAspectRatio(int width, int height) {
        if (width < 0 || height < 0) {
            throw new IllegalArgumentException("Size cannot be negative.");
        }
        
        Log.i("[Capture TextureView] Changing preview texture ratio to match " + width + "x" + height + " captured video size");
        mCapturedVideoWidth = width;
        mCapturedVideoHeight = height;

        rotateToMatchDisplayOrientation();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        rotateToMatchDisplayOrientation();
    }
}