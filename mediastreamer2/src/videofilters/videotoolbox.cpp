/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2016  Belledonne Communications SARL

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

#include <sstream>
#include <string>

#include <VideoToolbox/VideoToolbox.h>

#include "mediastreamer2/mscodecutils.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/rfc3984.h"

#include "encoding-filter-impl.h"
#include "filter-wrapper/encoding-filter-wrapper.h"
#include "filter-wrapper/decoding-filter-wrapper.h"
#include "h26x/h264-utils.h"


#define VTH264_ENC_NAME "VideoToolboxH264Encoder"
#define vth264enc_log(level, fmt, ...) ms_##level(VTH264_ENC_NAME ": " fmt, ##__VA_ARGS__)
#define vth264enc_message(fmt, ...) vth264enc_log(message, fmt, ##__VA_ARGS__)
#define vth264enc_warning(fmt, ...) vth264enc_log(warning, fmt, ##__VA_ARGS__)
#define vth264enc_error(fmt, ...) vth264enc_log(error, fmt, ##__VA_ARGS__)

using namespace std;

namespace mediastreamer {

const MSVideoConfiguration vth264enc_video_confs[] = {
	MS_VIDEO_CONF(1536000,  2560000, SXGA_MINUS, 25, 2),
	MS_VIDEO_CONF( 800000,  2000000,       720P, 25, 2),
	MS_VIDEO_CONF( 800000,  1536000,        XGA, 25, 2),
	MS_VIDEO_CONF( 600000,  1024000,       SVGA, 25, 2),
	MS_VIDEO_CONF( 800000,  3000000,        VGA, 30, 2),
	MS_VIDEO_CONF( 400000,   800000,        VGA, 15, 1),
	MS_VIDEO_CONF( 200000,   350000,        CIF, 18, 1),
	MS_VIDEO_CONF( 150000,   200000,       QVGA, 15, 1),
	MS_VIDEO_CONF( 100000,   150000,       QVGA, 10, 1),
	MS_VIDEO_CONF(  64000,   100000,       QCIF, 12, 1),
	MS_VIDEO_CONF(      0,    64000,       QCIF,  5 ,1)
};

std::string toString(::OSStatus status) {
	ostringstream message;
	switch(status) {
	case noErr:
		message << "no error";
		break;
	case kVTPropertyNotSupportedErr:
		message << "property not supported";
		break;
	case kVTVideoDecoderMalfunctionErr:
		message << "decoder malfunction";
		break;
	case kVTInvalidSessionErr:
		message << "invalid session";
		break;
	case kVTParameterErr:
		message << "parameter error";
		break;
	case kCVReturnAllocationFailed:
		message << "return allocation failed";
		break;
	case kVTVideoDecoderBadDataErr:
		message << "decoding bad data";
		break;
	default:
		break;
	}
	message << " [osstatus=" << int(status) << "]";
	return message.str();
}

class VideoToolboxH264EncoderFilterImpl: public EncodingFilterImpl {
public:
	VideoToolboxH264EncoderFilterImpl(MSFilter *f): EncodingFilterImpl(f) {
		ms_mutex_init(&_mutex, NULL);
		ms_queue_init(&_queue);
		_videoConfs = vth264enc_video_confs;
		_conf = ms_video_find_best_configuration_for_size(_videoConfs, MS_VIDEO_SIZE_CIF, ms_factory_get_cpu_count(getFactory()));
		_frameProperties = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, NULL, NULL);
		CFDictionarySetValue(_frameProperties, kVTEncodeFrameOptionKey_ForceKeyFrame, kCFBooleanFalse);
	}

	~VideoToolboxH264EncoderFilterImpl() {
		CFRelease(_frameProperties);
	}

	void preprocess() override {
		createEncodingSession();

		_packerCtx = rfc3984_new_with_factory(getFactory());
		rfc3984_set_mode(_packerCtx, 1);
		rfc3984_enable_stap_a(_packerCtx, FALSE);

		ms_video_starter_init(&_starter);
		ms_iframe_requests_limiter_init(&_iframeLimiter, 1000);
		_firstFrame = TRUE;
	}

