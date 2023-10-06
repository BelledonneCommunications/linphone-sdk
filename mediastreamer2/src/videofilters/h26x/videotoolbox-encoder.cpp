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

#include <bctoolbox/defs.h>

#include "h26x-utils.h"
#include "videotoolbox-utils.h"

#include "videotoolbox-encoder.h"

#define vt_enc_log(level, fmt, ...) ms_##level("VideoToolboxEncoder: " fmt, ##__VA_ARGS__)
#define vt_enc_message(fmt, ...) vt_enc_log(message, fmt, ##__VA_ARGS__)
#define vt_enc_warning(fmt, ...) vt_enc_log(warning, fmt, ##__VA_ARGS__)
#define vt_enc_error(fmt, ...) vt_enc_log(error, fmt, ##__VA_ARGS__)

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

VideoToolboxEncoder::VideoToolboxEncoder(const string &mime, int payloadMaxSize) : H26xEncoder(mime) {
	_payloadMaxSize = payloadMaxSize;
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

		CFMutableDictionaryRef pixbuf_attr =
		    CFDictionaryCreateMutable(kCFAllocatorDefault, 1, NULL, &kCFTypeDictionaryValueCallBacks);
		int32_t pixel_type = kCVPixelFormatType_420YpCbCr8Planar;
		value = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pixel_type);
		CFDictionarySetValue(pixbuf_attr, kCVPixelBufferPixelFormatTypeKey, value);
		CFRelease(value);

		CFMutableDictionaryRef session_props = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, NULL, NULL);
#if !TARGET_OS_IPHONE
		CFDictionarySetValue(session_props, kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder,
		                     kCFBooleanTrue);
#endif

		err = VTCompressionSessionCreate(kCFAllocatorDefault, _vsize.width, _vsize.height, utils->getCodecType(),
		                                 session_props, pixbuf_attr, kCFAllocatorDefault, outputCb, this, &_session);
		CFRelease(pixbuf_attr);
		CFRelease(session_props);
		if (err) throw runtime_error("could not initialize the VideoToolbox compresson session: " + toString(err));

		err = VTSessionSetProperty(_session, kVTCompressionPropertyKey_ProfileLevel, utils->getDefaultProfileLevel());
		if (err != noErr) {
			vt_enc_error("could not set profile and level: %s", toString(err).c_str());
		}

		err = VTSessionSetProperty(_session, kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);
		if (err != noErr) {
			vt_enc_warning("could not enable real-time mode: %s", toString(err).c_str());
		}
		err = VTSessionSetProperty(_session, kVTCompressionPropertyKey_AllowFrameReordering, kCFBooleanFalse);
		if (err != noErr) {
			vt_enc_warning("could not set kVTCompressionPropertyKey_AllowFrameReordering: %s", toString(err).c_str());
		}

		int max_frame_count = 1;
		value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &max_frame_count);

		err = VTSessionSetProperty(_session, kVTCompressionPropertyKey_MaxFrameDelayCount, value);
		if (err != noErr) {
			vt_enc_warning("could not set kVTCompressionPropertyKey_MaxFrameDelayCount: %s", toString(err).c_str());
		}
		CFRelease(value);

		value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &_payloadMaxSize);
		err = VTSessionSetProperty(_session, kVTCompressionPropertyKey_MaxH264SliceBytes, value);
		if (err != noErr) {
			vt_enc_warning("could not set kVTCompressionPropertyKey_MaxH264SliceBytes: %s", toString(err).c_str());
		}
		CFRelease(value);

		applyFramerate();
		applyBitrate();

		if ((err = VTCompressionSessionPrepareToEncodeFrames(_session)) != noErr) {
			throw runtime_error("could not prepare the VideoToolbox compression session: " + toString(err));
		}

		vt_enc_message("encoder succesfully initialized.");
#if !TARGET_OS_IPHONE
		CFBooleanRef hardware_acceleration_enabled;
		err = VTSessionCopyProperty(_session, kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder,
		                            kCFAllocatorDefault, &hardware_acceleration_enabled);
		if (err != noErr) {
			vt_enc_error("could not read kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder property: %s",
			             toString(err).c_str());
		} else {
			if (hardware_acceleration_enabled != nullptr && CFBooleanGetValue(hardware_acceleration_enabled)) {
				vt_enc_message("hardware acceleration enabled");
			} else {
				vt_enc_warning("hardware acceleration not enabled");
			}
		}
		if (hardware_acceleration_enabled) CFRelease(hardware_acceleration_enabled);
#endif
		return;
	} catch (const runtime_error &e) {
		vt_enc_error("%s", e.what());
		if (_session) {
			CFRelease(_session);
			_session = nullptr;
		}
	}
}

