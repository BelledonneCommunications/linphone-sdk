/*
 Mediastreamer2 videotoolbox-decoder.cpp
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

#include <vector>

#include "mediastreamer2/msvideo.h"

#include "videotoolbox-utils.h"

#include "videotoolbox-decoder.h"

#define VTH264_DEC_NAME "VideoToolboxH264Decoder"
#define vth264dec_log(level, fmt, ...) ms_##level(VTH264_DEC_NAME ": " fmt, ##__VA_ARGS__)
#define vth264dec_message(fmt, ...) vth264dec_log(message, fmt, ##__VA_ARGS__)
#define vth264dec_warning(fmt, ...) vth264dec_log(warning, fmt, ##__VA_ARGS__)
#define vth264dec_error(fmt, ...) vth264dec_log(error, fmt, ##__VA_ARGS__)
#define vth264dec_debug(fmt, ...) vth264dec_log(debug, fmt, ##__VA_ARGS__)

using namespace std;

namespace mediastreamer {

VideoToolboxDecoder::VideoToolboxDecoder() {
	_pixbufAllocator = ms_yuv_buf_allocator_new();
	ms_mutex_init(&_mutex, nullptr);
	const H26xToolFactory &factory = H26xToolFactory::get("video/avc");
	_psStore.reset(factory.createParameterSetsStore());
	_naluHeader.reset(factory.createNaluHeader());
}

VideoToolboxDecoder::~VideoToolboxDecoder() {
	ms_yuv_buf_allocator_free(_pixbufAllocator);
	if (_session) destroyDecoder();
}

bool VideoToolboxDecoder::feed(MSQueue *encodedFrame, uint64_t timestamp) {
	_psStore->extractAllPs(encodedFrame);
	if (_psStore->hasNewParameters()) {
		_psStore->acknowlege();
		if (_session) destroyDecoder();
	}
	if (ms_queue_empty(encodedFrame)) return true;
	if (!_psStore->psGatheringCompleted()) {
		vth264dec_message("decoding failed because some parameter sets haven't been received yet");
		return false;
	}
	if (_session == nullptr && !createDecoder()) return false;
	for (const mblk_t *nalu = ms_queue_peek_first(encodedFrame); !ms_queue_end(encodedFrame, nalu); nalu = ms_queue_next(encodedFrame, nalu)) {
		_naluHeader->parse(nalu->b_rptr);
		if (_naluHeader->getAbsType().isKeyFramePart()) {
			_freeze = false;
			break;
		}
	}
	if (_freeze) return true;
	return decodeFrame(encodedFrame, timestamp);
}

VideoDecoder::Status VideoToolboxDecoder::fetch(mblk_t *&frame) {
	if (_queue.empty()) {
		frame = nullptr;
		return noFrameAvailable;
	} else {
		frame = _queue.front().getData();
		_queue.pop_front();
		return frame ? noError : decodingFailure;
	}
}

bool VideoToolboxDecoder::createDecoder() {
	OSStatus status;
	VTDecompressionOutputCallbackRecord dec_cb = {outputCb, this};

	vth264dec_message("creating a decoding session");

	if (!formatDescFromSpsPps()) {
		return false;
	}

	CFMutableDictionaryRef decoder_params = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, NULL, NULL);
#if !TARGET_OS_IPHONE
	CFDictionarySetValue(decoder_params, kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder, kCFBooleanTrue);
#endif

	CFMutableDictionaryRef pixel_parameters = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, NULL, &kCFTypeDictionaryValueCallBacks);
	int32_t format = kCVPixelFormatType_420YpCbCr8Planar;
	CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &format);
	CFDictionarySetValue(pixel_parameters, kCVPixelBufferPixelFormatTypeKey, value);
	CFRelease(value);

	status = VTDecompressionSessionCreate(kCFAllocatorDefault, _formatDesc, decoder_params, pixel_parameters, &dec_cb, &_session);
	CFRelease(pixel_parameters);
	CFRelease(decoder_params);
	if(status != noErr) {
		vth264dec_error("could not create the decoding context: %s", toString(status).c_str());
		return false;
	} else {
#if !TARGET_OS_IPHONE
		CFBooleanRef hardware_acceleration;
		status = VTSessionCopyProperty(_session, kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder, kCFAllocatorDefault, &hardware_acceleration);
		if (status != noErr) {
			vth264dec_error("could not read kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder property: %s", toString(status).c_str());
		} else {
			if (hardware_acceleration != NULL && CFBooleanGetValue(hardware_acceleration)) {
				vth264dec_message("hardware acceleration enabled");
			} else {
				vth264dec_warning("hardware acceleration not enabled");
			}
		}
		if (hardware_acceleration != NULL) CFRelease(hardware_acceleration);
#endif

#if TARGET_OS_IPHONE // kVTDecompressionPropertyKey_RealTime is only available on MacOSX after 10.10 version
		status = VTSessionSetProperty(_session, kVTDecompressionPropertyKey_RealTime, kCFBooleanTrue);
		if (status != noErr) {
			vth264dec_warning("could not be able to switch to real-time mode: %s", toString(status).c_str());
		}
#endif

		return true;
	}
}

void VideoToolboxDecoder::destroyDecoder() {
	vth264dec_message("destroying decoder");
	VTDecompressionSessionInvalidate(_session);
	CFRelease(_session);
	CFRelease(_formatDesc);
	_session = nullptr;
	_formatDesc = nullptr;
}

bool VideoToolboxDecoder::decodeFrame(MSQueue *encodedFrame, uint64_t timestamp) {
	CMBlockBufferRef stream = nullptr;
	OSStatus status = CMBlockBufferCreateEmpty(kCFAllocatorDefault, 0, kCMBlockBufferAssureMemoryNowFlag, &stream);
	if (status != kCMBlockBufferNoErr) {
		vth264dec_error("failure while creating input buffer for decoder");
		return false;
	}
	while(mblk_t *nalu = ms_queue_get(encodedFrame)) {
		CMBlockBufferRef nalu_block;
		size_t nalu_block_size = msgdsize(nalu) + _naluSizeLength;
		uint32_t nalu_size = htonl(msgdsize(nalu));

		CMBlockBufferCreateWithMemoryBlock(NULL, NULL, nalu_block_size, NULL, NULL, 0, nalu_block_size, kCMBlockBufferAssureMemoryNowFlag, &nalu_block);
		CMBlockBufferReplaceDataBytes(&nalu_size, nalu_block, 0, _naluSizeLength);
		CMBlockBufferReplaceDataBytes(nalu->b_rptr, nalu_block, _naluSizeLength, msgdsize(nalu));
		CMBlockBufferAppendBufferReference(stream, nalu_block, 0, nalu_block_size, 0);
		CFRelease(nalu_block);
		freemsg(nalu);
	}
	if(!CMBlockBufferIsEmpty(stream)) {
		CMSampleBufferRef sample = NULL;
		CMSampleTimingInfo timing_info;
		timing_info.duration = kCMTimeInvalid;
		timing_info.presentationTimeStamp = CMTimeMake(timestamp, 1000);
		timing_info.decodeTimeStamp = CMTimeMake(timestamp, 1000);
		CMSampleBufferCreate(
					kCFAllocatorDefault, stream, TRUE, NULL, NULL,
					_formatDesc, 1, 1, &timing_info,
					0, NULL, &sample);

		status = VTDecompressionSessionDecodeFrame(_session, sample, kVTDecodeFrame_EnableAsynchronousDecompression | kVTDecodeFrame_1xRealTimePlayback, NULL, NULL);
		CFRelease(sample);
		if(status != noErr) {
			vth264dec_error("error while passing encoded frames to the decoder: %s", toString(status).c_str());
			CFRelease(stream);
			return false;
		}
	}
	CFRelease(stream);
	return true;
}

bool VideoToolboxDecoder::formatDescFromSpsPps() {
	MSQueue parameterSets;
	ms_queue_init(&parameterSets);
	_psStore->fetchAllPs(&parameterSets);

	vector<const uint8_t *> ptrs;
	vector<size_t> sizes;
	for (const mblk_t *ps = ms_queue_peek_first(&parameterSets); !ms_queue_end(&parameterSets, ps); ps = ms_queue_next(&parameterSets, ps)) {
		ptrs.push_back(ps->b_rptr);
		sizes.push_back(msgdsize(ps));
	}

	CMFormatDescriptionRef format_desc;
	OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(NULL, ptrs.size(), ptrs.data(), sizes.data(), _naluSizeLength, &format_desc);
	ms_queue_flush(&parameterSets);
	if(status != noErr) {
		vth264dec_error("could not find out the input format: %d", int(status));
		return false;
	}

	CMVideoDimensions vsize = CMVideoFormatDescriptionGetDimensions(format_desc);
	vth264dec_message("new video format %dx%d", int(vsize.width), int(vsize.height));
	if (_formatDesc) CFRelease(_formatDesc);
	_formatDesc = format_desc;
	return true;
}

void VideoToolboxDecoder::outputCb(void *decompressionOutputRefCon, void *sourceFrameRefCon, OSStatus status,
								   VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer,
								   CMTime presentationTimeStamp, CMTime presentationDuration) {
	auto ctx = static_cast<VideoToolboxDecoder *>(decompressionOutputRefCon);

	ms_mutex_lock(&ctx->_mutex);

	if(status != noErr || imageBuffer == nullptr) {
		vth264dec_error("fail to decode one frame: %s", toString(status).c_str());
		ctx->_queue.push_back(Frame());
		ms_mutex_unlock(&ctx->_mutex);
		return;
	}

	MSPicture pixbuf_desc;
	CGSize vsize = CVImageBufferGetEncodedSize(imageBuffer);
	mblk_t *pixbuf = ms_yuv_buf_allocator_get(ctx->_pixbufAllocator, &pixbuf_desc, int(vsize.width), int(vsize.height));

	uint8_t *src_planes[4] = {0};
	int src_strides[4] = {0};
	CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
	for(size_t i=0; i<3; i++) {
		src_planes[i] = static_cast<uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(imageBuffer, i));
		src_strides[i] = (int)CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, i);
	}
	ms_yuv_buf_copy(src_planes, src_strides, pixbuf_desc.planes, pixbuf_desc.strides, {int(vsize.width), int(vsize.height)});
	CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

	ctx->_queue.push_back(Frame(pixbuf));
	ms_mutex_unlock(&ctx->_mutex);
}

}
