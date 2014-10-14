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
	MediaCodec decoder;
	BufferInfo decoderBufferInfo;
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
	
	public boolean preprocess(int sampleRate, int channelCount, int bitrate) {
		if (initialized)
			return true;
		Log.i("Init: sampleRate=" + sampleRate + ", channel=" + channelCount + ", br=" + bitrate);
		
		this.sampleRate = sampleRate;
		this.channelCount = channelCount;
		this.bitrate = bitrate;
		
		byte[] asc = null;
		try {
			MediaFormat mediaFormat = MediaFormat.createAudioFormat("audio/mp4a-latm", sampleRate, channelCount);
			mediaFormat.setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectELD);
			mediaFormat.setInteger(MediaFormat.KEY_BIT_RATE, bitrate);
			
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
			
			
			// MediaCodecList.getCodecInfoAt(0);
			decoder = MediaCodec.createByCodecName("OMX.google.aac.decoder");
			decoder.configure(mediaFormat, null, null, 0);
			Log.i("DECODER SAMPLE RATE:" + mediaFormat.getInteger(MediaFormat.KEY_SAMPLE_RATE));
			decoder.start();
			
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
				return queueData(decoder, data, size);
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
			return dequeueData(decoder, decoderBufferInfo, b);
		} catch (Exception e) {
			return 0;
		}
	}
	
	public boolean pushToEncoder(byte[] data, int size) {
		try {
			if (data != null && encoder != null) {
				/* request available input buffer (0 == no wait) */
				return queueData(encoder, data, size);
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
			return dequeueData(encoder, encoderBufferInfo, b);
		} catch (Exception e) {
			return 0;
		}
	}
			
	public boolean postprocess() {
		if (initialized) {
			Log.i("Stopping encoder");
			encoder.stop();
			Log.i("Stopping decoder");
			decoder.stop();
			Log.i("Release encoder");
			// crashes encoder.release();
			Log.i("Release decoder");
			// crashes decoder.release();
			encoder = null;
			decoder = null;
			initialized = false;
		}
		return true;
	}
	
	static private boolean queueData(MediaCodec codec, byte[] data, int size) {
		int bufdex = codec.dequeueInputBuffer(0);
		if (bufdex >= 0) {
			codec.getInputBuffers()[bufdex].position(0);
			codec.getInputBuffers()[bufdex].put(data, 0, size);
			codec.queueInputBuffer(bufdex, 0, size, 0, 0);
			return true;
		} else {
			return false;
		}
	}
	static private int dequeueData(MediaCodec codec, BufferInfo bufferInfo, byte[] b) {
		int pcmbufPollCount = 0;
		while (pcmbufPollCount < 1) {
			int decBufIdx = codec.dequeueOutputBuffer(bufferInfo, 0);
			if (decBufIdx >= 0) {
				if (b.length < bufferInfo.size)
					Log.e("array is too small " + b.length + " < " + bufferInfo.size);
				if (bufferInfo.flags == MediaCodec.BUFFER_FLAG_CODEC_CONFIG) {
					Log.i("JUST READ MediaCodec.BUFFER_FLAG_CODEC_CONFIG buffer");

				}
				codec.getOutputBuffers()[decBufIdx].get(b, 0, bufferInfo.size);
				codec.getOutputBuffers()[decBufIdx].position(0);
				codec.releaseOutputBuffer(decBufIdx, false);
				return bufferInfo.size;
			} else if (decBufIdx == MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED) {
				Log.i("MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED");
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