	void process() override {
		mblk_t *frame;
		OSStatus err;

		if(_session == NULL) {
			ms_queue_flush(getInput(0));
			return;
		}

		if ((frame = ms_queue_peek_last(getInput(0)))) {
			YuvBuf src_yuv_frame, dst_yuv_frame = {0};
			CVPixelBufferRef pixbuf;
			CFMutableDictionaryRef enc_param = NULL;
			int i, pixbuf_fmt = kCVPixelFormatType_420YpCbCr8Planar;
			CFNumberRef value;
			CFMutableDictionaryRef pixbuf_attr;

			ms_yuv_buf_init_from_mblk(&src_yuv_frame, frame);

			pixbuf_attr = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
			value = CFNumberCreate(NULL, kCFNumberIntType, &pixbuf_fmt);
			CFDictionarySetValue(pixbuf_attr, kCVPixelBufferPixelFormatTypeKey, value);
			CVPixelBufferCreate(NULL, _conf.vsize.width, _conf.vsize.height, kCVPixelFormatType_420YpCbCr8Planar, pixbuf_attr,  &pixbuf);
			CFRelease(pixbuf_attr);

			CVPixelBufferLockBaseAddress(pixbuf, 0);
			dst_yuv_frame.w = (int)CVPixelBufferGetWidth(pixbuf);
			dst_yuv_frame.h = (int)CVPixelBufferGetHeight(pixbuf);
			for(i=0; i<3; i++) {
				dst_yuv_frame.planes[i] = static_cast<uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(pixbuf, i));
				dst_yuv_frame.strides[i] = (int)CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i);
			}
			ms_yuv_buf_copy(src_yuv_frame.planes, src_yuv_frame.strides, dst_yuv_frame.planes, dst_yuv_frame.strides, (MSVideoSize){dst_yuv_frame.w, dst_yuv_frame.h});
			CVPixelBufferUnlockBaseAddress(pixbuf, 0);

			lock();
			if(ms_iframe_requests_limiter_iframe_requested(&_iframeLimiter, getTime())) {
				vth264enc_message("I-frame requested (time=%llu)", getTime());
				CFDictionarySetValue(_frameProperties, kVTEncodeFrameOptionKey_ForceKeyFrame, kCFBooleanTrue);
				ms_iframe_requests_limiter_notify_iframe_sent(&_iframeLimiter, getTime());
			} else {
				CFDictionarySetValue(_frameProperties, kVTEncodeFrameOptionKey_ForceKeyFrame, kCFBooleanFalse);
			}
			unlock();

			if(!_enableAvpf) {
				if(_firstFrame) {
					ms_video_starter_first_frame(&_starter, getTime());
				}
				if(ms_video_starter_need_i_frame(&_starter, getTime())) {
					CFDictionarySetValue(_frameProperties, kVTEncodeFrameOptionKey_ForceKeyFrame, kCFBooleanTrue);
				}
			}

			CMTime p_time = CMTimeMake(getTime(), 1000);
			if((err = VTCompressionSessionEncodeFrame(_session, pixbuf, p_time, kCMTimeInvalid, _frameProperties, NULL, NULL)) != noErr) {
				vth264enc_error("could not pass a pixbuf to the encoder: %s", toString(err).c_str());
				if (err == kVTInvalidSessionErr) {
					destroyEncodingSession();
					createEncodingSession();
				}
			}
			CFRelease(pixbuf);

			_firstFrame = FALSE;

