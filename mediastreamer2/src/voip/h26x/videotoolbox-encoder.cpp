/*
 Mediastreamer2 videotoolbox-encoder.cpp
 Copyright (C) 2018 Belledonne Communications SARL

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

#include "h26x/h26x-utils.h"
#include "videotoolbox-utils.h"

#include "videotoolbox-encoder.h"

#define VTH264_ENC_NAME "VideoToolboxH264Encoder"
#define vth264enc_log(level, fmt, ...) ms_##level(VTH264_ENC_NAME ": " fmt, ##__VA_ARGS__)
#define vth264enc_message(fmt, ...) vth264enc_log(message, fmt, ##__VA_ARGS__)
#define vth264enc_warning(fmt, ...) vth264enc_log(warning, fmt, ##__VA_ARGS__)
#define vth264enc_error(fmt, ...) vth264enc_log(error, fmt, ##__VA_ARGS__)

using namespace std;

namespace mediastreamer {

VideoToolboxEncoder::Frame::Frame(Frame &&src) {
	ms_queue_init(&_nalus);
	while (mblk_t *m = ms_queue_get(&src._nalus)) {
		ms_queue_put(&_nalus, m);
	}
}

void VideoToolboxEncoder::Frame::insert(MSQueue *q) {
	mblk_t *insertionPoint = ms_queue_peek_first(&_nalus);
	while (mblk_t *m = ms_queue_get(q)) {
		ms_queue_insert(&_nalus, insertionPoint, m);
	}
}

VideoToolboxEncoder::VideoToolboxEncoder(const string &mime): H26xEncoder(mime) {
	_vsize.width = 0;
	_vsize.height = 0;
	ms_mutex_init(&_mutex, nullptr);
}

void VideoToolboxEncoder::setFps(float fps) {
	float oldFramerate = _framerate;
	try {
		_framerate = fps;
		if (isRunning()) applyFramerate();
	} catch (const runtime_error &e) {
		_framerate = oldFramerate;
		throw;
	}
}

void VideoToolboxEncoder::setBitrate(int bitrate) {
	int oldBitrate = _bitrate;
	try {
		_bitrate = bitrate;
		if (isRunning()) applyBitrate();
	} catch (const runtime_error &e) {
		_bitrate = oldBitrate;
		throw;
	}
}

void VideoToolboxEncoder::start() {
	try {
		OSStatus err;
		CFNumberRef value;

		unique_ptr<VideoToolboxUtilities> utils(VideoToolboxUtilities::create(_mime));

		CFMutableDictionaryRef pixbuf_attr = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, NULL, &kCFTypeDictionaryValueCallBacks);
		int32_t pixel_type = kCVPixelFormatType_420YpCbCr8Planar;
		value = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pixel_type);
		CFDictionarySetValue(pixbuf_attr, kCVPixelBufferPixelFormatTypeKey, value);
		CFRelease(value);

		CFMutableDictionaryRef session_props = CFDictionaryCreateMutable (kCFAllocatorDefault, 1, NULL, NULL);
#if !TARGET_OS_IPHONE
		CFDictionarySetValue(session_props, kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder, kCFBooleanTrue);
#endif

		err = VTCompressionSessionCreate(kCFAllocatorDefault, _vsize.width, _vsize.height, utils->getCodecType(),
										 session_props, pixbuf_attr, kCFAllocatorDefault, outputCb, this, &_session);
		CFRelease(pixbuf_attr);
		CFRelease(session_props);
		if(err) throw runtime_error("could not initialize the VideoToolbox compresson session: " + toString(err));

		err = VTSessionSetProperty(_session, kVTCompressionPropertyKey_ProfileLevel, utils->getDefaultProfileLevel());
		if (err != noErr) {
			vth264enc_error("could not set H264 profile and level: %s", toString(err).c_str());
		}

		err = VTSessionSetProperty(_session, kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);
		if (err != noErr) {
			vth264enc_warning("could not enable real-time mode: %s", toString(err).c_str());
		}

		applyFramerate();
		applyBitrate();

		if((err = VTCompressionSessionPrepareToEncodeFrames(_session)) != noErr) {
			throw runtime_error("could not prepare the VideoToolbox compression session: " + toString(err));
		}

		vth264enc_message("encoder succesfully initialized.");
#if !TARGET_OS_IPHONE
		CFBooleanRef hardware_acceleration_enabled;
		err = VTSessionCopyProperty(_session, kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder, kCFAllocatorDefault, &hardware_acceleration_enabled);
		if (err != noErr) {
			vth264enc_error("could not read kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder property: %s", toString(err).c_str());
		} else {
			if (hardware_acceleration_enabled != nullptr && CFBooleanGetValue(hardware_acceleration_enabled)) {
				vth264enc_message("hardware acceleration enabled");
			} else {
				vth264enc_warning("hardware acceleration not enabled");
			}
		}
		if (hardware_acceleration_enabled) CFRelease(hardware_acceleration_enabled);
#endif
		return;
	} catch (const runtime_error &e) {
		vth264enc_error("%s", e.what());
		if(_session) {
			CFRelease(_session);
			_session = nullptr;
		}
	}
}

void VideoToolboxEncoder::stop() {
	if (_session == nullptr) return;
	vth264enc_message("destroying the encoding session");
	VTCompressionSessionInvalidate(_session);
	CFRelease( _session);
	_session = nullptr;
}

void VideoToolboxEncoder::feed(mblk_t *rawData, uint64_t time, bool requestIFrame) {
	YuvBuf src_yuv_frame, dst_yuv_frame = {0};
	CVPixelBufferRef pixbuf = nullptr;
	const int pixbuf_fmt = kCVPixelFormatType_420YpCbCr8Planar;

	ms_yuv_buf_init_from_mblk(&src_yuv_frame, rawData);

	CFMutableDictionaryRef pixbuf_attr = CFDictionaryCreateMutable(nullptr, 0, nullptr, nullptr);
	CFNumberRef value = CFNumberCreate(nullptr, kCFNumberIntType, &pixbuf_fmt);
	CFDictionarySetValue(pixbuf_attr, kCVPixelBufferPixelFormatTypeKey, value);
	CVPixelBufferCreate(nullptr, _vsize.width, _vsize.height, kCVPixelFormatType_420YpCbCr8Planar, pixbuf_attr,  &pixbuf);
	CFRelease(pixbuf_attr);

	CVPixelBufferLockBaseAddress(pixbuf, 0);
	dst_yuv_frame.w = (int)CVPixelBufferGetWidth(pixbuf);
	dst_yuv_frame.h = (int)CVPixelBufferGetHeight(pixbuf);
	for(int i=0; i<3; i++) {
		dst_yuv_frame.planes[i] = static_cast<uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(pixbuf, i));
		dst_yuv_frame.strides[i] = (int)CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i);
	}
	ms_yuv_buf_copy(src_yuv_frame.planes, src_yuv_frame.strides, dst_yuv_frame.planes, dst_yuv_frame.strides, (MSVideoSize){dst_yuv_frame.w, dst_yuv_frame.h});
	CVPixelBufferUnlockBaseAddress(pixbuf, 0);

	CMTime p_time = CMTimeMake(time, 1000);
	CFMutableDictionaryRef frameProperties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr);
	CFDictionarySetValue(frameProperties, kVTEncodeFrameOptionKey_ForceKeyFrame, requestIFrame ? kCFBooleanTrue : kCFBooleanFalse);

	OSStatus err;
	if((err = VTCompressionSessionEncodeFrame(_session, pixbuf, p_time, kCMTimeInvalid, frameProperties, nullptr, nullptr)) != noErr) {
		vth264enc_error("could not pass a pixbuf to the encoder: %s", toString(err).c_str());
		if (err == kVTInvalidSessionErr) {
			stop();
			start();
		}
	}
	CFRelease(pixbuf);
}

bool VideoToolboxEncoder::fetch(MSQueue *encodedData) {
	ms_mutex_lock(&_mutex);
	if (_encodedFrames.empty()) {
		ms_mutex_unlock(&_mutex);
		return false;
	}
	Frame &frame = _encodedFrames.front();
	while (mblk_t *m = frame.get()) {
		ms_queue_put(encodedData, m);
	}
	_encodedFrames.pop_front();
	ms_mutex_unlock(&_mutex);
	return true;
}

void VideoToolboxEncoder::applyFramerate() {
	CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &_framerate);
	OSStatus status = VTSessionSetProperty(_session, kVTCompressionPropertyKey_ExpectedFrameRate, value);
	CFRelease(value);
	if (status != noErr) {
		ostringstream msg;
		msg << "error while setting kVTCompressionPropertyKey_ExpectedFrameRate: " << toString(status);
		throw runtime_error(msg.str());
	}
}

void VideoToolboxEncoder::applyBitrate() {
	OSStatus status;

	CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &_bitrate);
	status = VTSessionSetProperty(_session, kVTCompressionPropertyKey_AverageBitRate, value);
	CFRelease(value);
	if (status != noErr) {
		ostringstream msg;
		msg << "error while setting kVTCompressionPropertyKey_AverageBitRate: " << toString(status);
		throw runtime_error(msg.str());
	}

	int bytes_per_seconds = _bitrate / 8;
	int dur = 1;
	CFNumberRef bytes_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &bytes_per_seconds);
	CFNumberRef duration_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &dur);
	CFMutableArrayRef data_rate_limits = CFArrayCreateMutable(kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(data_rate_limits, bytes_value);
	CFArrayAppendValue(data_rate_limits, duration_value);
	status = VTSessionSetProperty(_session, kVTCompressionPropertyKey_DataRateLimits, data_rate_limits);
	CFRelease(bytes_value);
	CFRelease(duration_value);
	CFRelease(data_rate_limits);
	if (status != noErr) {
		ostringstream msg;
		msg << "error while setting kVTCompressionPropertyKey_DataRateLimits: " << toString(status);
		throw runtime_error(msg.str());
	}
}

void VideoToolboxEncoder::outputCb(void *outputCallbackRefCon, void *sourceFrameRefCon, OSStatus status, VTEncodeInfoFlags infoFlags, CMSampleBufferRef sampleBuffer) {
	VideoToolboxEncoder *ctx = static_cast<VideoToolboxEncoder *>(outputCallbackRefCon);

	try {
		if (sampleBuffer == nullptr) throw runtime_error("no output buffer");
		if (status != noErr) throw AppleOSError(status);

		if(ctx->_session) {
			Frame encodedFrame;
			CMBlockBufferRef block_buffer = CMSampleBufferGetDataBuffer(sampleBuffer);
			const size_t frame_size = CMBlockBufferGetDataLength(block_buffer);
			size_t read_size = 0;

			while(read_size < frame_size) {
				char *chunk;
				size_t chunk_size;
				OSStatus status = CMBlockBufferGetDataPointer(block_buffer, read_size, &chunk_size, NULL, &chunk);
				if (status != kCMBlockBufferNoErr) throw runtime_error(string("chunk reading failed: ") + toString(status));

				H26xUtils::naluStreamToNalus(reinterpret_cast<uint8_t *>(chunk), chunk_size, encodedFrame.getQueue());
				read_size += chunk_size;
			}

			bool isKeyFrame = false;
			unique_ptr<H26xNaluHeader> header(H26xToolFactory::get(ctx->_mime).createNaluHeader());
			for (const mblk_t *nalu = ms_queue_peek_first(encodedFrame.getQueue()); !ms_queue_end(encodedFrame.getQueue(), nalu); nalu = ms_queue_next(encodedFrame.getQueue(), nalu)) {
				header->parse(nalu->b_rptr);
				if (header->getAbsType().isKeyFramePart()) {
					isKeyFrame = true;
					break;
				}
			}

			if(isKeyFrame) {
				MSQueue parameterSets;
				ms_queue_init(&parameterSets);
				try {
					ms_message("VideoToolboxEncoder: I-frame created");
					unique_ptr<VideoToolboxUtilities> vtUtils(VideoToolboxUtilities::create(ctx->_mime));
					vtUtils->getParameterSets(CMSampleBufferGetFormatDescription(sampleBuffer), &parameterSets);
					encodedFrame.insert(&parameterSets);
				} catch (const AppleOSError &e) {
					ms_error("VideoToolboxEncoder: paramter sets generation failed: %s", e.what());
					ms_queue_flush(&parameterSets);
				}
			}

			ms_mutex_lock(&ctx->_mutex);
			ctx->_encodedFrames.push_back(move(encodedFrame));
			ms_mutex_unlock(&ctx->_mutex);
		}
	} catch (const runtime_error &e) {
		ms_error("VideoToolboxEncoder: decoding error: %s", e.what());
	} catch (const AppleOSError &e) {
		ms_error("VideoToolboxEncoder: decoding error: %s", e.what());
	}
}

}
