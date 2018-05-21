/*
mediastreamer2 mediacodech264dec.c
Copyright (C) 2015 Belledonne Communications SARL

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

#include <memory>
#include <vector>

#include <jni.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <ortp/b64.h>
#include <ortp/str_utils.h>

#include "mediastreamer2/formats.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msticker.h"

#include "android_mediacodec.h"
#include "h264utils.h"
#include "h264-nal-unpacker.h"

using namespace b64;
using namespace mediastreamer;

namespace mediastreamer {

	class MediaCodecH264DecoderFilterImpl {
	public:
		MediaCodecH264DecoderFilterImpl(MSFilter *f): _f(f) {
			ms_message("MSMediaCodecH264Dec initialization");
			_vsize.width = 0;
			_vsize.height = 0;
			ms_average_fps_init(&_fps, " H264 decoder: FPS: %f");
			_bufAllocator = ms_yuv_buf_allocator_new();
		}

		~MediaCodecH264DecoderFilterImpl() {
			if (_codec){
				AMediaCodec_stop(_codec);
				AMediaCodec_delete(_codec);
			}
			if (_sps) freemsg(_sps);
			if (_pps) freemsg(_pps);
			ms_yuv_buf_allocator_free(_bufAllocator);
		}

		void preprocess() {
			_firstImageDecoded = false;
		}

		void process() {
			MSPicture pic = {0};
			mblk_t *im = nullptr;
			ssize_t oBufidx = -1;
			size_t bufsize;
			bool need_reinit = false;
			bool request_pli = false;
			MSQueue nalus;
			AMediaCodecBufferInfo info;
			H264FrameAnalyser frameAnalyser;

			if (_packetNum == 0 && _sps && _pps) {
				_unpacker->setOutOfBandSpsPps(_sps, _pps);
				_sps = nullptr;
				_pps = nullptr;
			}

			ms_queue_init(&nalus);

			while ((im = ms_queue_get(_f->inputs[0])) != nullptr) {
				int size;
				uint8_t *buf = nullptr;
				ssize_t iBufidx;
				NalUnpacker::Status unpacking_ret = _unpacker->unpack(im, &nalus);

				if (!unpacking_ret.frameAvailable) continue;

				if (unpacking_ret.frameCorrupted) {
					ms_warning("MSMediaCodecH264Dec: corrupted frame");
					request_pli = true;
					if (_freezeOnError){
						ms_queue_flush(&nalus);
						_needKeyFrame = true;
						continue;
					}
				}

				H264FrameAnalyser::Info frameInfo = frameAnalyser.analyse(&nalus);
				if (_needKeyFrame && !frameInfo.hasIdr) {
					request_pli = true;
					ms_queue_flush(&nalus);
					continue;
				} else if (frameInfo.hasIdr) {
					ms_message("MSMediaCodecH264Dec: I-frame received");
				}

				nalusToFrame(&nalus, &need_reinit);
				size = bitstream.size();
				//Initialize the video size
				if((_codec==nullptr) && _sps) {
					initMediaCodec();
				}

				if (_codec == nullptr) continue;

				if (need_reinit) {
					//In case of remote rotation, the decoder needs to flushed in order to restart with the new video size
					ms_message("MSMediaCodecH264Dec: SPS/PPS have changed. Flushing all MediaCodec's buffers");
					flush(true);
				}

				/*First put our H264 bitstream into the decoder*/
				iBufidx = AMediaCodec_dequeueInputBuffer(_codec, _timeoutUs);

				if (iBufidx >= 0) {
					struct timespec ts;
					buf = AMediaCodec_getInputBuffer(_codec, iBufidx, &bufsize);

					if (buf == nullptr) {
						ms_error("MSMediaCodecH264Dec: AMediaCodec_getInputBuffer() returned NULL");
						continue;
					}
					clock_gettime(CLOCK_MONOTONIC, &ts);

					if ((size_t)size > bufsize) {
						ms_error("Cannot copy the all the bitstream into the input buffer size : %i and bufsize %i", size, (int) bufsize);
						size = MIN((size_t)size, bufsize);
					}
					memcpy(buf, bitstream.data(), bitstream.size());

					if (_needKeyFrame){
						ms_message("MSMediaCodecH264Dec: fresh I-frame submitted to the decoder");
						_needKeyFrame = false;
					}

					if (AMediaCodec_queueInputBuffer(_codec, iBufidx, 0, (size_t)size, (ts.tv_nsec / 1000) + 10000LL, 0) == 0){
						if (!_bufferQueued) _bufferQueued = true;
					}else{
						ms_error("MSMediaCodecH264Dec: AMediaCodec_queueInputBuffer() had an exception");
						flush(false);
						request_pli = true;
						continue;
					}
				} else if (iBufidx == -1){
					ms_error("MSMediaCodecH264Dec: no buffer available for queuing this frame ! Decoder is too slow.");
					/*
					 * This is a problematic case because we can't wait the decoder to be ready, otherwise we'll freeze the entire
					 * video MSTicker thread.
					 * We have no other option to drop the frame, and retry later, but with an I-frame of course.
					 **/
					request_pli = true;
					_needKeyFrame = true;
					continue;
				}else {
					ms_error("MSMediaCodecH264Dec: AMediaCodec_dequeueInputBuffer() had an exception");
					flush(false);
					request_pli = true;
					continue;
				}
				_packetNum++;
			}

			/*secondly try to get decoded frames from the decoder, this is performed every tick*/
			while (_bufferQueued && (oBufidx = AMediaCodec_dequeueOutputBuffer(_codec, &info, _timeoutUs)) >= 0) {
				mblk_t *om = nullptr;
				uint8_t *buf = AMediaCodec_getOutputBuffer(_codec, oBufidx, &bufsize);

				if (buf == nullptr) {
					ms_error("MSMediaCodecH264Dec: AMediaCodec_getOutputBuffer() returned NULL");
					continue;
				}

				AMediaImage image;
				if (AMediaCodec_getOutputImage(_codec, oBufidx, &image)) {
					int dst_pix_strides[4] = {1, 1, 1, 1};
					MSRect dst_roi = {0, 0, image.crop_rect.w, image.crop_rect.h};

					_vsize.width = image.crop_rect.w;
					_vsize.height = image.crop_rect.h;

					om = ms_yuv_buf_allocator_get(_bufAllocator, &pic, _vsize.width, _vsize.height);
					ms_yuv_buf_copy_with_pix_strides(image.buffers, image.row_strides, image.pixel_strides, image.crop_rect,
														pic.planes, pic.strides, dst_pix_strides, dst_roi);
					AMediaImage_close(&image);
				}else{
					ms_error("AMediaCodec_getOutputImage() failed");
				}

				if (om){

					if (!_firstImageDecoded) {
						ms_message("First frame decoded %ix%i", _vsize.width, _vsize.height);
						_firstImageDecoded = true;
						ms_filter_notify_no_arg(_f, MS_VIDEO_DECODER_FIRST_IMAGE_DECODED);
					}

					ms_average_fps_update(&_fps, _f->ticker->time);
					ms_queue_put(_f->outputs[0], om);
				}

				AMediaCodec_releaseOutputBuffer(_codec, oBufidx, FALSE);
			}

			if (oBufidx == AMEDIA_ERROR_UNKNOWN) {
				ms_error("MSMediaCodecH264Dec: AMediaCodec_dequeueOutputBuffer() had an exception");
				flush(false);
				request_pli = true;
			}

			if (_avpfEnabled && request_pli) {
				ms_filter_notify_no_arg(_f, MS_VIDEO_DECODER_SEND_PLI);
			}
		}

		void postprocess() {
		}

		void addFmtp(const char *fmtp) {
			char value[256];
			if (fmtp_get_value(fmtp, "sprop-parameter-sets", value, sizeof(value))) {
				char *b64_sps = value;
				char *b64_pps = strchr(value, ',');

				if (b64_pps) {
					*b64_pps = '\0';
					++b64_pps;
					ms_message("Got sprop-parameter-sets : sps=%s , pps=%s", b64_sps, b64_pps);
					_sps = allocb(sizeof(value), 0);
					_sps->b_wptr += b64_decode(b64_sps, strlen(b64_sps), _sps->b_wptr, sizeof(value));
					_pps = allocb(sizeof(value), 0);
					_pps->b_wptr += b64_decode(b64_pps, strlen(b64_pps), _pps->b_wptr, sizeof(value));
				}
			}
		}

		void resetFirstImage() {
			_firstImageDecoded = false;
		}

		MSVideoSize getVideoSize() const {
			return _firstImageDecoded ? _vsize : MS_VIDEO_SIZE_UNKNOWN;
		}

		float getFps() const {
			return ms_average_fps_get(&_fps);
		}

		const MSFmtDescriptor *getOutFmt() const {
			return ms_factory_get_video_format(_f->factory, "YUV420P", ms_video_size_make(_vsize.width, _vsize.height), 0, nullptr);
		}

		void enableAvpf(bool enable) {
			_avpfEnabled = enable;
		}

		void enableFreezeOnError(bool enable) {
			_freezeOnError = enable;
			ms_message("MSMediaCodecH264Dec: freeze on error %s", _freezeOnError ? "enabled" : "disabled");
		}

		static void onFilterInit(MSFilter *f) {
			f->data = new MediaCodecH264DecoderFilterImpl(f);
		}

		static void onFilterUninit(MSFilter *f) {
			delete static_cast<MediaCodecH264DecoderFilterImpl *>(f->data);
		}

		static void onFilterPreProcess(MSFilter *f) {
			static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->preprocess();
		}

		static void onFilterPostProcess(MSFilter *f) {
			static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->postprocess();
		}

		static void onFilterProcces(MSFilter *f) {
			static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->process();
		}

		static int onAddFmtpCall(MSFilter *f, void *arg) {
			const char *fmtp = static_cast<const char *>(arg);
			static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->addFmtp(fmtp);
			return 0;
		}

		static int onResetFirstImageCall(MSFilter *f, void *arg) {
			static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->resetFirstImage();
			return 0;
		}

		static int onGetVideoSizeCall(MSFilter *f, void *arg) {
			MSVideoSize *vsize = static_cast<MSVideoSize *>(arg);
			*vsize = static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->getVideoSize();
			return 0;
		}

		static int onGetFpsCall(MSFilter *f, void *arg) {
			float *fps = static_cast<float *>(arg);
			*fps = static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->getFps();
			return 0;
		}

		static int onGetOutFmtCall(MSFilter *f, void *arg) {
			MSPinFormat *pinFormat = static_cast<MSPinFormat *>(arg);
			pinFormat->fmt = static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->getOutFmt();
			return 0;
		}

		static int onEnableAvpfCall(MSFilter *f, void *arg) {
			const bool_t *enable = static_cast<bool_t *>(arg);
			static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->enableAvpf(enable);
			return 0;
		}

		static int onEnableFreezeOnErrorCall(MSFilter *f, void *arg) {
			const bool_t *enable = static_cast<bool_t *>(arg);
			static_cast<MediaCodecH264DecoderFilterImpl *>(f->data)->enableFreezeOnError(enable);
			return 0;
		}

	private:
		int initMediaCodec() {
			AMediaFormat *format;
			media_status_t status = AMEDIA_OK;

			if (_codec == nullptr){
				_codec = AMediaCodec_createDecoderByType("video/avc");
				if (_codec == nullptr){
					ms_error("MSMediaCodecH264Dec: could not create MediaCodec");
					return AMEDIA_ERROR_UNKNOWN;
				}
			}

			format = AMediaFormat_new();
			AMediaFormat_setString(format, "mime", "video/avc");
			//Size mandatory for decoder configuration
			if(_sps) {
				MSVideoSize initial_size = ms_h264_sps_get_video_size(_sps);
				AMediaFormat_setInt32(format, "width", initial_size.width);
				AMediaFormat_setInt32(format, "height", initial_size.height);
			}

			AMediaFormat_setInt32(format, "color-format", 0x7f420888);

			if ((status = AMediaCodec_configure(_codec, format, nullptr, nullptr, 0)) != AMEDIA_OK) {
				ms_error("MSMediaCodecH264Dec: configuration failure: %i", (int)status);
				goto end;
			}

			if ((status = AMediaCodec_start(_codec)) != AMEDIA_OK) {
				ms_error("MSMediaCodecH264Dec: starting failure: %i", (int)status);
				goto end;
			}
			_needKeyFrame = true;

			end:
			AMediaFormat_delete(format);
			return status;
		}

		void flush(bool with_reset){
			if (with_reset || (AMediaCodec_flush(_codec) != 0)){
				AMediaCodec_reset(_codec);
				initMediaCodec();
			}
			_needKeyFrame = true;
			_bufferQueued = false;
		}

		void updateSps(mblk_t *sps) {
			if (_sps) freemsg(_sps);
			_sps = dupb(sps);
		}

		void updatePps(mblk_t *pps) {
			if (_pps) freemsg(_pps);
			if (pps) _pps = dupb(pps);
			else _pps = nullptr;
		}

		bool checkSpsChange(mblk_t *sps) {
			bool ret = false;

			if (_sps) {
				ret = (msgdsize(sps) != msgdsize(_sps)) || (memcmp(_sps->b_rptr, sps->b_rptr, msgdsize(sps)) != 0);

				if (ret) {
					ms_message("SPS changed ! %i,%i", (int)msgdsize(sps), (int)msgdsize(_sps));
					updateSps(sps);
					updatePps(nullptr);
				}
			} else {
				ms_message("Receiving first SPS");
				updateSps(sps);
			}

			return ret;
		}

		bool checkPpsChange(mblk_t *pps) {
			bool_t ret = false;

			if (_pps) {
				ret = (msgdsize(pps) != msgdsize(_pps)) || (memcmp(_pps->b_rptr, pps->b_rptr, msgdsize(pps)) != 0);

				if (ret) {
					ms_message("PPS changed ! %i,%i", (int)msgdsize(pps), (int)msgdsize(_pps));
					updatePps(pps);
				}
			} else {
				ms_message("Receiving first PPS");
				updatePps(pps);
			}

			return ret;
		}

		void nalusToFrame(MSQueue *naluq, bool *new_sps_pps) {
			bool start_picture = true;
			*new_sps_pps = false;
			bitstream.resize(0);
			while (mblk_t *im = ms_queue_get(naluq)) {
				const uint8_t *src = im->b_rptr;
				if (src[0] == 0 && src[1] == 0 && src[2] == 0 && src[3] == 1) {
					while (src != im->b_wptr) {
						bitstream.push_back(*src++);
					}
				} else {
					uint8_t nalu_type = (*src) & ((1 << 5) - 1);

					if (nalu_type == 7)
						*new_sps_pps = (checkSpsChange(im) || *new_sps_pps);

					if (nalu_type == 8)
						*new_sps_pps = (checkPpsChange(im) || *new_sps_pps);

					if (start_picture || nalu_type == 7/*SPS*/ || nalu_type == 8/*PPS*/) {
						bitstream.push_back(0);
						start_picture = false;
					}

					/*prepend nal marker*/
					bitstream.push_back(0);
					bitstream.push_back(0);
					bitstream.push_back(1);
					bitstream.push_back(*src++);

					while (src < (im->b_wptr - 3)) {
						if (src[0] == 0 && src[1] == 0 && src[2] < 3) {
							bitstream.push_back(0);
							bitstream.push_back(0);
							bitstream.push_back(3);
							src += 2;
						}

						bitstream.push_back(*src++);
					}

					bitstream.push_back(*src++);
					bitstream.push_back(*src++);
					bitstream.push_back(*src++);
				}

				freemsg(im);
			}
		}

		MSFilter *_f = nullptr;
		mblk_t *_sps = nullptr;
		mblk_t *_pps = nullptr;
		MSVideoSize _vsize;
		AMediaCodec *_codec = nullptr;

		MSAverageFPS _fps;
		std::unique_ptr<H264NalUnpacker> _unpacker;
		unsigned int _packetNum = 0;
		std::vector<uint8_t> bitstream;
		MSYuvBufAllocator *_bufAllocator = nullptr;
		bool _bufferQueued = false;
		bool _firstImageDecoded = false;
		bool _avpfEnabled = false;
		bool _needKeyFrame = false;
		bool _freezeOnError = true;

		static const unsigned int _timeoutUs = 0;
	};

};