			if(enc_param) CFRelease(enc_param);
		}
		ms_queue_flush(getInput(0));

		lock();
		while ((frame = ms_queue_get(&_queue))) {
			ms_queue_put(getOutput(0), frame);
		}
		unlock();
	}

	void postprocess() override {
		if(_session != NULL) {
			destroyEncodingSession();
		}
		ms_queue_flush(&_queue);
		rfc3984_destroy(_packerCtx);
	}

	MSVideoSize getVideoSize() const override {
		return _conf.vsize;
	}

	void setVideoSize(const MSVideoSize &vsize) override {
		MSVideoConfiguration conf;
		vth264enc_message("requested video size: %dx%d", vsize.width, vsize.height);
		if(_session != NULL) {
			vth264enc_error("could not set video size: encoder is running");
			throw MethodCallFailed();
		}
		conf = ms_video_find_best_configuration_for_size(_videoConfs, vsize, getFactory()->cpu_count);
		_conf.vsize = conf.vsize;
		_conf.fps = conf.fps;
		_conf.bitrate_limit = conf.bitrate_limit;
		if(_conf.required_bitrate > _conf.bitrate_limit) {
			_conf.required_bitrate = _conf.bitrate_limit;
		}
		vth264enc_message("selected video conf: size=%dx%d, framerate=%ffps, bitrate=%dbit/s",
				   _conf.vsize.width, _conf.vsize.height, _conf.fps, _conf.required_bitrate);
	}

	int getBitrate() const override {
		return _conf.required_bitrate;
	}

	void setBitrate(int bitrate) override {
		vth264enc_message("requested bitrate: %d bits/s", bitrate);
		if(_session == NULL) {
			_conf = ms_video_find_best_configuration_for_size_and_bitrate(_videoConfs, _conf.vsize, ms_factory_get_cpu_count(getFactory()), bitrate);
			vth264enc_message("selected video conf: size=%dx%d, framerate=%ffps", _conf.vsize.width, _conf.vsize.height, _conf.fps);
		} else {
			lock();
			_conf.required_bitrate = bitrate;
			setEncodingSessionBItrate(_session, bitrate);
			unlock();
		}
	}

	float getFps() const override {
		return _conf.fps;
	}

	void setFps(float fps) override {
		lock();
		_conf.fps = fps;
		if(_session != NULL) {
			setEncodingSessionFps(_session, fps);
		}
		unlock();
		vth264enc_message("new frame rate target (%ffps)", _conf.fps);
	}

	void requestVfu() override {
		lock();
		ms_video_starter_deactivate(&_starter);
		ms_iframe_requests_limiter_request_iframe(&_iframeLimiter);
		unlock();
	}

	void notifyFir() override {
		requestVfu();
	}

	void notifyPli() override {
		requestVfu();
	}

	void notifySli() override {
		requestVfu();
	}

	void enableAvpf(bool enable_avpf) override {
		if(_session != NULL) {
			vth264enc_error("could not %s AVPF: encoder is running", enable_avpf ? "enable" : "disable");
			throw MethodCallFailed();
		}
		vth264enc_message("%s AVPF", enable_avpf ? "enabling" : "disabling");
		_enableAvpf = enable_avpf;
	}

	const MSVideoConfiguration *getVideoConfigurations() const override {
		return _videoConfs;
	}

	void setVideoConfigurations(const MSVideoConfiguration *conf_list) override {
		_videoConfs = conf_list ? conf_list : vth264enc_video_confs;
		_conf = ms_video_find_best_configuration_for_size(_videoConfs, _conf.vsize, getFactory()->cpu_count);
		vth264enc_message("new video settings: %dx%d, %dbit/s, %ffps",
				   _conf.vsize.width, _conf.vsize.height,
				   _conf.required_bitrate, _conf.fps);
	}

	void setVideoConfiguration(const MSVideoConfiguration *conf) override {
		lock();
		_conf = *conf;
		if(_session != NULL) {
			setEncodingSessionBItrate(_session, _conf.required_bitrate);
			setEncodingSessionFps(_session, _conf.fps);
		}
		unlock();
		vth264enc_message("new video settings: %dx%d, %dbit/s, %ffps",
				   _conf.vsize.width, _conf.vsize.height,
				   _conf.required_bitrate, _conf.fps);
	}