void VideoToolboxEncoder::stop() {
	if (_session == nullptr) return;
	vt_enc_message("destroying the encoding session");
	VTCompressionSessionInvalidate(_session);
	CFRelease(_session);
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
	CVPixelBufferCreate(nullptr, _vsize.width, _vsize.height, kCVPixelFormatType_420YpCbCr8Planar, pixbuf_attr,
	                    &pixbuf);
	CFRelease(pixbuf_attr);

	CVPixelBufferLockBaseAddress(pixbuf, 0);
	dst_yuv_frame.w = (int)CVPixelBufferGetWidth(pixbuf);
	dst_yuv_frame.h = (int)CVPixelBufferGetHeight(pixbuf);
	for (int i = 0; i < 3; i++) {
		dst_yuv_frame.planes[i] = static_cast<uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(pixbuf, i));
		dst_yuv_frame.strides[i] = (int)CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i);
	}
	ms_yuv_buf_copy(src_yuv_frame.planes, src_yuv_frame.strides, dst_yuv_frame.planes, dst_yuv_frame.strides,
	                (MSVideoSize){dst_yuv_frame.w, dst_yuv_frame.h});
	CVPixelBufferUnlockBaseAddress(pixbuf, 0);

	CMTime p_time = CMTimeMake(time, 1000);
	CFMutableDictionaryRef frameProperties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr);
	CFDictionarySetValue(frameProperties, kVTEncodeFrameOptionKey_ForceKeyFrame,
	                     requestIFrame ? kCFBooleanTrue : kCFBooleanFalse);

	OSStatus err;
	if ((err = VTCompressionSessionEncodeFrame(_session, pixbuf, p_time, kCMTimeInvalid, frameProperties, nullptr,
	                                           nullptr)) != noErr) {
		vt_enc_error("could not pass a pixbuf to the encoder: %s", toString(err).c_str());
		if (err == kVTInvalidSessionErr) {
			stop();
			start();
		}
	}
	CFRelease(pixbuf);
	CFRelease(frameProperties);
	freemsg(rawData);
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

	vt_enc_message("setting output bitrate to [%i] bits/s", _bitrate);
	CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &_bitrate);
	status = VTSessionSetProperty(_session, kVTCompressionPropertyKey_AverageBitRate, value);
	CFRelease(value);
	if (status != noErr) {
		ostringstream msg;
		msg << "error while setting kVTCompressionPropertyKey_AverageBitRate: " << toString(status);
		vt_enc_error("%s : %i bits/s", msg.str().c_str(), _bitrate);
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
		vt_enc_error("%s", msg.str().c_str());
		throw runtime_error(msg.str());
	}
}

void VideoToolboxEncoder::outputCb(void *outputCallbackRefCon,
                                   BCTBX_UNUSED(void *sourceFrameRefCon),
                                   OSStatus status,
                                   BCTBX_UNUSED(VTEncodeInfoFlags infoFlags),
                                   CMSampleBufferRef sampleBuffer) {
	VideoToolboxEncoder *ctx = static_cast<VideoToolboxEncoder *>(outputCallbackRefCon);

	try {
		if (sampleBuffer == nullptr) throw runtime_error("no output buffer");
		if (status != noErr) throw AppleOSError(status);

		if (ctx->_session) {
			Frame encodedFrame;
			CMBlockBufferRef block_buffer = CMSampleBufferGetDataBuffer(sampleBuffer);
			const size_t frame_size = CMBlockBufferGetDataLength(block_buffer);
			size_t read_size = 0;

			while (read_size < frame_size) {
				char *chunk;
				size_t chunk_size;
				OSStatus status = CMBlockBufferGetDataPointer(block_buffer, read_size, &chunk_size, NULL, &chunk);
				if (status != kCMBlockBufferNoErr)
					throw runtime_error(string("chunk reading failed: ") + toString(status));

				H26xUtils::naluStreamToNalus(reinterpret_cast<uint8_t *>(chunk), chunk_size, encodedFrame.getQueue());
				read_size += chunk_size;
			}

			bool isKeyFrame = false;
			unique_ptr<H26xNaluHeader> header(H26xToolFactory::get(ctx->_mime).createNaluHeader());
			for (const mblk_t *nalu = ms_queue_peek_first(encodedFrame.getQueue());
			     !ms_queue_end(encodedFrame.getQueue(), nalu); nalu = ms_queue_next(encodedFrame.getQueue(), nalu)) {
				header->parse(nalu->b_rptr);
				if (header->getAbsType().isKeyFramePart()) {
					isKeyFrame = true;
					break;
				}
			}

			if (isKeyFrame) {
				MSQueue parameterSets;
				ms_queue_init(&parameterSets);
				try {
					ms_message("VideoToolboxEncoder: I-frame created");
					unique_ptr<VideoToolboxUtilities> vtUtils(VideoToolboxUtilities::create(ctx->_mime));
					vtUtils->getParameterSets(CMSampleBufferGetFormatDescription(sampleBuffer), &parameterSets);
					encodedFrame.insert(&parameterSets);
				} catch (const AppleOSError &e) {
					ms_error("VideoToolboxEncoder: parameter sets generation failed: %s", e.what());
					ms_queue_flush(&parameterSets);
				}
			}

			ms_mutex_lock(&ctx->_mutex);
			ctx->_encodedFrames.push_back(std::move(encodedFrame));
			ms_mutex_unlock(&ctx->_mutex);
		}
	} catch (const runtime_error &e) {
		ms_error("VideoToolboxEncoder: encoding error: %s", e.what());
	} catch (const AppleOSError &e) {
		ms_error("VideoToolboxEncoder: encoding error: %s", e.what());
	}
}

} // namespace mediastreamer