static MSFilterMethod  mediacodec_h264_dec_methods[] = {
	{	MS_FILTER_ADD_FMTP                                 , MediaCodecH264DecoderFilterImpl::onAddFmtpCall             },
	{	MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION    , MediaCodecH264DecoderFilterImpl::onResetFirstImageCall     },
	{	MS_FILTER_GET_VIDEO_SIZE                           , MediaCodecH264DecoderFilterImpl::onGetVideoSizeCall        },
	{	MS_FILTER_GET_FPS                                  , MediaCodecH264DecoderFilterImpl::onGetFpsCall              },
	{	MS_FILTER_GET_OUTPUT_FMT                           , MediaCodecH264DecoderFilterImpl::onGetOutFmtCall           },
	{ 	MS_VIDEO_DECODER_ENABLE_AVPF                       , MediaCodecH264DecoderFilterImpl::onEnableAvpfCall          },
	{	MS_VIDEO_DECODER_FREEZE_ON_ERROR                   , MediaCodecH264DecoderFilterImpl::onEnableFreezeOnErrorCall },
	{	0                                                  , nullptr                                                    }
};


extern "C" MSFilterDesc ms_mediacodec_h264_dec_desc = {
	.id = MS_MEDIACODEC_H264_DEC_ID,
	.name = "MSMediaCodecH264Dec",
	.text = "A H264 decoder based on MediaCodec API.",
	.category = MS_FILTER_DECODER,
	.enc_fmt = "H264",
	.ninputs = 1,
	.noutputs = 1,
	.init = MediaCodecH264DecoderFilterImpl::onFilterInit,
	.preprocess = MediaCodecH264DecoderFilterImpl::onFilterPreProcess,
	.process = MediaCodecH264DecoderFilterImpl::onFilterProcces,
	.postprocess = MediaCodecH264DecoderFilterImpl::onFilterPostProcess,
	.uninit = MediaCodecH264DecoderFilterImpl::onFilterUninit,
	.methods = mediacodec_h264_dec_methods,
	.flags = MS_FILTER_IS_PUMP
};