private:
	static void outputCb(VideoToolboxH264EncoderFilterImpl *ctx, void *sourceFrameRefCon, OSStatus status, VTEncodeInfoFlags infoFlags, CMSampleBufferRef sampleBuffer) {
		MSQueue nalu_queue;
		CMBlockBufferRef block_buffer;
		size_t read_size=0, offset=0, frame_size;
		bool_t is_keyframe = FALSE;
		mblk_t *nalu;

		if(sampleBuffer == NULL || status != noErr) {
			vth264enc_error("could not encode frame: error %d", (int)status);
			return;
		}

		ms_mutex_lock(&ctx->_mutex);
		if(ctx->_session) {
			ms_queue_init(&nalu_queue);
			block_buffer = CMSampleBufferGetDataBuffer(sampleBuffer);
			frame_size = CMBlockBufferGetDataLength(block_buffer);
			while(read_size < frame_size) {
				char *chunk;
				size_t chunk_size;
				int idr_count;
				OSStatus status = CMBlockBufferGetDataPointer(block_buffer, offset, &chunk_size, NULL, &chunk);
				if (status != kCMBlockBufferNoErr) {
					vth264enc_error("error while reading a chunk of encoded frame: %s", toString(status).c_str());
					break;
				}
				ms_h264_stream_to_nalus((uint8_t *)chunk, chunk_size, &nalu_queue, &idr_count);
				if(idr_count) is_keyframe = TRUE;
				read_size += chunk_size;
				offset += chunk_size;
			}

			if (read_size < frame_size) {
				vth264enc_error("error while reading an encoded frame. Dropping it");
				ms_queue_flush(&nalu_queue);
				ms_mutex_unlock(&ctx->_mutex);
				return;
			}

			if(is_keyframe) {
				mblk_t *insertion_point = ms_queue_peek_first(&nalu_queue);
				const uint8_t *parameter_set;
				size_t parameter_set_size;
				size_t parameter_set_count;
				CMFormatDescriptionRef format_desc = CMSampleBufferGetFormatDescription(sampleBuffer);
				offset=0;
				do {
					CMVideoFormatDescriptionGetH264ParameterSetAtIndex(format_desc, offset, &parameter_set, &parameter_set_size, &parameter_set_count, NULL);
					nalu = allocb(parameter_set_size, 0);
					memcpy(nalu->b_wptr, parameter_set, parameter_set_size);
					nalu->b_wptr += parameter_set_size;
					ms_queue_insert(&nalu_queue, insertion_point, nalu);
					offset++;
				} while(offset < parameter_set_count);
				vth264enc_message("I-frame created");
			}

			rfc3984_pack(ctx->_packerCtx, &nalu_queue, &ctx->_queue, (uint32_t)(ctx->getTime() * 90));
		}
		ms_mutex_unlock(&ctx->_mutex);
	}

	static bool setEncodingSessionFps(VTCompressionSessionRef session, float fps) {
		CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &fps);
		OSStatus status = VTSessionSetProperty(session, kVTCompressionPropertyKey_ExpectedFrameRate, value);
		CFRelease(value);
		if (status != noErr) {
			vth264enc_error("error while setting kVTCompressionPropertyKey_ExpectedFrameRate: %s", toString(status).c_str());
			return false;
		}
		return true;
	}

	static bool setEncodingSessionBItrate(VTCompressionSessionRef session, int bitrate) {
		OSStatus status;

		CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &bitrate);
		status = VTSessionSetProperty(session, kVTCompressionPropertyKey_AverageBitRate, value);
		CFRelease(value);
		if (status != noErr) {
			vth264enc_error("error while setting kVTCompressionPropertyKey_AverageBitRate: %s", toString(status).c_str());
			return false;
		}

		int bytes_per_seconds = bitrate/8;
		int dur = 1;
		CFNumberRef bytes_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &bytes_per_seconds);
		CFNumberRef duration_value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &dur);
		CFMutableArrayRef data_rate_limits = CFArrayCreateMutable(kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(data_rate_limits, bytes_value);
		CFArrayAppendValue(data_rate_limits, duration_value);
		status = VTSessionSetProperty(session, kVTCompressionPropertyKey_DataRateLimits, data_rate_limits);
		CFRelease(bytes_value);
		CFRelease(duration_value);
		CFRelease(data_rate_limits);
		if (status != noErr) {
			vth264enc_error("error while setting kVTCompressionPropertyKey_DataRateLimits: %s", toString(status).c_str());
			return false;
		}

		return true;
	}

	void createEncodingSession() {
		OSStatus err;
		CFNumberRef value;

		CFMutableDictionaryRef pixbuf_attr = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, NULL, &kCFTypeDictionaryValueCallBacks);
		int32_t pixel_type = kCVPixelFormatType_420YpCbCr8Planar;
		value = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pixel_type);
		CFDictionarySetValue(pixbuf_attr, kCVPixelBufferPixelFormatTypeKey, value);
		CFRelease(value);

		CFMutableDictionaryRef session_props = CFDictionaryCreateMutable (kCFAllocatorDefault, 1, NULL, NULL);
	#if !TARGET_OS_IPHONE
		CFDictionarySetValue(session_props, kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder, kCFBooleanTrue);
	#endif

		err = VTCompressionSessionCreate(kCFAllocatorDefault, _conf.vsize.width, _conf.vsize.height, kCMVideoCodecType_H264,
										session_props, pixbuf_attr, kCFAllocatorDefault, (VTCompressionOutputCallback)outputCb, this, &_session);
		CFRelease(pixbuf_attr);
		CFRelease(session_props);
		if(err) {
			vth264enc_error("could not initialize the VideoToolbox compresson session: %s", toString(err).c_str());
			goto fail;
		}

		err = VTSessionSetProperty(_session, kVTCompressionPropertyKey_ProfileLevel, kVTProfileLevel_H264_Baseline_AutoLevel);
		if (err != noErr) {
			vth264enc_error("could not set H264 profile and level: %s", toString(err).c_str());
		}

		err = VTSessionSetProperty(_session, kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);
		if (err != noErr) {
			vth264enc_warning("could not enable real-time mode: %s", toString(err).c_str());
		}

		setEncodingSessionFps(_session, _conf.fps);
		setEncodingSessionBItrate(_session, _conf.required_bitrate);

		if((err = VTCompressionSessionPrepareToEncodeFrames(_session)) != noErr) {
			vth264enc_error("could not prepare the VideoToolbox compression session: %s", toString(err).c_str());
			goto fail;
		} else {
			vth264enc_message("encoder succesfully initialized.");
	#if !TARGET_OS_IPHONE
			CFBooleanRef hardware_acceleration_enabled;
			err = VTSessionCopyProperty(_session, kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder, kCFAllocatorDefault, &hardware_acceleration_enabled);
			if (err != noErr) {
				vth264enc_error("could not read kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder property: %s", toString(err).c_str());
			} else {
				if (hardware_acceleration_enabled != NULL && CFBooleanGetValue(hardware_acceleration_enabled)) {
					vth264enc_message("hardware acceleration enabled");
				} else {
					vth264enc_warning("hardware acceleration not enabled");
				}
			}
			if (hardware_acceleration_enabled != NULL) CFRelease(hardware_acceleration_enabled);
	#endif
		}
		return;

	fail:
		if(_session != NULL) CFRelease(_session);
	}

	void destroyEncodingSession() {
		vth264enc_message("destroying the encoding session");
		VTCompressionSessionInvalidate(_session);
		CFRelease( _session);
		_session = NULL;
	}

	VTCompressionSessionRef _session = nullptr;
	CFMutableDictionaryRef _frameProperties = nullptr;
	MSVideoConfiguration _conf;
	MSQueue _queue;
	ms_mutex_t _mutex;
	Rfc3984Context *_packerCtx = nullptr;
	const MSVideoConfiguration *_videoConfs = nullptr;
	MSVideoStarter _starter;
	bool_t _enableAvpf = false;
	bool_t _firstFrame = false;
	MSIFrameRequestsLimiterCtx _iframeLimiter;
};

} // namespace mediastreamer

