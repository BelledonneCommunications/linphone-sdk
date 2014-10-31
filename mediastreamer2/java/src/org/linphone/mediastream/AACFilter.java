/*
Log.java
Copyright (C) 2011  Belledonne Communications, Grenoble, France

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

import java.nio.ByteBuffer;

import android.annotation.TargetApi;
import android.media.MediaCodec;
import android.media.MediaCodec.BufferInfo;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;
@TargetApi(16)
public class AACFilter {
	int sampleRate, channelCount, bitrate;

	MediaCodec encoder;
	BufferInfo encoderBufferInfo;
	ByteBuffer[] encoderOutputBuffers, encoderInputBuffers;
	MediaCodec decoder;
	BufferInfo decoderBufferInfo;
	ByteBuffer[] decoderOutputBuffers, decoderInputBuffers;
	boolean initialized;

	/* Pattern to handle unload/reload of Java class
    private static native void nativeInit();

    static {
        nativeInit();
    }*/

	private static AACFilter singleton;
	public static AACFilter instance() {
		if (singleton == null) singleton = new AACFilter();
		return singleton;
	}

	public AACFilter() {
		initialized = false;
	}

	public boolean preprocess(int sampleRate, int channelCount, int bitrate, boolean sbr_enabled) {
		if (initialized)
			return true;

		this.sampleRate = sampleRate;
		this.channelCount = channelCount;
		this.bitrate = bitrate;

		byte[] asc = null;
		try {
            MediaFormat mediaFormat = MediaFormat.createAudioFormat("audio/mp4a-latm", sampleRate, channelCount);
            mediaFormat.setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectELD);
            mediaFormat.setInteger(MediaFormat.KEY_BIT_RATE, bitrate);

            // Configuring the SBR for AAC requires API lvl 21, which is not out there right now (Oct 2014)
            // AAC is supposed to autoconfigure itself, but we should react to fmtp, so activate this when
            // the API is here.
            // if( sbr_enabled ) {
            //      mediaFormat.setInteger(MediaFormat.KEY_AAC_SBR_MODE, 2);
            // }

			encoder = MediaCodec.createByCodecName("OMX.google.aac.encoder");
			encoder.configure(mediaFormat, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);

			encoder.start();

			encoderBufferInfo = new MediaCodec.BufferInfo();

			int ascPollCount = 0;
			while (asc == null && ascPollCount < 1000) {
				// Try to get the asc
				int encInBufIdx = -1;
				encInBufIdx = encoder.dequeueOutputBuffer(encoderBufferInfo, 0);
				if (encInBufIdx >= 0) {
					if (encoderBufferInfo.flags == MediaCodec.BUFFER_FLAG_CODEC_CONFIG) {
						asc = new byte[encoderBufferInfo.size];
						encoder.getOutputBuffers()[encInBufIdx].get(asc, 0, encoderBufferInfo.size);
						encoder.getOutputBuffers()[encInBufIdx].position(0);
						encoder.releaseOutputBuffer(encInBufIdx, false);
					}
				}
				ascPollCount++;
			}
			encoderOutputBuffers= encoder.getOutputBuffers();
			encoderInputBuffers= encoder.getInputBuffers();
			if (asc == null) {
				Log.e("Sigh, failed to read asc from encoder");
			}
		} catch (Exception exc) {
			Log.e(exc, "Unable to create AAC Encoder");
			return false;
		}
		Log.i("AAC encoder initialized");

		try {
			MediaFormat mediaFormat = null;
			if (asc != null) {
				mediaFormat = MediaFormat.createAudioFormat("audio/mp4a-latm", 0, 0);
				ByteBuffer ascBuf = ByteBuffer.wrap(asc);
				/* csd-0 = codec specific data */
				mediaFormat.setByteBuffer("csd-0", ascBuf);
			} else {
				mediaFormat = MediaFormat.createAudioFormat("audio/mp4a-latm", sampleRate, channelCount);
				mediaFormat.setInteger(MediaFormat.KEY_BIT_RATE, bitrate);
			}

			decoder = MediaCodec.createByCodecName("OMX.google.aac.decoder");
			decoder.configure(mediaFormat, null, null, 0);
			decoder.start();

			decoderOutputBuffers= decoder.getOutputBuffers();
			decoderInputBuffers= decoder.getInputBuffers();

			decoderBufferInfo = new MediaCodec.BufferInfo();
		} catch (Exception exc) {
			Log.e(exc, "Unable to create AAC Decoder");
			return false;
		}

		Log.i("AAC decoder initialized");
		initialized = true;

		return true;
	}

	public boolean pushToDecoder(byte[] data, int size) {
		try {
			if (data != null && decoder != null) {
				/* request available input buffer (0 == no wait) */
				return queueData(decoder, decoderInputBuffers, data, size);
			} else {
				return false;
			}
		} catch (Exception e) {
			Log.e(e, "Push to decoder failed");
			return false;
		}
	}

	public int pullFromDecoder(byte[] b) {
		try {
			/* read available decoded data */
			int result = dequeueData(decoder, decoderOutputBuffers, decoderBufferInfo, b);
			if (result == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
				decoderOutputBuffers = decoder.getOutputBuffers();
				return pullFromDecoder(b);
			} else {
				return result;
			}
		} catch (Exception e) {
			return 0;
		}
	}

	public boolean pushToEncoder(byte[] data, int size) {
		try {
			if (data != null && encoder != null) {
				/* request available input buffer (0 == no wait) */
				return queueData(encoder, encoderInputBuffers, data, size);
			} else {
				return false;
			}
		} catch (Exception e) {
			Log.e(e, "Push to encoder failed");
			return false;
		}
	}

	public int pullFromEncoder(byte[] b) {
		try {
			/* read available decoded data */
			int result = dequeueData(encoder, encoderOutputBuffers, encoderBufferInfo, b);
			if (result == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
				encoderOutputBuffers = encoder.getOutputBuffers();
				return pullFromDecoder(b);
			} else {
				return result;
			}
		} catch (Exception e) {
			return 0;
		}
	}

	public boolean postprocess() {
		if (initialized) {
			encoder.flush();
			Log.i("Stopping encoder");
			encoder.stop();
			Log.i("Stopping decoder");
			decoder.flush();
			decoder.stop();
			Log.i("Release encoder");
			encoder.release();
			Log.i("Release decoder");
			decoder.release();
			encoder = null;
			decoder = null;
			initialized = false;
		}
		return true;
	}

	static private boolean queueData(MediaCodec codec, ByteBuffer[] inputBuffers, byte[] data, int size) {
		int bufdex = codec.dequeueInputBuffer(0);
		if (bufdex >= 0) {
			inputBuffers[bufdex].position(0);
			inputBuffers[bufdex].put(data, 0, size);
			codec.queueInputBuffer(bufdex, 0, size, 0, 0);
			return true;
		} else {
			return false;
		}
	}
	static private int dequeueData(MediaCodec codec, ByteBuffer[] ouputBuffers, BufferInfo bufferInfo, byte[] b) {
		int pcmbufPollCount = 0;
		while (pcmbufPollCount < 1) {
			int decBufIdx = codec.dequeueOutputBuffer(bufferInfo, 100);

			if (decBufIdx >= 0) {
				if (b.length < bufferInfo.size)
					Log.e("array is too small " + b.length + " < " + bufferInfo.size);
				if (bufferInfo.flags == MediaCodec.BUFFER_FLAG_CODEC_CONFIG) {
					Log.i("JUST READ MediaCodec.BUFFER_FLAG_CODEC_CONFIG buffer");

				}
				ouputBuffers[decBufIdx].get(b, 0, bufferInfo.size);
				ouputBuffers[decBufIdx].position(0);

				codec.releaseOutputBuffer(decBufIdx, false);
				return bufferInfo.size;
			} else if (decBufIdx == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
				return MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED;
			} else if (decBufIdx == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED) {
				Log.i("MediaCodec.INFO_OUTPUT_FORMAT_CHANGED");
				Log.i("CHANNEL_COUNT: " + codec.getOutputFormat().getInteger(MediaFormat.KEY_CHANNEL_COUNT));
				Log.i("SAMPLE_RATE: " + codec.getOutputFormat().getInteger(MediaFormat.KEY_SAMPLE_RATE));

			} else if (decBufIdx == MediaCodec.INFO_TRY_AGAIN_LATER) {
				// Log.i("MediaCodec.INFO_TRY_AGAIN_LATER");
			}
			++pcmbufPollCount;
		}
		return 0;
	}
}