using namespace mediastreamer;

MS_ENCODING_FILTER_WRAPPER_METHODS_DECLARATION(VideoToolboxH264Encoder);
MS_ENCODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(VideoToolboxH264Encoder, MS_VT_H264_ENC_ID, "H264 hardware encoder for iOS and MacOSX", "H264", MS_FILTER_IS_PUMP);

/* Undefine encoder log message macro to avoid to use them in decoder code */
#undef vth264enc_message
#undef vth264enc_warning
#undef vth264enc_error
#undef vth264enc_log



#define H264_NALU_HEAD_SIZE 4

#define VTH264_DEC_NAME "VideoToolboxH264Decoder"
#define vth264dec_log(level, fmt, ...) ms_##level(VTH264_DEC_NAME ": " fmt, ##__VA_ARGS__)
#define vth264dec_message(fmt, ...) vth264dec_log(message, fmt, ##__VA_ARGS__)
#define vth264dec_warning(fmt, ...) vth264dec_log(warning, fmt, ##__VA_ARGS__)
#define vth264dec_error(fmt, ...) vth264dec_log(error, fmt, ##__VA_ARGS__)
#define vth264dec_debug(fmt, ...) vth264dec_log(debug, fmt, ##__VA_ARGS__)


#define h264_dec_handle_error(need_pli) \
	need_pli = true; \
	if (_freezeOnErrorEnabled) { \
		vth264dec_message("pausing decoder until next I-frame"); \
		_freezed = true; \
		continue; \
	}

namespace mediastreamer {

class VideoToolboxH264DecoderFilterImpl: public DecodingFilterImpl {
public:
	VideoToolboxH264DecoderFilterImpl(MSFilter *f): DecodingFilterImpl(f) {
		ms_queue_init(&_queue);
		_pixbufAllocator = ms_yuv_buf_allocator_new();
		_unpacker = rfc3984_new_with_factory(getFactory());
		_vsize = MS_VIDEO_SIZE_UNKNOWN;
		ms_average_fps_init(&_fps, "VideoToolboxDecoder: decoding at %ffps");
	}

	~VideoToolboxH264DecoderFilterImpl() {
		rfc3984_destroy(_unpacker);
		if(_session != NULL) uninitDecoder();
		if(_formatDesc != NULL) CFRelease(_formatDesc);
		ms_queue_flush(&_queue);

		ms_yuv_buf_allocator_free(_pixbufAllocator);

		if (_sps != NULL) freemsg(_sps);
		if (_pps != NULL) freemsg(_pps);
	}

	void preprocess() override {}

	void process() override{
		mblk_t *pkt;
		mblk_t *pixbuf;
		MSQueue q_nalus;
		MSQueue q_nalus2;
		MSPicture pixbuf_desc;
		bool_t need_pli = FALSE;

		ms_queue_init(&q_nalus);
		ms_queue_init(&q_nalus2);

		while((pkt = ms_queue_get(getInput(0)))) {
			unsigned int unpack_status;

			ms_queue_flush(&q_nalus);
			ms_queue_flush(&q_nalus2);

			unpack_status = rfc3984_unpack2(_unpacker, pkt, &q_nalus);
			if (unpack_status & Rfc3984FrameAvailable) {
				filterNaluStream(&q_nalus, &q_nalus2);
			} else continue;
			if (unpack_status & Rfc3984FrameCorrupted) {
				h264_dec_handle_error(need_pli);
			}
			if ((_sps != NULL && _pps != NULL) && (_session == NULL || (unpack_status & (Rfc3984NewSPS | Rfc3984NewPPS)))) {
				if (_session != NULL) uninitDecoder();
				if (!initDecoder()) {
					vth264dec_error("decoder creation has failed");
					h264_dec_handle_error(need_pli);
				}
			}
			if (_session == NULL) {
				h264_dec_handle_error(need_pli);
			}
			if (unpack_status & Rfc3984HasIDR) {
				need_pli = FALSE;
				_freezed = FALSE;
			}

			if (!_freezed && !ms_queue_empty(&q_nalus2)) {
				OSStatus status = decodeFrame(&q_nalus2);
				if (status != noErr) {
					vth264dec_error("fail to decode one frame: %s", toString(status).c_str());
					if (status == kVTInvalidSessionErr) {
						uninitDecoder();
					}
					h264_dec_handle_error(need_pli);
				}
			}
		}

		ms_queue_flush(&q_nalus);
		ms_queue_flush(&q_nalus2);

		// Transfer decoded frames in the output queue
		lock();
		while((pixbuf = ms_queue_get(&_queue))) {
			ms_yuv_buf_init_from_mblk(&pixbuf_desc, pixbuf);
			if(pixbuf_desc.w != _vsize.width || pixbuf_desc.h != _vsize.height) {
				_vsize = (MSVideoSize){ pixbuf_desc.w , pixbuf_desc.h };
			}
			ms_average_fps_update(&_fps, (uint32_t)getTime());
			if(_firstImage) {
				notify(MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
				_firstImage = FALSE;
			}
			ms_queue_put(getOutput(0), pixbuf);
		}
		unlock();

		if (need_pli) {
			if (_enableAvpf) {
				notify(MS_VIDEO_DECODER_SEND_PLI);
			} else {
				notify(MS_VIDEO_DECODER_DECODING_ERRORS);
			}
		}
	}

	void postprocess() override {}

	MSVideoSize getVideoSize() const override {
		MSVideoSize vsize;
		lock();
		vsize = _vsize;
		unlock();
		return vsize;
	}

	float getFps() const override {
		float fps;
		lock();
		fps = ms_average_fps_get(&_fps);
		unlock();
		return fps;
	}

	const MSFmtDescriptor *getOutputFmt() const override {
		const MSFmtDescriptor *fmt;
		lock();
		fmt = ms_factory_get_video_format(getFactory(), "YUV420P", _vsize, 0.0f, NULL);
		unlock();
		return fmt;
	}

	void resetFirstImage() override {
		lock();
		_firstImage = true;
		unlock();
	}

	void enableAvpf(bool enable) override {
		lock();
		_enableAvpf = enable;
		unlock();
	}

	bool freezeOnErrorEnabled() const override {
		 return _freezeOnErrorEnabled;
	}

	void enableFreezeOnError(bool enable) override {
		_freezeOnErrorEnabled = enable;
	}

	void addFmtp(const char *fmtp) override {
		vth264dec_error("'MS_VIDEO_DECODER_ADD_FMTP' is not supported by VideoToolbox filters");
		throw MethodCallFailed();
	}

private:
	bool formatDescFromSpsPps() {
		const size_t ps_count = 2;
		const uint8_t *ps_ptrs[ps_count];
		size_t ps_sizes[ps_count];
		OSStatus status;
		CMFormatDescriptionRef format_desc;
		CMVideoDimensions vsize;

		ps_ptrs[0] = _sps->b_rptr;
		ps_sizes[0] = _sps->b_wptr - _sps->b_rptr;
		ps_ptrs[1] = _pps->b_rptr;
		ps_sizes[1] = _pps->b_wptr - _pps->b_rptr;

		status = CMVideoFormatDescriptionCreateFromH264ParameterSets(NULL, ps_count, ps_ptrs, ps_sizes, H264_NALU_HEAD_SIZE, &format_desc);
		if(status != noErr) {
			vth264dec_error("could not find out the input format: %d", (int)status);
			return false;
		}
		vsize = CMVideoFormatDescriptionGetDimensions(format_desc);
		vth264dec_message("new video format %dx%d", (int)vsize.width, (int)vsize.height);
		if (_formatDesc != NULL) CFRelease(_formatDesc);
		_formatDesc = format_desc;
		return true;
	}

	static void outputCb(VideoToolboxH264DecoderFilterImpl *ctx, void *sourceFrameRefCon,
								   OSStatus status, VTDecodeInfoFlags infoFlags, CVImageBufferRef imageBuffer,
								   CMTime presentationTimeStamp, CMTime presentationDuration ) {

		CGSize vsize;
		MSPicture pixbuf_desc;
		mblk_t *pixbuf = NULL;
		uint8_t *src_planes[4] = { NULL };
		int src_strides[4] = { 0 };
		size_t i;

		if(status != noErr || imageBuffer == NULL) {
			vth264dec_error("fail to decode one frame: %s", toString(status).c_str());
			if(ctx->_enableAvpf) {
				ctx->notify(MS_VIDEO_DECODER_SEND_PLI);
			}else{
				ctx->notify(MS_VIDEO_DECODER_DECODING_ERRORS);
			}
			return;
		}

		vsize = CVImageBufferGetEncodedSize(imageBuffer);
		ctx->_vsize.width = (int)vsize.width;
		ctx->_vsize.height = (int)vsize.height;
		pixbuf = ms_yuv_buf_allocator_get(ctx->_pixbufAllocator, &pixbuf_desc, (int)vsize.width, (int)vsize.height);

		CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
		for(i=0; i<3; i++) {
			src_planes[i] = static_cast<uint8_t *>(CVPixelBufferGetBaseAddressOfPlane(imageBuffer, i));
			src_strides[i] = (int)CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, i);
		}
		ms_yuv_buf_copy(src_planes, src_strides, pixbuf_desc.planes, pixbuf_desc.strides, ctx->_vsize);
		CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

		ctx->lock();
		ms_queue_put(&ctx->_queue, pixbuf);
		ctx->unlock();
	}

	bool initDecoder() {
		OSStatus status;
		VTDecompressionOutputCallbackRecord dec_cb = { (VTDecompressionOutputCallback)outputCb, this };

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
			status = VTSessionSetProperty(ctx->session, kVTDecompressionPropertyKey_RealTime, kCFBooleanTrue);
			if (status != noErr) {
				vth264dec_warning("could not be able to switch to real-time mode: %s", toString(status).c_str());
			}
	#endif

			return true;
		}
	}

	void uninitDecoder() {
		vth264dec_message("destroying decoder");
		VTDecompressionSessionInvalidate(_session);
		CFRelease(_session);
		CFRelease(_formatDesc);
		_session = NULL;
		_formatDesc = NULL;
	}

	OSStatus decodeFrame(MSQueue *frame) {
		CMBlockBufferRef stream = NULL;
		mblk_t *nalu = NULL;
		OSStatus status;
		status = CMBlockBufferCreateEmpty(kCFAllocatorDefault, 0, kCMBlockBufferAssureMemoryNowFlag, &stream);
		if (status != kCMBlockBufferNoErr) {
			vth264dec_error("failure while creating input buffer for decoder");
			return status;
		}
		while((nalu = ms_queue_get(frame))) {
			CMBlockBufferRef nalu_block;
			size_t nalu_block_size = msgdsize(nalu) + H264_NALU_HEAD_SIZE;
			uint32_t nalu_size = htonl(msgdsize(nalu));

			CMBlockBufferCreateWithMemoryBlock(NULL, NULL, nalu_block_size, NULL, NULL, 0, nalu_block_size, kCMBlockBufferAssureMemoryNowFlag, &nalu_block);
			CMBlockBufferReplaceDataBytes(&nalu_size, nalu_block, 0, H264_NALU_HEAD_SIZE);
			CMBlockBufferReplaceDataBytes(nalu->b_rptr, nalu_block, H264_NALU_HEAD_SIZE, msgdsize(nalu));
			CMBlockBufferAppendBufferReference(stream, nalu_block, 0, nalu_block_size, 0);
			CFRelease(nalu_block);
			freemsg(nalu);
		}
		if(!CMBlockBufferIsEmpty(stream)) {
			CMSampleBufferRef sample = NULL;
			CMSampleTimingInfo timing_info;
			timing_info.duration = kCMTimeInvalid;
			timing_info.presentationTimeStamp = CMTimeMake(getTime(), 1000);
			timing_info.decodeTimeStamp = CMTimeMake(getTime(), 1000);
			CMSampleBufferCreate(
						kCFAllocatorDefault, stream, TRUE, NULL, NULL,
						_formatDesc, 1, 1, &timing_info,
						0, NULL, &sample);

			status = VTDecompressionSessionDecodeFrame(_session, sample, kVTDecodeFrame_EnableAsynchronousDecompression | kVTDecodeFrame_1xRealTimePlayback, NULL, NULL);
			CFRelease(sample);
			if(status != noErr) {
				vth264dec_error("error while passing encoded frames to the decoder: %s", toString(status).c_str());
				CFRelease(stream);
				return status;
			}
		}
		CFRelease(stream);
		return noErr;
	}

	/*
	 * Remove non-VCL NALu from a nalu stream. SPSs and PPSs are saved in
	 * the decoding context.
	 */
	void filterNaluStream(MSQueue *input, MSQueue *output) {
		mblk_t *nalu;
		while((nalu = ms_queue_get(input))) {
			MSH264NaluType nalu_type = ms_h264_nalu_get_type(nalu);
			switch (nalu_type) {
			case MSH264NaluTypeSPS:
				if (_sps != NULL) freemsg(_sps);
				_sps = nalu;
				break;
			case MSH264NaluTypePPS:
				if (_pps != NULL) freemsg(_pps);
				_pps = nalu;
				break;
			case MSH264NaluTypeSEI:
				freemsg(nalu);
				break;
			default:
				ms_queue_put(output, nalu);
			}
		}
	}

	VTDecompressionSessionRef _session = nullptr;
	CMFormatDescriptionRef _formatDesc = nullptr;
	Rfc3984Context *_unpacker = nullptr;
	MSQueue _queue;
	MSYuvBufAllocator *_pixbufAllocator = nullptr;
	MSVideoSize _vsize;
	MSAverageFPS _fps;
	bool _firstImage = true;
	bool _enableAvpf = false;
	bool _freezeOnErrorEnabled = true;
	bool _freezed = true;
	mblk_t *_sps = nullptr;
	mblk_t *_pps = nullptr;
};

} // namespace mediastreamer


MS_DECODING_FILTER_WRAPPER_METHODS_DECLARATION(VideoToolboxH264Decoder);
MS_DECODING_FILTER_WRAPPER_DESCRIPTION_DECLARATION(VideoToolboxH264Decoder, MS_VT_H264_DEC_ID, "H264 hardware decoder for iOS and MacOSX", "H264", MS_FILTER_IS_PUMP);

/* Undefine decoder log message macros to avoid to use them in other code */
#undef vth264dec_message
#undef vth264dec_warning
#undef vth264dec_error
#undef vth264dec_log

extern "C" void _register_videotoolbox_if_supported(MSFactory *factory) {
#if TARGET_OS_SIMULATOR
	ms_message("VideoToolbox H264 codec is not supported on simulators");
#else

#if TARGET_OS_IPHONE
	if (kCFCoreFoundationVersionNumber >= kCFCoreFoundationVersionNumber_iOS_8_0) {
#else
	if (kCFCoreFoundationVersionNumber >= kCFCoreFoundationVersionNumber10_8) {
#endif
		ms_message("Registering VideoToobox H264 codec");
		ms_factory_register_filter(factory, &ms_VideoToolboxH264Encoder_desc);
		ms_factory_register_filter(factory, &ms_VideoToolboxH264Decoder_desc);
	} else {
		ms_message("Cannot register VideoToolbox H264 codec. That "
			"requires iOS 8 or MacOSX 10.8");
	}
	
#endif
}


