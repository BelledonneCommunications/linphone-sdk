/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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

#define bool_t matroska_bool_t
#include <matroska/matroska.h>
#include <matroska/matroska_sem.h>
#undef bool_t

#define bool_t ms_bool_t
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/rfc3984.h"
#include "mediastreamer2/msticker.h"
#include "waveheader.h"
#include "mediastreamer2/formats.h"
#include "vp8rtpfmt.h"
#include "mkv_reader.h"
#undef bool_t

#define bool_t ambigous use ms_bool_t or matroska_bool_t

static int recorder_close(MSFilter *f, void *arg);


/*********************************************************************************************
 * Module interface                                                                          *
 *********************************************************************************************/
typedef void *(*ModuleNewFunc)(MSFactory *factory);
typedef void (*ModuleFreeFunc)(void *obj);
typedef void (*ModuleSetFunc)(void *obj, const MSFmtDescriptor *fmt);
typedef int (*ModulePreProFunc)(void *obj, MSQueue *input, MSQueue *output);
typedef mblk_t *(*ModuleProFunc)(void *obj, mblk_t *buffer, ms_bool_t *isKeyFrame, ms_bool_t *isVisible, uint8_t **codecPrivateData, size_t *codecPrivateSize);
typedef void (*ModuleReverseFunc)(MSFactory* factory, void *obj, mblk_t *input, MSQueue *output, ms_bool_t isFirstFrame, const uint8_t *codecPrivateData, size_t codecPrivateSize);
typedef void (*ModulePrivateDataFunc)(const void *obj, uint8_t **data, size_t *data_size);
typedef void (*ModulePrivateDataLoadFunc)(void *obj, const uint8_t *data, size_t size);
typedef ms_bool_t (*ModuleIsKeyFrameFunc)(const mblk_t *frame);

typedef struct {
	const char *rfcName;
	const char *codecId;
	ModuleNewFunc new_module;
	ModuleFreeFunc free_module;
	ModuleSetFunc set;
	ModulePreProFunc preprocess;
	ModuleProFunc process;
	ModuleReverseFunc reverse;
	ModulePrivateDataFunc get_private_data;
	ModulePrivateDataLoadFunc load_private_data;
	ModuleIsKeyFrameFunc is_key_frame;
} ModuleDesc;

/*********************************************************************************************
 * h264 module                                                                               *
 *********************************************************************************************/
/* H264Private */
typedef struct {
	uint8_t profile;
	uint8_t level;
	uint8_t NALULenghtSizeMinusOne;
	bctbx_list_t *sps_list;
	bctbx_list_t *pps_list;
} H264Private;

static void _ms_list_append_copy(const mblk_t *buffer, bctbx_list_t **list) {
	*list = bctbx_list_append(*list, copymsg(buffer));
}

static void H264Private_init(H264Private *obj, const bctbx_list_t *spsList, const bctbx_list_t *ppsList) {
	memset(obj, 0, sizeof(H264Private));
	obj->NALULenghtSizeMinusOne = 0xFF;
	bctbx_list_for_each2(spsList, (MSIterate2Func)_ms_list_append_copy, &obj->sps_list);
	bctbx_list_for_each2(ppsList, (MSIterate2Func)_ms_list_append_copy, &obj->pps_list);
	if(obj->sps_list != NULL) {
		const mblk_t *firstSPS = (const mblk_t *)bctbx_list_nth_data(obj->sps_list, 0);
		obj->profile = firstSPS->b_rptr[1];
		obj->level = firstSPS->b_rptr[3];
	}
}

static void H264Private_uninit(H264Private *obj) {
	if(obj->sps_list != NULL) bctbx_list_free_with_data(obj->sps_list, (void ( *)(void *))freemsg);
	if(obj->pps_list != NULL) bctbx_list_free_with_data(obj->pps_list, (void ( *)(void *))freemsg);
}

static H264Private *H264Private_new(const bctbx_list_t *spsList, const bctbx_list_t *ppsList) {
	H264Private *obj = (H264Private *)ms_new0(H264Private, 1);
	H264Private_init(obj, spsList, ppsList);
	return obj;
}

static void H264Private_free(H264Private *obj) {
	H264Private_uninit(obj);
	ms_free(obj);
}

static const bctbx_list_t *H264Private_getSPS(const H264Private *obj) {
	return obj->sps_list;
}

static const bctbx_list_t *H264Private_getPPS(const H264Private *obj) {
	return obj->pps_list;
}

static void H264Private_serialize(const H264Private *obj, uint8_t **data, size_t *size) {
	uint8_t nbSPS, nbPPS;
	bctbx_list_t *it = NULL;
	uint8_t *result = NULL;
	size_t i;

	nbSPS = (uint8_t)bctbx_list_size(obj->sps_list);
	nbPPS = (uint8_t)bctbx_list_size(obj->pps_list);

	*size = 7;
	*size += (nbSPS + nbPPS) * 2;

	for(it = obj->sps_list; it != NULL; it = it->next) {
		mblk_t *buff = (mblk_t *)(it->data);
		*size += msgdsize(buff);
	}
	for(it = obj->pps_list; it != NULL; it = it->next) {
		mblk_t *buff = (mblk_t *)(it->data);
		*size += msgdsize(buff);
	}

	result = (uint8_t *)ms_new0(uint8_t, *size);
	result[0] = 0x01;
	result[1] = obj->profile;
	result[3] = obj->level;
	result[4] = obj->NALULenghtSizeMinusOne;
	result[5] = nbSPS & 0x1F;

	i = 6;
	for(it = obj->sps_list; it != NULL; it = it->next) {
		mblk_t *buff = (mblk_t *)(it->data);
		size_t buff_size = msgdsize(buff);
		uint16_t buff_size_be = htons((uint16_t)buff_size);
		memcpy(&result[i], &buff_size_be, sizeof(buff_size_be));
		i += sizeof(buff_size_be);
		memcpy(&result[i], buff->b_rptr, buff_size);
		i += buff_size;
	}

	result[i] = nbPPS;
	i++;

	for(it = obj->pps_list; it != NULL; it = it->next) {
		mblk_t *buff = (mblk_t *)(it->data);
		size_t buff_size = msgdsize(buff);
		uint16_t buff_size_be = htons((uint16_t)buff_size);
		memcpy(&result[i], &buff_size_be, sizeof(buff_size_be));
		i += sizeof(buff_size_be);
		memcpy(&result[i], buff->b_rptr, buff_size);
		i += buff_size;
	}
	*data = result;
}

static void H264Private_load(H264Private *obj, const uint8_t *data) {
	int i, N;
	const uint8_t *r_ptr = NULL;
	uint16_t nalu_size;
	mblk_t *nalu = NULL;

	H264Private_uninit(obj);
	H264Private_init(obj, NULL, NULL);

	N = data[5] & 0x1F;
	r_ptr = data + 6;
	for(i = 0; i < N; i++) {
		memcpy(&nalu_size, r_ptr, sizeof(uint16_t));
		r_ptr += sizeof(uint16_t);
		nalu_size = ntohs(nalu_size);
		nalu = allocb(nalu_size, 0);
		memcpy(nalu->b_wptr, r_ptr, nalu_size);
		nalu->b_wptr += nalu_size;
		r_ptr += nalu_size;
		obj->sps_list = bctbx_list_append(obj->sps_list, nalu);
	}

	N = *r_ptr;
	r_ptr += 1;
	for(i = 0; i < N; i++) {
		memcpy(&nalu_size, r_ptr, sizeof(uint16_t));
		r_ptr += sizeof(uint16_t);
		nalu_size = ntohs(nalu_size);
		nalu = allocb(nalu_size, 0);
		memcpy(nalu->b_wptr, r_ptr, nalu_size);
		nalu->b_wptr += nalu_size;
		r_ptr += nalu_size;
		obj->pps_list = bctbx_list_append(obj->pps_list, nalu);
	}

	if(obj->sps_list != NULL) {
		const mblk_t *firstSPS = (const mblk_t *)bctbx_list_nth_data(obj->sps_list, 0);
		obj->profile = firstSPS->b_rptr[1];
		obj->level = firstSPS->b_rptr[3];
	}
}

/* h264 module */
typedef struct {
	Rfc3984Context rfc3984Context;
	H264Private *codecPrivate;
} H264Module;

static void *h264_module_new(MSFactory *factory) {
	H264Module *mod = ms_new0(H264Module, 1);
	rfc3984_init(&mod->rfc3984Context);
	mod->rfc3984Context.maxsz = ms_factory_get_payload_max_size(factory);
	rfc3984_set_mode(&mod->rfc3984Context, 1);
	return mod;
}

static void h264_module_free(void *data) {
	H264Module *obj = (H264Module *)data;
	rfc3984_uninit(&obj->rfc3984Context);
	if(obj->codecPrivate != NULL) {
		H264Private_free(obj->codecPrivate);
	}
	ms_free(obj);
}

static int h264_module_preprocessing(void *data, MSQueue *input, MSQueue *output) {
	H264Module *obj = (H264Module *)data;
	MSQueue queue;
	mblk_t *inputBuffer = NULL;

	ms_queue_init(&queue);
	while((inputBuffer = ms_queue_get(input)) != NULL) {
		rfc3984_unpack2(&obj->rfc3984Context, inputBuffer, &queue);
		if(!ms_queue_empty(&queue)) {
			mblk_t *frame = ms_queue_get(&queue);
			mblk_t *end = frame;
			while(!ms_queue_empty(&queue)) {
				end = concatb(end, ms_queue_get(&queue));
			}
			ms_queue_put(output, frame);
		}
	}

	return 0;
}

static int h264_nalu_type(const mblk_t *nalu) {
	return (nalu->b_rptr[0]) & ((1 << 5) - 1);
}

static ms_bool_t h264_is_key_frame(const mblk_t *frame) {
	const mblk_t *curNalu = NULL;
	for(curNalu = frame; curNalu != NULL && h264_nalu_type(curNalu) != 5; curNalu = curNalu->b_cont);
	return curNalu != NULL;
}

static void nalus_to_frame(mblk_t *buffer, mblk_t **frame, bctbx_list_t **spsList, bctbx_list_t **ppsList, ms_bool_t *isKeyFrame) {
	mblk_t *curNalu = NULL;
	uint32_t timecode = mblk_get_timestamp_info(buffer);
	*frame = NULL;
	*isKeyFrame = FALSE;


	for(curNalu = buffer; curNalu != NULL;) {
		mblk_t *buff = curNalu;
		int type = h264_nalu_type(buff);

		curNalu = curNalu->b_cont;
		buff->b_cont = NULL;
		switch(type) {
			case 7:
				*spsList = bctbx_list_append(*spsList, buff);
				break;

			case 8:
				*ppsList = bctbx_list_append(*ppsList, buff);
				break;

			default:
			{
				uint32_t bufferSize = htonl((uint32_t)msgdsize(buff));
				mblk_t *size = allocb(4, 0);

				if(type == 5) {
					*isKeyFrame = TRUE;
				}
				memcpy(size->b_wptr, &bufferSize, sizeof(bufferSize));
				size->b_wptr = size->b_wptr + sizeof(bufferSize);
				concatb(size, buff);
				buff = size;

				if(*frame == NULL) {
					*frame = buff;
				} else {
					concatb(*frame, buff);
				}
			}
		}
	}

	if(*frame != NULL) {
		msgpullup(*frame, (size_t)-1);
		mblk_set_timestamp_info(*frame, timecode);
	}
}

static mblk_t *h264_module_processing(void *data, mblk_t *nalus, ms_bool_t *isKeyFrame, ms_bool_t *isVisible, uint8_t **codecPrivateData, size_t *codecPrivateSize) {
	H264Module *obj = (H264Module *)data;
	mblk_t *frame = NULL;
	bctbx_list_t *spsList = NULL, *ppsList = NULL;
	nalus_to_frame(nalus, &frame, &spsList, &ppsList, isKeyFrame);
	if(spsList != NULL || ppsList != NULL) {
		H264Private *codecPrivateStruct = H264Private_new(spsList, ppsList);
		ms_message("MKVRecorder: Received H264 SPS [%p] or PPS [%p]", spsList, ppsList);
		if(obj->codecPrivate == NULL) {
			obj->codecPrivate = codecPrivateStruct;
		} else {
			H264Private_serialize(codecPrivateStruct, codecPrivateData, codecPrivateSize);
			H264Private_free(codecPrivateStruct);
		}
		if(spsList != NULL) bctbx_list_free_with_data(spsList, (void ( *)(void *))freemsg);
		if(ppsList != NULL) bctbx_list_free_with_data(ppsList, (void ( *)(void *))freemsg);
	}
	*isVisible = TRUE;
	return frame;
}

static void h264_module_reverse(MSFactory* factory, void *data, mblk_t *input, MSQueue *output, ms_bool_t isFirstFrame, const uint8_t *codecPrivateData, size_t codecPrivateSize) {
	mblk_t *buffer = NULL, *bufferFrag = NULL;
	H264Module *obj = (H264Module *)data;
	MSQueue queue;
	const bctbx_list_t *it = NULL;
	H264Private *codecPrivate = NULL, *selectedCodecPrivate = NULL;

	ms_queue_init(&queue);
	while(input->b_rptr != input->b_wptr) {
		uint32_t naluSize;
		mblk_t *nalu;
		memcpy(&naluSize, input->b_rptr, sizeof(uint32_t));
		input->b_rptr += sizeof(uint32_t);
		naluSize = ntohl(naluSize);
		nalu = allocb(naluSize, 0);
		memcpy(nalu->b_wptr, input->b_rptr, naluSize);
		nalu->b_wptr += naluSize;
		input->b_rptr += naluSize;
		if(buffer == NULL) {
			buffer = nalu;
		} else {
			concatb(buffer, nalu);
		}
	}
	if(isFirstFrame) {
		selectedCodecPrivate = obj->codecPrivate;
	} else if(codecPrivateData != NULL) {
		codecPrivate = H264Private_new(NULL, NULL);
		H264Private_load(codecPrivate, codecPrivateData);
		selectedCodecPrivate = codecPrivate;
	}
	if(selectedCodecPrivate != NULL) {
		for(it = H264Private_getSPS(selectedCodecPrivate); it != NULL; it = it->next) {
			mblk_t *sps = copymsg((mblk_t *)it->data);
			ms_queue_put(&queue, sps);
			ms_message("MKVPlayer: send SPS");
		}
		for(it = H264Private_getPPS(selectedCodecPrivate); it != NULL; it = it->next) {
			mblk_t *pps = copymsg((mblk_t *)it->data);
			ms_queue_put(&queue, pps);
			ms_message("MKVPlayer: send PPS");
		}
	}

	if(codecPrivate != NULL) H264Private_free(codecPrivate);

	for(bufferFrag = buffer; bufferFrag != NULL;) {
		mblk_t *curBuff = bufferFrag;
		bufferFrag = bufferFrag->b_cont;
		curBuff->b_cont = NULL;
		ms_queue_put(&queue, curBuff);
	}
	rfc3984_pack(&obj->rfc3984Context, &queue, output, mblk_get_timestamp_info(input));
	freemsg(input);
}

static void h264_module_get_private_data(const void *o, uint8_t **data, size_t *data_size) {
	const H264Module *obj = (const H264Module *)o;
	if (obj->codecPrivate == NULL) {
		ms_warning("MKVRecorder: No H264 private data available");
	} else {
		H264Private_serialize(obj->codecPrivate, data, data_size);
	}
}

static void h264_module_load_private_data(void *o, const uint8_t *data, size_t size) {
	H264Module *obj = (H264Module *)o;
	obj->codecPrivate = H264Private_new(NULL, NULL);
	H264Private_load(obj->codecPrivate, data);
}

/* h264 module description */
#ifdef _MSC_VER
static const ModuleDesc h264_module_desc = {
	"H264",
	"V_MPEG4/ISO/AVC",
	h264_module_new,
	h264_module_free,
	NULL,
	h264_module_preprocessing,
	h264_module_processing,
	h264_module_reverse,
	h264_module_get_private_data,
	h264_module_load_private_data,
	h264_is_key_frame
};
#else
static const ModuleDesc h264_module_desc = {
	.rfcName = "H264",
	.codecId = "V_MPEG4/ISO/AVC",
	.new_module = h264_module_new,
	.free_module = h264_module_free,
	.set = NULL,
	.preprocess = h264_module_preprocessing,
	.process = h264_module_processing,
	.reverse = h264_module_reverse,
	.get_private_data = h264_module_get_private_data,
	.load_private_data = h264_module_load_private_data,
	.is_key_frame = h264_is_key_frame
};
#endif

/*********************************************************************************************
 * VP8 module                                                                               *
 *********************************************************************************************/

#ifdef HAVE_VPX

typedef struct _Vp8Module {
	Vp8RtpFmtUnpackerCtx unpacker;
	Vp8RtpFmtPackerCtx packer;
	uint16_t cseq;
} Vp8Module;

static void *vp8_module_new(MSFactory *factory) {
	Vp8Module *obj = (Vp8Module *)ms_new0(Vp8Module, 1);
	vp8rtpfmt_unpacker_init(&obj->unpacker, NULL, FALSE, TRUE, FALSE);
	vp8rtpfmt_packer_init(&obj->packer);
	return obj;
}

static void vp8_module_free(void *obj) {
	Vp8Module *mod = (Vp8Module *)obj;
	vp8rtpfmt_unpacker_uninit(&mod->unpacker);
	vp8rtpfmt_packer_uninit(&mod->packer);
	ms_free(mod);
}

static int vp8_module_preprocess(void *obj, MSQueue *input, MSQueue *output) {
	Vp8RtpFmtUnpackerCtx *unpacker = (Vp8RtpFmtUnpackerCtx *)obj;
	Vp8RtpFmtFrameInfo infos;
	vp8rtpfmt_unpacker_feed(unpacker, input);
	return vp8rtpfmt_unpacker_get_frame(unpacker, output, &infos);
}

static mblk_t *vp8_module_process(void *obj, mblk_t *buffer, ms_bool_t *isKeyFrame, ms_bool_t *isVisible, uint8_t **codecPrivateData, size_t *codecPrivateSize) {
	uint8_t first_byte = *buffer->b_rptr;
	*isKeyFrame = !(first_byte & 0x01);
	*isVisible = first_byte & 0x10;
	return buffer;
}

static void vp8_module_reverse(MSFactory * f, void *obj, mblk_t *input, MSQueue *output, ms_bool_t isFirstFrame, const uint8_t *codecPrivateData, size_t codecPrivateSize) {
	Vp8Module *mod = (Vp8Module *)obj;
	bctbx_list_t *packer_input = NULL;
	Vp8RtpFmtPacket *packet = ms_new0(Vp8RtpFmtPacket, 1);
	MSQueue q;
	mblk_t *m;

	ms_queue_init(&q);
	packet->m = input;
	packet->pd = ms_new0(Vp8RtpFmtPayloadDescriptor, 1);
	packet->pd->start_of_partition = TRUE;
	packet->pd->non_reference_frame = TRUE;
	packet->pd->extended_control_bits_present = FALSE;
	packet->pd->pictureid_present = FALSE;
	packet->pd->pid = 0;
	mblk_set_marker_info(packet->m, TRUE);
	packer_input = bctbx_list_append(packer_input, packet);
	vp8rtpfmt_packer_process(&mod->packer, packer_input, &q, f);

	while((m = ms_queue_get(&q))) {
		mblk_set_cseq(m, mod->cseq++);
		ms_queue_put(output, m);
	}
}

static ms_bool_t vp8_module_is_keyframe(const mblk_t *frame) {
	uint8_t first_byte = *frame->b_rptr;
	return !(first_byte & 0x01);
}

/* VP8 module description */
#ifdef _MSC_VER
static const ModuleDesc vp8_module_desc = {
	"VP8",
	"V_VP8",
	vp8_module_new,
	vp8_module_free,
	NULL,
	vp8_module_preprocess,
	vp8_module_process,
	vp8_module_reverse,
	NULL,
	NULL,
	vp8_module_is_keyframe
};
#else
static const ModuleDesc vp8_module_desc = {
	.rfcName = "VP8",
	.codecId = "V_VP8",
	.new_module = vp8_module_new,
	.free_module = vp8_module_free,
	.set = NULL,
	.preprocess = vp8_module_preprocess,
	.process = vp8_module_process,
	.reverse = vp8_module_reverse,
	.get_private_data = NULL,
	.load_private_data = NULL,
	.is_key_frame = vp8_module_is_keyframe
};
#endif

#endif /* HAVE_VPX */

/*********************************************************************************************
 * µLaw module                                                                               *
 *********************************************************************************************/
/* WavPrivate */
typedef struct {
	uint16_t wFormatTag;
	uint16_t nbChannels;
	uint32_t nSamplesPerSec;
	uint32_t nAvgBytesPerSec;
	uint16_t nBlockAlign;
	uint16_t wBitsPerSample;
	uint16_t cbSize;
} WavPrivate;

static void wav_private_set(WavPrivate *data, const MSFmtDescriptor *obj) {
	uint16_t bitsPerSample = 8;
	uint16_t nbBlockAlign = (uint16_t)((bitsPerSample * obj->nchannels) / 8);
	uint32_t bitrate = bitsPerSample * obj->nchannels * obj->rate;

	data->wFormatTag = le_uint16((uint16_t)7);
	data->nbChannels = le_uint16((uint16_t)obj->nchannels);
	data->nSamplesPerSec = le_uint32((uint32_t)obj->rate);
	data->nAvgBytesPerSec = le_uint32(bitrate);
	data->nBlockAlign = le_uint16((uint16_t)nbBlockAlign);
	data->wBitsPerSample = le_uint16(bitsPerSample);
	data->cbSize = 0;
}

static void wav_private_serialize(const WavPrivate *obj, uint8_t **data, size_t *size) {
	*size = sizeof(WavPrivate);
	*data = (uint8_t *)ms_new0(uint8_t, *size);
	memcpy(*data, obj, *size);
}

static void wav_private_load(WavPrivate *obj, const uint8_t *data) {
	memcpy(obj, data, sizeof(WavPrivate));
}

/* µLaw module */
static void *mu_law_module_new(MSFactory *factory) {
	return ms_new0(WavPrivate, 1);
}

static void mu_law_module_free(void *o) {
	ms_free(o);
}

static void mu_law_module_set(void *o, const MSFmtDescriptor *fmt) {
	WavPrivate *obj = (WavPrivate *)o;
	wav_private_set(obj, fmt);
}

static void mu_law_module_get_private_data(const void *o, uint8_t **data, size_t *data_size) {
	const WavPrivate *obj = (const WavPrivate *)o;
	wav_private_serialize(obj, data, data_size);
}

static void mu_law_module_load_private(void *o, const uint8_t *data, size_t size) {
	WavPrivate *obj = (WavPrivate *)o;
	wav_private_load(obj, data);
}

/* µLaw module description */
#ifdef _MSC_VER
static const ModuleDesc mu_law_module_desc = {
	"pcmu",
	"A_MS/ACM",
	mu_law_module_new,
	mu_law_module_free,
	mu_law_module_set,
	NULL,
	NULL,
	NULL,
	mu_law_module_get_private_data,
	mu_law_module_load_private,
	NULL
};
#else
static const ModuleDesc mu_law_module_desc = {
	.rfcName = "pcmu",
	.codecId = "A_MS/ACM",
	.new_module = mu_law_module_new,
	.free_module = mu_law_module_free,
	.set = mu_law_module_set,
	.preprocess = NULL,
	.process = NULL,
	.reverse = NULL,
	.get_private_data = mu_law_module_get_private_data,
	.load_private_data = mu_law_module_load_private,
	.is_key_frame = NULL
};
#endif

/*********************************************************************************************
 * Opus module                                                                               *
 *********************************************************************************************/
// OpusCodecPrivate
typedef struct {
	uint8_t version;
	uint8_t channelCount;
	uint16_t preSkip;
	uint32_t inputSampleRate;
	uint16_t outputGain;
	uint8_t mappingFamily;
} OpusCodecPrivate;

static void opus_codec_private_init(OpusCodecPrivate *obj) {
	memset(obj, 0, sizeof(OpusCodecPrivate));
	obj->version = 1;
	obj->preSkip = le_uint16(3840); // 80ms at 48kHz
	obj->outputGain = le_uint16(0);
}

static void opus_codec_private_set(OpusCodecPrivate *obj, int nChannels, int inputSampleRate) {
	obj->channelCount = (uint8_t)nChannels;
	obj->inputSampleRate = le_uint32((uint32_t)inputSampleRate);
}

static void opus_codec_private_serialize(const OpusCodecPrivate *obj, uint8_t **data, size_t *size) {
	const char signature[9] = "OpusHead";
	*size = 19;
	*data = ms_new0(uint8_t, *size);
	memcpy(*data, signature, 8);
	memcpy((*data) + 8, obj, 11);
}

static void opus_codec_private_load(OpusCodecPrivate *obj, const uint8_t *data, size_t size) {
	memcpy(obj, data + 8, 11);
}

// OpusModule
static void *opus_module_new(MSFactory *factory) {
	OpusCodecPrivate *obj = ms_new0(OpusCodecPrivate, 1);
	opus_codec_private_init(obj);
	return obj;
}

static void opus_module_free(void *o) {
	OpusCodecPrivate *obj = (OpusCodecPrivate *)o;
	ms_free(obj);
}

static void opus_module_set(void *o, const MSFmtDescriptor *fmt) {
	OpusCodecPrivate *obj = (OpusCodecPrivate *)o;
	opus_codec_private_set(obj, fmt->nchannels, 0);
}

static void opus_module_get_private_data(const void *o, uint8_t **data, size_t *dataSize) {
	const OpusCodecPrivate *obj = (const OpusCodecPrivate *)o;
	opus_codec_private_serialize(obj, data, dataSize);
}

static void opus_module_load_private_data(void *o, const uint8_t *data, size_t size) {
	OpusCodecPrivate *obj = (OpusCodecPrivate *)o;
	opus_codec_private_load(obj, data, size);
}

#ifdef _MSC_VER
static const ModuleDesc opus_module_desc = {
	"opus",
	"A_OPUS",
	opus_module_new,
	opus_module_free,
	opus_module_set,
	NULL,
	NULL,
	NULL,
	opus_module_get_private_data,
	opus_module_load_private_data,
	NULL
};
#else
static const ModuleDesc opus_module_desc = {
	.rfcName = "opus",
	.codecId = "A_OPUS",
	.new_module = opus_module_new,
	.free_module = opus_module_free,
	.set = opus_module_set,
	.preprocess = NULL,
	.process = NULL,
	.reverse = NULL,
	.get_private_data = opus_module_get_private_data,
	.load_private_data = opus_module_load_private_data,
	.is_key_frame = NULL
};
#endif

/*********************************************************************************************
 * Modules list                                                                              *
 *********************************************************************************************/
#if 0
typedef enum {
    NONE_ID,
    H264_MOD_ID,
    VP8_MOD_ID,
    MU_LAW_MOD_ID,
    OPUS_MOD_ID
} ModuleId;
#endif

static const ModuleDesc *moduleDescs[] = {
	&h264_module_desc,
#ifdef HAVE_VPX
	&vp8_module_desc,
#endif /* HAVE_VPX */
	&mu_law_module_desc,
	&opus_module_desc,
	NULL
};

static const ModuleDesc *find_module_desc_from_rfc_name(const char *rfcName) {
	int i;
	for(i = 0; moduleDescs[i] && strcasecmp(moduleDescs[i]->rfcName, rfcName) != 0; i++);
	return moduleDescs[i];
}

static const ModuleDesc *find_module_desc_from_codec_id(const char *codecId) {
	int i;
	for(i = 0; moduleDescs[i] && strcmp(moduleDescs[i]->codecId, codecId) != 0; i++);
	return moduleDescs[i];
}

static const char *codec_id_to_rfc_name(const char *codecId) {
	const ModuleDesc *desc = find_module_desc_from_codec_id(codecId);
	return desc ? desc->rfcName : NULL;
}

/*********************************************************************************************
 * Module                                                                                    *
 *********************************************************************************************/
typedef struct {
	const ModuleDesc *desc;
	void *data;
} Module;

static Module *module_new(MSFactory *factory, const char *rfcName) {
	const ModuleDesc *desc = find_module_desc_from_rfc_name(rfcName);
	if(desc == NULL) {
		return NULL;
	} else {
		Module *module = (Module *)ms_new0(Module, 1);
		module->desc = desc;
		if(desc->new_module) module->data = desc->new_module(factory);
		return module;
	}
}

static void module_free(Module *module) {
	if(module->desc->free_module) module->desc->free_module(module->data);
	ms_free(module);
}

static void module_set(Module *module, const MSFmtDescriptor *format) {
	if(module->desc->set != NULL) {
		module->desc->set(module->data, format);
	}
}

static int module_preprocess(Module *module, MSQueue *input, MSQueue *output) {
	if(module->desc->preprocess != NULL) {
		return module->desc->preprocess(module->data, input, output);
	} else {
		mblk_t *buffer;
		while((buffer = ms_queue_get(input)) != NULL) {
			ms_queue_put(output, buffer);
		}
		return 0;
	}
}

static mblk_t *module_process(Module *module, mblk_t *buffer, ms_bool_t *isKeyFrame, ms_bool_t *isVisible, uint8_t **codecPrivateData, size_t *codecPrivateSize) {
	mblk_t *frame;
	if(module->desc->process != NULL) {
		frame = module->desc->process(module->data, buffer, isKeyFrame, isVisible, codecPrivateData, codecPrivateSize);
	} else {
		frame = buffer;
		*isKeyFrame = TRUE;
		*isVisible = TRUE;
		*codecPrivateData = NULL;
	}
	return frame;
}

static void module_reverse(MSFactory* f, Module *module, mblk_t *input, MSQueue *output, ms_bool_t isFirstFrame, const uint8_t *codecPrivateData, size_t codecPrivateSize) {
	if(module->desc->reverse == NULL) {
		ms_queue_put(output, input);
	} else {
		module->desc->reverse(f, module->data, input, output, isFirstFrame, codecPrivateData, codecPrivateSize);
	}
}

static void module_get_private_data(const Module *module, uint8_t **data, size_t *dataSize) {
	if(module->desc->get_private_data) {
		module->desc->get_private_data(module->data, data, dataSize);
	} else {
		*data = NULL;
		*dataSize = 0;
	}
}

static void module_load_private_data(Module *module, const uint8_t *data, size_t size) {
	if(module->desc->load_private_data) {
		module->desc->load_private_data(module->data, data, size);
	}
}

static ms_bool_t module_is_key_frame(const Module *module, const mblk_t *frame) {
	if(module->desc->is_key_frame) {
		return module->desc->is_key_frame(frame);
	} else {
		return TRUE;
	}
}

static const char *module_get_codec_id(const Module *module) {
	return module->desc->codecId;
}

/*********************************************************************************************
 * Matroska                                                                                  *
 *********************************************************************************************/
#define WRITE_DEFAULT_ELEMENT FALSE

static const timecode_t MKV_TIMECODE_SCALE = 1000000;
static const int MKV_DOCTYPE_VERSION = 4;
static const int MKV_DOCTYPE_READ_VERSION = 2;

extern const nodemeta LangStr_Class[];
extern const nodemeta UrlPart_Class[];
extern const nodemeta BufStream_Class[];
extern const nodemeta MemStream_Class[];
extern const nodemeta Streams_Class[];
extern const nodemeta File_Class[];
extern const nodemeta Stdio_Class[];
extern const nodemeta Matroska_Class[];
extern const nodemeta EBMLElement_Class[];
extern const nodemeta EBMLMaster_Class[];
extern const nodemeta EBMLBinary_Class[];
extern const nodemeta EBMLString_Class[];
extern const nodemeta EBMLInteger_Class[];
extern const nodemeta EBMLCRC_Class[];
extern const nodemeta EBMLDate_Class[];
extern const nodemeta EBMLVoid_Class[];

static void loadModules(nodemodule *modules) {
	NodeRegisterClassEx(modules, Streams_Class);
	NodeRegisterClassEx(modules, File_Class);
	NodeRegisterClassEx(modules, Matroska_Class);
	NodeRegisterClassEx(modules, EBMLElement_Class);
	NodeRegisterClassEx(modules, EBMLMaster_Class);
	NodeRegisterClassEx(modules, EBMLBinary_Class);
	NodeRegisterClassEx(modules, EBMLString_Class);
	NodeRegisterClassEx(modules, EBMLInteger_Class);
	NodeRegisterClassEx(modules, EBMLCRC_Class);
	NodeRegisterClassEx(modules, EBMLDate_Class);
	NodeRegisterClassEx(modules, EBMLVoid_Class);
}

typedef enum {
    MKV_OPEN_CREATE,
    MKV_OPEN_APPEND,
    MKV_OPEN_RO
} MatroskaOpenMode;

typedef struct {
	parsercontext *p;
	stream *output;
	ebml_master *header, *segment, *cluster, *info, *tracks, *metaSeek, *cues, *firstCluster, *currentCluster;
	matroska_seekpoint *infoMeta, *tracksMeta, *cuesMeta;
	matroska_block *currentBlock;
	timecode_t timecodeScale;
	filepos_t segmentInfoPosition;
	int nbClusters;
} Matroska;


static void matroska_init(Matroska *obj) {
	memset(obj, 0, sizeof(Matroska));
	obj->p = (parsercontext *)ms_new0(parsercontext, 1);
	ParserContext_Init(obj->p, NULL, NULL, NULL);
	loadModules((nodemodule *)obj->p);
	MATROSKA_Init((nodecontext *)obj->p);
	obj->segmentInfoPosition = -1;
	obj->timecodeScale = -1;
}

static void matroska_uninit(Matroska *obj) {
	MATROSKA_Done((nodecontext *)obj->p);
	ParserContext_Done(obj->p);
	ms_free(obj->p);
}

static int ebml_reading_profile(ebml_master *head) {
	tchar_t docType[9];
	int profile;
	int64_t docTypeReadVersion;
	tchar_t *matroska_doc_type =
#ifdef UNICODE
		L"matroska";
#else
		"matroska";
#endif
	tchar_t *webm_doc_type =
#ifdef UNICODE
		L"webm";
#else
		"webm";
#endif

	EBML_StringGet((ebml_string *)EBML_MasterGetChild(head, &EBML_ContextDocType), docType, sizeof(docType));
	docTypeReadVersion = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild(head, &EBML_ContextDocTypeReadVersion));

	if (tcscmp(docType, matroska_doc_type) == 0) {
		switch (docTypeReadVersion) {
		case 1:
			profile = PROFILE_MATROSKA_V1;
			break;
		case 2:
			profile = PROFILE_MATROSKA_V2;
			break;
		case 3:
			profile = PROFILE_MATROSKA_V3;
			break;
		case 4:
			profile = PROFILE_MATROSKA_V4;
			break;
		default:
			profile = -1;
		}
	} else if(tcscmp(docType, webm_doc_type)) {
		profile = PROFILE_WEBM;
	} else {
		profile = -1;
	}

	return profile;
}

static ms_bool_t matroska_create_file(Matroska *obj, const char path[]) {
	obj->header = (ebml_master *)EBML_ElementCreate(obj->p, &EBML_ContextHead, TRUE, NULL);
	obj->segment = (ebml_master *)EBML_ElementCreate(obj->p, &MATROSKA_ContextSegment, TRUE, NULL);
	obj->metaSeek = (ebml_master *)EBML_MasterAddElt(obj->segment, &MATROSKA_ContextSeekHead, FALSE);
	obj->infoMeta = (matroska_seekpoint *)EBML_MasterAddElt(obj->metaSeek, &MATROSKA_ContextSeek, TRUE);
	obj->tracksMeta = (matroska_seekpoint *)EBML_MasterAddElt(obj->metaSeek, &MATROSKA_ContextSeek, TRUE);
	obj->cuesMeta = (matroska_seekpoint *)EBML_MasterAddElt(obj->metaSeek, &MATROSKA_ContextSeek, TRUE);
	obj->info = (ebml_master *)EBML_MasterAddElt(obj->segment, &MATROSKA_ContextInfo, TRUE);
	obj->tracks = (ebml_master *)EBML_MasterAddElt(obj->segment, &MATROSKA_ContextTracks, FALSE);
	obj->cues = (ebml_master *)EBML_MasterAddElt(obj->segment, &MATROSKA_ContextCues, FALSE);
	obj->timecodeScale = MKV_TIMECODE_SCALE;

	MATROSKA_LinkMetaSeekElement(obj->infoMeta, (ebml_element *)obj->info);
	MATROSKA_LinkMetaSeekElement(obj->tracksMeta, (ebml_element *)obj->tracks);
	MATROSKA_LinkMetaSeekElement(obj->cuesMeta, (ebml_element *)obj->cues);

	return TRUE;
}

static ms_bool_t matroska_load_file(Matroska *obj) {
	int upperLevels = 0;
	ebml_parser_context readContext;
	ebml_element *elt;

	readContext.Context = &MATROSKA_ContextStream;
	readContext.EndPosition = INVALID_FILEPOS_T;
	readContext.Profile = 0;
	readContext.UpContext = NULL;

	obj->header = (ebml_master *)EBML_FindNextElement(obj->output, &readContext, &upperLevels, FALSE);
	EBML_ElementReadData(obj->header, obj->output, &readContext, FALSE, SCOPE_ALL_DATA, 0);
	readContext.Profile = ebml_reading_profile((ebml_master *)obj->header);

	obj->segment = (ebml_master *)EBML_FindNextElement(obj->output, &readContext, &upperLevels, FALSE);
	EBML_ElementReadData(obj->segment, obj->output, &readContext, FALSE, SCOPE_PARTIAL_DATA, 0);

	for(elt = EBML_MasterChildren(obj->segment); elt != NULL; elt = EBML_MasterNext(elt)) {
		if(EBML_ElementIsType(elt, &MATROSKA_ContextSeekHead)) {
			matroska_seekpoint *seekPoint = NULL;
			obj->metaSeek = (ebml_master *)elt;

			for(seekPoint = (matroska_seekpoint *)EBML_MasterChildren(obj->metaSeek); seekPoint != NULL; seekPoint = (matroska_seekpoint *)EBML_MasterNext(seekPoint)) {
				if(MATROSKA_MetaSeekIsClass(seekPoint, &MATROSKA_ContextInfo)) {
					obj->infoMeta = seekPoint;
				} else if(MATROSKA_MetaSeekIsClass(seekPoint, &MATROSKA_ContextTracks)) {
					obj->tracksMeta = seekPoint;
				} else if(MATROSKA_MetaSeekIsClass(seekPoint, &MATROSKA_ContextCues)) {
					obj->cuesMeta = seekPoint;
				}
			}
		} else if(EBML_ElementIsType(elt, &MATROSKA_ContextInfo)) {
			obj->info = (ebml_master *)elt;
			obj->timecodeScale = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild(obj->info, &MATROSKA_ContextTimecodeScale));
			MATROSKA_LinkMetaSeekElement(obj->infoMeta, (ebml_element *)obj->info);
		} else if(EBML_ElementIsType(elt, &MATROSKA_ContextTracks)) {
			obj->tracks = (ebml_master *)elt;
			MATROSKA_LinkMetaSeekElement(obj->tracksMeta, (ebml_element *)obj->tracks);
		} else if(EBML_ElementIsType(elt, &MATROSKA_ContextCues)) {
			obj->cues = (ebml_master *)elt;
			MATROSKA_LinkMetaSeekElement(obj->cuesMeta, (ebml_element *)obj->cues);
		} else if(EBML_ElementIsType(elt, &MATROSKA_ContextCluster)) {
			obj->cluster = (ebml_master *)elt;
			if(obj->nbClusters == 0) {
				obj->firstCluster = obj->cluster;
			}
			MATROSKA_LinkClusterBlocks((matroska_cluster *)obj->cluster, obj->info, obj->tracks, FALSE);
			obj->nbClusters++;
		}
	}

	/* Create an empty MetaSeek table if no has been found and create
	   the missing entries */
	if(obj->metaSeek == NULL) obj->metaSeek = (ebml_master *)EBML_MasterAddElt(obj->segment, &MATROSKA_ContextSeekHead, FALSE);
	if(obj->infoMeta == NULL) {
		obj->infoMeta = (matroska_seekpoint *)EBML_MasterAddElt(obj->metaSeek, &MATROSKA_ContextSeek, TRUE);
		MATROSKA_LinkMetaSeekElement(obj->infoMeta, (ebml_element *)obj->info);
	}
	if(obj->tracksMeta == NULL) {
		obj->tracksMeta = (matroska_seekpoint *)EBML_MasterAddElt(obj->metaSeek, &MATROSKA_ContextSeek, TRUE);
		MATROSKA_LinkMetaSeekElement(obj->tracksMeta, (ebml_element *)obj->tracks);
	}
	if(obj->cuesMeta == NULL) {
		obj->cuesMeta = (matroska_seekpoint *)EBML_MasterAddElt(obj->metaSeek, &MATROSKA_ContextSeek, TRUE);
		MATROSKA_LinkMetaSeekElement(obj->cuesMeta, (ebml_element *)obj->cues);
	}
	return TRUE;
}

static int matroska_open_file(Matroska *obj, const char *path, MatroskaOpenMode mode) {
	int err = 0;
	tchar_t *tpath;

#ifdef _MSC_VER
	wchar_t wpath[MAX_PATH + 1];
#ifndef UNICODE
	char mbpath[MAX_PATH + 1];
#endif
	if(MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH + 1) == 0) {
		ms_error("Could not convert %s into UTF-16", path);
		return -1;
	}
#ifdef UNICODE
	tpath = (tchar_t *)wpath;
#else
	if(WideCharToMultiByte(CP_ACP, 0, wpath, -1, mbpath, MAX_PATH + 1, NULL, NULL) == 0) {
		ms_error("Could not convert %s from UTF-16 to ACP", path);
		return -1;
	}
	tpath = (tchar_t *)mbpath;
#endif
#else
	tpath = (tchar_t *)path;
#endif

	switch(mode) {
		case MKV_OPEN_CREATE:
			if((obj->output = StreamOpen(obj->p, tpath, SFLAG_WRONLY | SFLAG_CREATE)) == NULL) {
				err = -2;
				break;
			}
			if(!matroska_create_file(obj, path)) {
				err = -3;
			}
			break;

		case MKV_OPEN_APPEND:
			if((obj->output = StreamOpen(obj->p, tpath, SFLAG_REOPEN)) == NULL) {
				err = -2;
				break;
			}
			if(!matroska_load_file(obj)) {
				err = -3;
				break;
			}
			if(obj->cues == NULL) {
				obj->cues = (ebml_master *)EBML_ElementCreate(obj->p, &MATROSKA_ContextCues, FALSE, NULL);
			}
			if(obj->cluster == NULL) {
				Stream_Seek(obj->output, 0, SEEK_END);
			} else {
				Stream_Seek(obj->output, EBML_ElementPositionEnd((ebml_element *)obj->cluster), SEEK_SET);
			}
			break;

		case MKV_OPEN_RO:
			if((obj->output = StreamOpen(obj->p, tpath, SFLAG_RDONLY)) == NULL) {
				err = -2;
				break;
			}
			if(!matroska_load_file(obj)) {
				err = -3;
				break;
			}
			break;

		default:
			err = -1;
			break;
	}
	return err;
}

static void matroska_close_file(Matroska *obj) {
	if(obj->output != NULL) {
		StreamClose(obj->output);
	}
	if(obj->header != NULL) {
		Node_Release((node *)obj->header);
	}
	if(obj->segment != NULL) {
		Node_Release((node *)obj->segment);
	}
	obj->output = NULL;
	obj->header = NULL;
	obj->header = NULL;
	obj->segment = NULL;
	obj->cluster = NULL;
	obj->info = NULL;
	obj->tracks = NULL;
	obj->metaSeek = NULL;
	obj->cues = NULL;
	obj->firstCluster = NULL;
	obj->currentCluster = NULL;
	obj->infoMeta = NULL;
	obj->tracksMeta = NULL;
	obj->cuesMeta = NULL;
	obj->currentBlock = NULL;
	obj->timecodeScale = -1;
	obj->segmentInfoPosition = -1;
	obj->nbClusters = 0;
}

static void matroska_set_doctype_version(Matroska *obj, int doctypeVersion, int doctypeReadVersion) {
	EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(obj->header, &EBML_ContextDocTypeVersion), doctypeVersion);
	EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(obj->header, &EBML_ContextDocTypeReadVersion), doctypeReadVersion);
}

static void matroska_write_ebml_header(Matroska *obj) {
	EBML_ElementRender((ebml_element *)obj->header, obj->output, TRUE, FALSE, FALSE, NULL);
}

static int matroska_set_segment_info(Matroska *obj, const char writingApp[], const char muxingApp[], double duration) {
	if(obj->timecodeScale == -1) {
		return -1;
	} else {
		EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(obj->info, &MATROSKA_ContextTimecodeScale), obj->timecodeScale);
		EBML_StringSetValue((ebml_string *)EBML_MasterGetChild(obj->info, &MATROSKA_ContextMuxingApp), muxingApp);
		EBML_StringSetValue((ebml_string *)EBML_MasterGetChild(obj->info, &MATROSKA_ContextWritingApp), writingApp);
		EBML_FloatSetValue((ebml_float *)EBML_MasterGetChild(obj->info, &MATROSKA_ContextDuration), duration);
		return 0;
	}
}

static timecode_t matroska_get_duration(const Matroska *obj) {
	return (timecode_t)EBML_FloatValue((ebml_float *)EBML_MasterGetChild(obj->info, &MATROSKA_ContextDuration));
}

timecode_t matroska_get_timecode_scale(const Matroska *obj) {
	return obj->timecodeScale;
}

static void updateElementHeader(ebml_element *element, stream *file) {
	filepos_t initial_pos = Stream_Seek(file, 0, SEEK_CUR);
	Stream_Seek(file, EBML_ElementPosition(element), SEEK_SET);
	EBML_ElementUpdateSize(element, WRITE_DEFAULT_ELEMENT, FALSE);
	EBML_ElementRenderHead(element, file, FALSE, NULL);
	Stream_Seek(file, initial_pos, SEEK_SET);
}

static void matroska_start_segment(Matroska *obj) {
	EBML_ElementSetSizeLength((ebml_element *)obj->segment, 8);
	EBML_ElementRenderHead((ebml_element *)obj->segment, obj->output, FALSE, NULL);
}

static size_t matroska_write_zeros(Matroska *obj, size_t nbZeros) {
	uint8_t *data = (uint8_t *)ms_new0(uint8_t, nbZeros);
	size_t written = 0;

	if(data == NULL) {
		return 0;
	}
	Stream_Write(obj->output, data, nbZeros, &written);
	ms_free(data);
	return written;
}

static void matroska_mark_segment_info_position(Matroska *obj) {
	obj->segmentInfoPosition = Stream_Seek(obj->output, 0, SEEK_CUR);
}

static int matroska_go_to_segment_info_mark(Matroska *obj) {
	if(obj->segmentInfoPosition == -1) {
		return -1;
	}
	Stream_Seek(obj->output, obj->segmentInfoPosition, SEEK_SET);
	return 0;
}

static void matroska_go_to_file_end(Matroska *obj) {
	Stream_Seek(obj->output, 0, SEEK_END);
}

static void matroska_go_to_segment_begin(Matroska *obj) {
	Stream_Seek(obj->output, EBML_ElementPositionData((ebml_element *)obj->segment), SEEK_SET);
}

void matroska_go_to_last_cluster_end(Matroska *obj) {
	Stream_Seek(obj->output, EBML_ElementPositionEnd((ebml_element *)obj->cluster), SEEK_SET);
}

static void matroska_go_to_segment_info_begin(Matroska *obj) {
	Stream_Seek(obj->output, EBML_ElementPosition((ebml_element *)obj->info), SEEK_SET);
}

timecode_t matroska_block_get_timestamp(const Matroska *obj) {
	return MATROSKA_BlockTimecode((matroska_block *)obj->currentBlock) / obj->timecodeScale;
}

int matroska_block_get_track_num(const Matroska *obj) {
	return MATROSKA_BlockTrackNum((matroska_block *)obj->currentBlock);
}

static int ebml_element_cmp_position(const void *a, const void *b) {
	return (int)(EBML_ElementPosition((ebml_element *)a) - EBML_ElementPosition((ebml_element *)b));
}

static void ebml_master_sort(ebml_master *master_elt) {
	bctbx_list_t *elts = NULL;
	bctbx_list_t *it = NULL;
	ebml_element *elt = NULL;

	for(elt = EBML_MasterChildren(master_elt); elt != NULL; elt = EBML_MasterNext(elt)) {
		elts = bctbx_list_insert_sorted(elts, elt, (bctbx_compare_func)ebml_element_cmp_position);
	}
	EBML_MasterClear(master_elt);

	for(it = elts; it != NULL; it = bctbx_list_next(it)) {
		EBML_MasterAppend(master_elt, (ebml_element *)it->data);
	}
	bctbx_list_free(elts);
}

static int ebml_master_fill_blanks(stream *output, ebml_master *master) {
	bctbx_list_t *voids = NULL;
	bctbx_list_t *it = NULL;
	ebml_element *elt1 = NULL, *elt2 = NULL;

	for(elt1 = EBML_MasterChildren(master), elt2 = EBML_MasterNext(elt1); elt2 != NULL; elt1 = EBML_MasterNext(elt1), elt2 = EBML_MasterNext(elt2)) {
		filepos_t elt1_end_pos = EBML_ElementPositionEnd(elt1);
		filepos_t elt2_pos = EBML_ElementPosition(elt2);
		int interval = (int)(elt2_pos - elt1_end_pos);
		if(interval < 0) {
			return -1; // Elements are neither contigus or distinct.
		} else if(interval == 0) {
			// Nothing to do. Elements are contigus.
		} else if(interval > 0 && interval < 2) {
			return -2; // Not enough space to write a void element.
		} else {
			ebml_element *voidElt = EBML_ElementCreate(master, &EBML_ContextEbmlVoid, TRUE, NULL);
			EBML_VoidSetFullSize(voidElt, interval);
			Stream_Seek(output, elt1_end_pos, SEEK_SET);
			EBML_ElementRender(voidElt, output, FALSE, FALSE, FALSE, NULL);
			voids = bctbx_list_append(voids, voidElt);
		}
	}

	for(it = voids; it != NULL; it = bctbx_list_next(it)) {
		EBML_MasterAppend(master, (ebml_element *)it->data);
	}
	bctbx_list_free(voids);
	return 0;
}

static void ebml_master_delete_empty_elements(ebml_master *master) {
	ebml_element *child = NULL;
	for(child = EBML_MasterChildren(master); child != NULL; child = EBML_MasterNext(child)) {
		if(EBML_ElementDataSize(child, WRITE_DEFAULT_ELEMENT) <= 0) {
			EBML_MasterRemove(master, child);
			NodeDelete((node *)child);
		}
	}
}

static int matroska_close_segment(Matroska *obj) {
	filepos_t initialPos = Stream_Seek(obj->output, 0, SEEK_CUR);
	EBML_ElementUpdateSize(obj->segment, WRITE_DEFAULT_ELEMENT, FALSE);
	ebml_master_delete_empty_elements(obj->segment);
	ebml_master_sort(obj->segment);
	if(ebml_master_fill_blanks(obj->output, obj->segment) < 0) {
		return -1;
	}
	updateElementHeader((ebml_element *)obj->segment, obj->output);
	Stream_Seek(obj->output, initialPos, SEEK_SET);
	return 0;
}

static ebml_master *matroska_find_track_entry(const Matroska *obj, int trackNum) {
	ebml_element *trackEntry = NULL;
	for(trackEntry = EBML_MasterChildren(obj->tracks);
	        trackEntry != NULL && EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)trackEntry, &MATROSKA_ContextTrackNumber)) != trackNum;
	        trackEntry = EBML_MasterNext(trackEntry));
	return (ebml_master *)trackEntry;
}

static int matroska_get_codec_private(const Matroska *obj, int trackNum, const uint8_t **data, size_t *length) {
	ebml_master *trackEntry = matroska_find_track_entry(obj, trackNum);
	ebml_binary *codecPrivate;
	if(trackEntry == NULL)
		return -1;
	codecPrivate = (ebml_binary *)EBML_MasterFindChild(trackEntry, &MATROSKA_ContextCodecPrivate);
	if(codecPrivate == NULL)
		return -2;

	*length = (size_t)EBML_ElementDataSize((ebml_element *)codecPrivate, FALSE);
	*data =  EBML_BinaryGetData(codecPrivate);
	if(*data == NULL)
		return -3;
	else
		return 0;
}

static void matroska_start_cluster(Matroska *obj, timecode_t clusterTimecode) {
	obj->cluster = (ebml_master *)EBML_MasterAddElt(obj->segment, &MATROSKA_ContextCluster, TRUE);
	if(obj->nbClusters == 0) {
		obj->firstCluster = obj->cluster;
	}
	EBML_ElementSetSizeLength((ebml_element *)obj->cluster, 8);
	EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(obj->cluster, &MATROSKA_ContextTimecode), clusterTimecode);
	EBML_ElementRender((ebml_element *)obj->cluster, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
	obj->nbClusters++;
}

static void matroska_close_cluster(Matroska *obj) {
	ebml_element *block = NULL;
	if(obj->cluster == NULL) {
		return;
	} else {
		block = EBML_MasterFindChild(obj->cluster, &MATROSKA_ContextSimpleBlock);
	}
	if(block == NULL) {
		ebml_element *voidElt = EBML_ElementCreate(obj->p, &EBML_ContextEbmlVoid, FALSE, NULL);
		EBML_MasterAppend(obj->segment, voidElt);
		EBML_VoidSetFullSize(voidElt, EBML_ElementFullSize((ebml_element *)obj->cluster, WRITE_DEFAULT_ELEMENT));
		Stream_Seek((ebml_master *)obj->output, EBML_ElementPosition((ebml_element *)obj->cluster), SEEK_SET);
		EBML_ElementRender(voidElt, obj->output, FALSE, FALSE, FALSE, NULL);
		EBML_MasterRemove(obj->segment, (ebml_element *)obj->cluster);
		NodeDelete((node *)obj->cluster);
		obj->cluster = NULL;
	} else {
		updateElementHeader((ebml_element *)obj->cluster, obj->output);
	}
}

static int matroska_clusters_count(const Matroska *obj) {
	return obj->nbClusters;
}

static timecode_t matroska_current_cluster_timecode(const Matroska *obj) {
	return EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild(obj->cluster, &MATROSKA_ContextTimecode));
}

static ebml_master *matroska_find_track(const Matroska *obj, int trackNum) {
	ebml_element *elt = NULL;
	for(elt = EBML_MasterChildren(obj->tracks);
	        elt != NULL && EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(elt, &MATROSKA_ContextTrackNumber)) != trackNum;
	        elt = EBML_MasterNext(elt));
	return (ebml_master *)elt;
}

static int matroska_add_track(Matroska *obj, int trackNum, const char codecID[]) {
	ebml_master *track = matroska_find_track(obj, trackNum);
	if(track != NULL) {
		return -1;
	} else {
		track = (ebml_master *)EBML_MasterAddElt(obj->tracks, &MATROSKA_ContextTrackEntry, FALSE);
		if(track == NULL) {
			return -2;
		}
		EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextTrackNumber), trackNum);
		EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextTrackUID), trackNum);
		EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextFlagEnabled), 1);
		EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextFlagDefault), 1);
		EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextFlagForced), 0);
		EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextFlagLacing), 0);
		EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextMinCache), 1);
		EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextMaxBlockAdditionID), 0);
		EBML_StringSetValue((ebml_string *)EBML_MasterGetChild(track, &MATROSKA_ContextCodecID), codecID);
		EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextCodecDecodeAll), 0);
		return 0;
	}
}

static int matroska_del_track(Matroska *obj, int trackNum) {
	ebml_master *track = matroska_find_track(obj, trackNum);
	if(track == NULL) {
		return -1;
	} else {
		if(EBML_MasterRemove(obj->tracks, (ebml_element *)track) != ERR_NONE) {
			return -2;
		} else {
			NodeDelete((node *)track);
			return 0;
		}
	}
}

static int matroska_track_set_codec_private(Matroska *obj, int trackNum, const uint8_t *data, size_t dataSize) {
	ebml_master *track = matroska_find_track(obj, trackNum);
	if(track == NULL) {
		return -1;
	} else {
		ebml_binary *codecPrivate = (ebml_binary *)EBML_MasterGetChild(track, &MATROSKA_ContextCodecPrivate);
		if(EBML_BinarySetData(codecPrivate, data, dataSize) != ERR_NONE) {
			return -2;
		} else {
			return 0;
		}
	}
}

static int matroska_track_set_info(Matroska *obj, int trackNum, const MSFmtDescriptor *fmt) {
	ebml_master *track = matroska_find_track(obj, trackNum);
	ebml_master *videoInfo;
	ebml_master *audioInfo;
	if(track == NULL) {
		return -1;
	} else {
		switch(fmt->type) {
			case MSVideo:
				EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextTrackType), TRACK_TYPE_VIDEO);
				videoInfo = (ebml_master *)EBML_MasterGetChild(track, &MATROSKA_ContextVideo);
				EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(videoInfo, &MATROSKA_ContextFlagInterlaced), 0);
				EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(videoInfo, &MATROSKA_ContextPixelWidth), fmt->vsize.width);
				EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(videoInfo, &MATROSKA_ContextPixelHeight), fmt->vsize.height);
				break;

			case MSAudio:
				EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextTrackType), TRACK_TYPE_AUDIO);
				audioInfo = (ebml_master *)EBML_MasterGetChild(track, &MATROSKA_ContextAudio);
				EBML_FloatSetValue((ebml_float *)EBML_MasterGetChild(audioInfo, &MATROSKA_ContextSamplingFrequency), fmt->rate);
				EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(audioInfo, &MATROSKA_ContextChannels), fmt->nchannels);
				break;
			default:
				ms_error("matroska_track_set_info: format type '%s' not handled.", ms_format_type_to_string(fmt->type));
				break;
		}
		return 0;
	}
}

static ms_bool_t matroska_track_check_block_presence(Matroska *obj, int trackNum) {
	ebml_element *elt1 = NULL;
	for(elt1 = EBML_MasterChildren(obj->segment); elt1 != NULL; elt1 = EBML_MasterNext(elt1)) {
		if(EBML_ElementIsType(elt1, &MATROSKA_ContextCluster)) {
			ebml_element *cluster = elt1, *elt2 = NULL;
			for(elt2 = EBML_MasterChildren(cluster); elt2 != NULL; elt2 = EBML_MasterNext(elt2)) {
				if(EBML_ElementIsType(elt2, &MATROSKA_ContextSimpleBlock)) {
					matroska_block *block = (matroska_block *)elt2;
					if(MATROSKA_BlockTrackNum(block) == trackNum) {
						break;
					}
				}
			}
			if(elt2 != NULL) {
				break;
			}
		}
	}
	if(elt1 == NULL) {
		return FALSE;
	} else {
		return TRUE;
	}
}

static int matroska_add_cue(Matroska *obj, matroska_block *block) {
	matroska_cuepoint *cue = (matroska_cuepoint *)EBML_MasterAddElt(obj->cues, &MATROSKA_ContextCuePoint, TRUE);
	if(cue == NULL) {
		return -1;
	} else {
		MATROSKA_LinkCuePointBlock(cue, block);
		MATROSKA_LinkCueSegmentInfo(cue, obj->info);
		MATROSKA_CuePointUpdate(cue, (ebml_element *)obj->segment);
		return 0;
	}
}

static void matroska_write_segment_info(Matroska *obj) {
	EBML_ElementRender((ebml_element *)obj->info, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
	MATROSKA_MetaSeekUpdate(obj->infoMeta);
}

static void matroska_write_tracks(Matroska *obj) {
	EBML_ElementRender((ebml_element *)obj->tracks, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
	MATROSKA_MetaSeekUpdate(obj->tracksMeta);
}

static int matroska_write_cues(Matroska *obj) {
	if(EBML_MasterChildren(obj->cues) != NULL) {
		EBML_ElementRender((ebml_element *)obj->cues, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
		MATROSKA_MetaSeekUpdate(obj->cuesMeta);
		return 0;
	} else {
		EBML_MasterRemove(obj->segment, (ebml_element *)obj->cues);
		EBML_MasterRemove(obj->metaSeek, (ebml_element *)obj->cuesMeta);
		Node_Release((node *)obj->cues);
		Node_Release((node *)obj->cuesMeta);
		return -1;
	}
}

static void matroska_write_metaSeek(Matroska *obj) {
	EBML_ElementRender((ebml_element *)obj->metaSeek, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
}

timecode_t matroska_get_current_cluster_timestamp(const Matroska *obj) {
	return (timecode_t)EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild(obj->cluster, &MATROSKA_ContextTimecode));
}

static matroska_block *matroska_write_block(Matroska *obj, const matroska_frame *m_frame, uint16_t trackNum, ms_bool_t isKeyFrame, ms_bool_t isVisible, const uint8_t *codecPrivateData, size_t codecPrivateDataSize) {
	matroska_block *block = NULL;
	ebml_master *blockGroup = NULL;
	timecode_t clusterTimecode;

	if(obj->timecodeScale == -1) {
		block = NULL;
	} else {
		if(codecPrivateData == NULL) {
			block = (matroska_block *)EBML_MasterAddElt(obj->cluster, &MATROSKA_ContextSimpleBlock, FALSE);
		} else {
			ebml_binary *codecPrivateElt = NULL;
			blockGroup = (ebml_master *)EBML_MasterAddElt(obj->cluster, &MATROSKA_ContextBlockGroup, FALSE);
			block = (matroska_block *)EBML_MasterAddElt(blockGroup, &MATROSKA_ContextBlock, FALSE);
			codecPrivateElt = (ebml_binary *)EBML_MasterAddElt(blockGroup, &MATROSKA_ContextCodecState, FALSE);
			EBML_BinarySetData(codecPrivateElt, codecPrivateData, codecPrivateDataSize);
		}
		block->TrackNumber = trackNum;
		MATROSKA_LinkBlockWithReadTracks(block, obj->tracks, TRUE);
		MATROSKA_LinkBlockWriteSegmentInfo(block, obj->info);
		MATROSKA_BlockSetKeyframe(block, isKeyFrame);
		MATROSKA_BlockSetDiscardable(block, FALSE);
		clusterTimecode = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild(obj->cluster, &MATROSKA_ContextTimecode));
		MATROSKA_BlockAppendFrame(block, m_frame, clusterTimecode * obj->timecodeScale);
		if(codecPrivateData == NULL) {
			EBML_ElementRender((ebml_element *)block, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
		} else {
			EBML_ElementRender((ebml_element *)blockGroup, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
		}
		MATROSKA_BlockReleaseData(block, TRUE);
	}
	return block;
}

/*********************************************************************************************
 * Muxer                                                                                     *
 *********************************************************************************************/
typedef struct {
	uint16_t nqueues;
	MSQueue *queues;
} Muxer;

static void muxer_init(Muxer *obj, uint16_t ninputs) {
	uint16_t i;
	obj->nqueues = ninputs;
	obj->queues = (MSQueue *)ms_new0(MSQueue, ninputs);
	for(i = 0; i < ninputs; i++) {
		ms_queue_init(&obj->queues[i]);
	}
}

static void muxer_empty_internal_queues(Muxer *obj) {
	uint16_t i;
	for(i = 0; i < obj->nqueues; i++) {
		ms_queue_flush(&obj->queues[i]);
	}
}

static void muxer_uninit(Muxer *obj) {
	muxer_empty_internal_queues(obj);
	ms_free(obj->queues);
}

static void muxer_put_buffer(Muxer *obj, mblk_t *buffer, uint16_t pin) {
	ms_queue_put(&obj->queues[pin], buffer);
}

static mblk_t *muxer_get_buffer(Muxer *obj, uint16_t *pin) {
	uint16_t i;
	MSQueue *minQueue = NULL;

	for(i = 0; i < obj->nqueues; i++) {
		if(!ms_queue_empty(&obj->queues[i])) {
			if(minQueue == NULL) {
				minQueue = &obj->queues[i];
			} else {
				uint32_t t1 = mblk_get_timestamp_info(qfirst(&minQueue->q));
				uint32_t t2 = mblk_get_timestamp_info(qfirst(&(&obj->queues[i])->q));
				if(t2 < t1) {
					minQueue = &obj->queues[i];
				}
			}
		}
	}
	if(minQueue == NULL) {
		return NULL;
	} else {
		*pin = (uint16_t)(minQueue - obj->queues);
		return ms_queue_get(minQueue);
	}
}

/*********************************************************************************************
 * Timestamp corrector                                                                       *
 *********************************************************************************************/
typedef struct {
	timecode_t globalOrigin;
	timecode_t globalOffset;
	timecode_t *offsetList;
	ms_bool_t globalOffsetIsSet;
	ms_bool_t *offsetIsSet;
	int nPins;
	const MSTicker *ticker;
} TimeCorrector;

static void time_corrector_init(TimeCorrector *obj, int nPins) {
	obj->globalOrigin = 0;
	obj->nPins = nPins;
	obj->offsetList = (timecode_t *)ms_new0(timecode_t, obj->nPins);
	obj->offsetIsSet = (ms_bool_t *)ms_new0(ms_bool_t, obj->nPins);
	obj->globalOffsetIsSet = FALSE;
	obj->ticker = NULL;
}

static void time_corrector_uninit(TimeCorrector *obj) {
	ms_free(obj->offsetList);
	ms_free(obj->offsetIsSet);
}

static void time_corrector_set_origin(TimeCorrector *obj, timecode_t origin) {
	obj->globalOrigin = origin;
}

static void time_corrector_set_ticker(TimeCorrector *obj, const MSTicker *ticker) {
	obj->ticker = ticker;
}

static void time_corrector_reset(TimeCorrector *obj) {
	int i;
	obj->globalOffsetIsSet = FALSE;
	for(i = 0; i < obj->nPins; i++) {
		obj->offsetIsSet[i] = FALSE;
	}
}

static void time_corrector_proceed(TimeCorrector *obj, mblk_t *buffer, int pin) {
	if(!obj->globalOffsetIsSet) {
		obj->globalOffset = obj->globalOrigin - obj->ticker->time;
		obj->globalOffsetIsSet = TRUE;
	}
	if(!obj->offsetIsSet[pin]) {
		uint64_t origin = obj->ticker->time + obj->globalOffset;
		obj->offsetList[pin] = origin - mblk_get_timestamp_info(buffer);
		obj->offsetIsSet[pin] = TRUE;
	}
	mblk_set_timestamp_info(buffer, mblk_get_timestamp_info(buffer) + (uint32_t)obj->offsetList[pin]);
}

/*********************************************************************************************
 * TimeLoopCanceler																			 *
 *********************************************************************************************/
typedef struct {
	int64_t offset;
	int module;
	int64_t prevTimestamp;
} TimeLoopCanceler;

static void time_loop_canceler_init(TimeLoopCanceler *obj) {
	obj->offset = -1;
	obj->module = 0;
	obj->prevTimestamp = -1;
}

static TimeLoopCanceler *time_loop_canceler_new(void) {
	TimeLoopCanceler *obj = ms_new0(TimeLoopCanceler, 1);
	time_loop_canceler_init(obj);
	return obj;
}

static uint32_t time_loop_canceler_apply(TimeLoopCanceler *obj, uint32_t timestamp) {
	if(obj->offset < 0) {
		obj->offset = timestamp;
	}
	if(obj->prevTimestamp >= 0) {
		int64_t delta = (int64_t)timestamp - obj->prevTimestamp;
		if(delta < 0 && -delta >= (int64_t)1 << 31) {
			obj->module++;
		}
	}
	obj->prevTimestamp = timestamp;
	return (uint32_t)((uint64_t)timestamp + (uint64_t)0x00000000ffffffff * (uint64_t)obj->module - (uint64_t)obj->offset);
}

/*********************************************************************************************
 * MKV Recorder Filter                                                                       *
 *********************************************************************************************/
#define CLUSTER_MAX_DURATION 5000

typedef struct {
	Matroska file;
	timecode_t duration;
	MatroskaOpenMode openMode;
	MSRecorderState state;
	Muxer muxer;
	TimeCorrector timeCorrector;
	uint64_t lastFirTime;
	const MSFmtDescriptor **inputDescsList;
	Module **modulesList;
	TimeLoopCanceler **timeLoopCancelers;
	ms_bool_t needKeyFrame;
	ms_bool_t tracksInitialized;
} MKVRecorder;

static void recorder_init(MSFilter *f) {
	MKVRecorder *obj = (MKVRecorder *)ms_new0(MKVRecorder, 1);

	ms_message("MKVRecorder: initialisation");
	matroska_init(&obj->file);

	obj->state = MSRecorderClosed;
	obj->needKeyFrame = TRUE;

	muxer_init(&obj->muxer, (uint16_t)f->desc->ninputs);

	obj->inputDescsList = (const MSFmtDescriptor **)ms_new0(const MSFmtDescriptor *, f->desc->ninputs);
	obj->modulesList = (Module **)ms_new0(Module *, f->desc->ninputs);
	obj->timeLoopCancelers = (TimeLoopCanceler **)ms_new0(TimeLoopCanceler *, f->desc->ninputs);
	obj->lastFirTime = (uint64_t) - 1;
	time_corrector_init(&obj->timeCorrector, f->desc->ninputs);

	f->data = obj;
}

static void recorder_uninit(MSFilter *f) {
	MKVRecorder *obj = (MKVRecorder *)f->data;
	int i;

	if(obj->state != MSRecorderClosed) {
		recorder_close(f, NULL);
	}
	muxer_uninit(&obj->muxer);
	matroska_uninit(&obj->file);
	for(i = 0; i < f->desc->ninputs; i++) {
		if(obj->modulesList[i] != NULL) module_free(obj->modulesList[i]);
		if(f->inputs[i] != NULL) ms_queue_flush(f->inputs[i]);
		if(obj->timeLoopCancelers[i] != NULL) ms_free(obj->timeLoopCancelers[i]);
	}
	time_corrector_uninit(&obj->timeCorrector);
	ms_free(obj->timeLoopCancelers);
	ms_free(obj->modulesList);
	ms_free((void *)obj->inputDescsList);
	ms_free(obj);
	ms_message("MKVRecorder: destroyed");
}

static void recorder_preprocess(MSFilter *f) {
	MKVRecorder *obj = (MKVRecorder *)f->data;
	ms_filter_lock(f);
	time_corrector_set_ticker(&obj->timeCorrector, f->ticker);
	ms_filter_unlock(f);
}

static void recorder_postprocess(MSFilter *f) {
	MKVRecorder *obj = (MKVRecorder *)f->data;
	ms_filter_lock(f);
	time_corrector_set_ticker(&obj->timeCorrector, NULL);
	ms_filter_unlock(f);
}

static matroska_block *write_frame(MKVRecorder *obj, mblk_t *buffer, uint16_t pin) {
	ms_bool_t isKeyFrame;
	ms_bool_t isVisible;
	uint8_t *codecPrivateData = NULL;
	size_t codecPrivateSize;
	matroska_block *block;
	mblk_t *frame = NULL;
	matroska_frame m_frame;

	frame = module_process(obj->modulesList[pin], buffer, &isKeyFrame, &isVisible, &codecPrivateData, &codecPrivateSize);

	m_frame.Timecode = ((int64_t)mblk_get_timestamp_info(frame)) * 1000000LL;
	m_frame.Size = (uint32_t)msgdsize(frame);
	m_frame.Data = frame->b_rptr;

	if(matroska_clusters_count(&obj->file) == 0) {
		matroska_start_cluster(&obj->file, mblk_get_timestamp_info(frame));
	} else {
		if((obj->inputDescsList[pin]->type == MSVideo && isKeyFrame) || (obj->duration - matroska_current_cluster_timecode(&obj->file) >= CLUSTER_MAX_DURATION)) {
			matroska_close_cluster(&obj->file);
			matroska_start_cluster(&obj->file, mblk_get_timestamp_info(frame));
		}
	}

	block = matroska_write_block(&obj->file, &m_frame, pin + 1, isKeyFrame, isVisible, codecPrivateData, codecPrivateSize);
	freemsg(frame);
	ms_free(codecPrivateData);
	return block;
}

static void changeClockRate(mblk_t *buffer, unsigned int oldClockRate, unsigned int newClockRate) {
	mblk_set_timestamp_info(buffer, (uint64_t)mblk_get_timestamp_info(buffer) * (uint64_t)newClockRate / (uint64_t)oldClockRate);
}

static int recorder_open_file(MSFilter *f, void *arg) {
	MKVRecorder *obj = (MKVRecorder *)f->data;
	const char *filename = (const char *)arg;

	ms_filter_lock(f);
	if(obj->state != MSRecorderClosed) {
		ms_error("MKVRecorder: %s is alread open", filename);
		goto fail;
	}
	if(access(filename, R_OK | W_OK) == 0) {
		obj->openMode = MKV_OPEN_APPEND;
	} else {
		obj->openMode = MKV_OPEN_CREATE;
	}

	ms_message("MKVRecorder: opening file %s in %s mode", filename, obj->openMode == MKV_OPEN_APPEND ? "append" : "create");
	if(matroska_open_file(&obj->file, filename, obj->openMode) != 0) {
		ms_error("MKVRecorder: fail to open %s", filename);
		goto fail;
	}


	obj->state = MSRecorderPaused;
	ms_filter_unlock(f);
	return 0;

fail:
	ms_filter_unlock(f);
	return -1;
}

static void recorder_request_fir(MSFilter *f, MKVRecorder *obj) {
	if(obj->lastFirTime == (uint64_t)-1 || obj->lastFirTime + 2000 < f->ticker->time) {
		obj->lastFirTime = f->ticker->time;
		ms_filter_notify_no_arg(f, MS_RECORDER_NEEDS_FIR);
	}
}

static int recorder_start(MSFilter *f, void *arg) {
	MKVRecorder *obj = (MKVRecorder *)f->data;
	int i;

	ms_filter_lock(f);
	if(obj->state == MSRecorderClosed) {
		ms_error("MKVRecorder: fail to start recording. The file has not been opened");
		goto fail;
	}
	if(!obj->tracksInitialized) {
		if(obj->openMode == MKV_OPEN_CREATE) {
			for(i = 0; i < f->desc->ninputs; i++) {
				if(obj->inputDescsList[i] != NULL) {
					obj->modulesList[i] = module_new(f->factory, obj->inputDescsList[i]->encoding);
					if(obj->modulesList[i] == NULL) {
						ms_error("Could not start the MKV recorder: %s is not supported", obj->inputDescsList[i]->encoding);
						goto fail;
					}
					module_set(obj->modulesList[i], obj->inputDescsList[i]);
					matroska_add_track(&obj->file, i + 1, module_get_codec_id(obj->modulesList[i]));
					obj->timeLoopCancelers[i] = time_loop_canceler_new();
				}
			}
			obj->duration = 0;
			matroska_set_doctype_version(&obj->file, MKV_DOCTYPE_VERSION, MKV_DOCTYPE_READ_VERSION);
			matroska_write_ebml_header(&obj->file);
			matroska_start_segment(&obj->file);
			matroska_write_zeros(&obj->file, 1024);
			matroska_mark_segment_info_position(&obj->file);
			matroska_write_zeros(&obj->file, 1024);
		} else {
			for(i = 0; i < f->desc->ninputs; i++) {
				if(obj->inputDescsList[i] != NULL) {
					const uint8_t *data = NULL;
					size_t length;
					obj->modulesList[i] = module_new(f->factory, obj->inputDescsList[i]->encoding);
					module_set(obj->modulesList[i], obj->inputDescsList[i]);
					if(matroska_get_codec_private(&obj->file, i + 1, &data, &length) == 0) {
						module_load_private_data(obj->modulesList[i], data, length);
					}
					obj->timeLoopCancelers[i] = time_loop_canceler_new();
				}
			}
			obj->duration = matroska_get_duration(&obj->file);
		}
		time_corrector_set_origin(&obj->timeCorrector, obj->duration);
		obj->tracksInitialized = TRUE;
	}
	obj->state = MSRecorderRunning;
	obj->needKeyFrame = TRUE;
	recorder_request_fir(f, obj);
	ms_message("MKVRecorder: recording successfully started");
	ms_filter_unlock(f);
	return 0;

fail:
	for(i = 0; i < f->desc->ninputs; i++) {
		if(obj->modulesList[i]) {
			module_free(obj->modulesList[i]);
			obj->modulesList[i] = NULL;
		}
	}
	ms_filter_unlock(f);
	return -1;
}

static int recorder_stop(MSFilter *f, void *arg) {
	MKVRecorder *obj = (MKVRecorder *)f->data;
	ms_filter_lock(f);
	switch(obj->state) {
		case MSRecorderClosed:
			ms_error("MKVRecorder: fail to stop recording. The file has not been opened");
			goto fail;
		case MSRecorderPaused:
			ms_warning("MKVRecorder: recording has already been stopped");
			break;
		case MSRecorderRunning:
			obj->state = MSRecorderPaused;
			muxer_empty_internal_queues(&obj->muxer);
			ms_message("MKVRecorder: recording successfully stopped");
			break;
	}
	ms_filter_unlock(f);
	return 0;

fail:
	ms_filter_unlock(f);
	return -1;
}

static void recorder_process(MSFilter *f) {
	MKVRecorder *obj = f->data;
	uint16_t i;

	ms_filter_lock(f);
	if(obj->state != MSRecorderRunning) {
		for(i = 0; i < f->desc->ninputs; i++) {
			if(f->inputs[i] != NULL) {
				ms_queue_flush(f->inputs[i]);
			}
		}
	} else {
		uint16_t pin;
		mblk_t *buffer = NULL;
		for(i = 0; i < f->desc->ninputs; i++) {
			if(f->inputs[i] != NULL && obj->inputDescsList[i] == NULL) {
				ms_queue_flush(f->inputs[i]);
			} else if(f->inputs[i] != NULL && obj->inputDescsList[i] != NULL) {
				MSQueue frames, frames_ms;
				ms_queue_init(&frames);
				ms_queue_init(&frames_ms);

				if((module_preprocess(obj->modulesList[i], f->inputs[i], &frames) < 0) && obj->needKeyFrame) {
					ms_warning("MKVRecorder: preprocess error, waiting for an I-frame");
					recorder_request_fir(f, obj);
				}
				while((buffer = ms_queue_get(&frames)) != NULL) {
					mblk_set_timestamp_info(buffer, time_loop_canceler_apply(obj->timeLoopCancelers[i], mblk_get_timestamp_info(buffer)));
					changeClockRate(buffer, obj->inputDescsList[i]->rate, 1000);
					ms_queue_put(&frames_ms, buffer);
				}

				if(obj->inputDescsList[i]->type == MSVideo && obj->needKeyFrame) {
					while((buffer = ms_queue_get(&frames_ms)) != NULL) {
						if(module_is_key_frame(obj->modulesList[i], buffer)) {
							ms_message("MKVRecorder: first I-frame received");
							break;
						} else {
							ms_warning("MKVRecorder: waiting for an I-frame");
							recorder_request_fir(f, obj);
							freemsg(buffer);
						}
					}
					if(buffer != NULL) {
						do {
							time_corrector_proceed(&obj->timeCorrector, buffer, i);
							muxer_put_buffer(&obj->muxer, buffer, i);
						} while((buffer = ms_queue_get(&frames_ms)) != NULL);
						obj->needKeyFrame = FALSE;
					}
				} else {
					while((buffer = ms_queue_get(&frames_ms)) != NULL) {
						time_corrector_proceed(&obj->timeCorrector, buffer, i);
						muxer_put_buffer(&obj->muxer, buffer, i);
					}
				}
			}
		}
		while((buffer = muxer_get_buffer(&obj->muxer, &pin)) != NULL) {
			matroska_block *block;
			timecode_t bufferTimecode;

			bufferTimecode = mblk_get_timestamp_info(buffer);

			block = write_frame(obj, buffer, pin);

			if(obj->inputDescsList[pin]->type == MSVideo) {
				matroska_add_cue(&obj->file, block);
			}
			if(bufferTimecode > obj->duration) {
				obj->duration = bufferTimecode;
			}
		}
	}
	ms_filter_unlock(f);
}

static int recorder_close(MSFilter *f, void *arg) {
	MKVRecorder *obj = (MKVRecorder *)f->data;
	int i;

	ms_filter_lock(f);
	ms_message("MKVRecorder: closing file");
	if(obj->state == MSRecorderClosed) {
		ms_warning("MKVRecorder: no file has been opened");
		goto end;
	}

	for(i = 0; i < f->desc->ninputs; i++) {
		if(obj->inputDescsList[i] != NULL) {
			if(matroska_track_check_block_presence(&obj->file, i + 1)) {
				uint8_t *codecPrivateData = NULL;
				size_t codecPrivateDataSize = 0;
				module_get_private_data(obj->modulesList[i], &codecPrivateData, &codecPrivateDataSize);
				matroska_track_set_info(&obj->file, i + 1, obj->inputDescsList[i]);
				if(codecPrivateDataSize > 0) {
					matroska_track_set_codec_private(&obj->file, i + 1, codecPrivateData, codecPrivateDataSize);
				}
				if(codecPrivateData) ms_free(codecPrivateData);
			} else {
				matroska_del_track(&obj->file, i + 1);
			}
		}
	}

	matroska_close_cluster(&obj->file);
	if(matroska_write_cues(&obj->file) != 0) {
		ms_warning("MKVRecorder: no cues written");
	}

	if(obj->openMode == MKV_OPEN_CREATE) {
		matroska_go_to_segment_info_mark(&obj->file);
	} else {
		matroska_go_to_segment_info_begin(&obj->file);
	}
	obj->duration++;
	matroska_set_segment_info(&obj->file, "libmediastreamer2", "libmediastreamer2", (double)obj->duration);
	matroska_write_segment_info(&obj->file);
	matroska_write_tracks(&obj->file);
	matroska_go_to_segment_begin(&obj->file);
	matroska_write_metaSeek(&obj->file);
	matroska_go_to_file_end(&obj->file);
	matroska_close_segment(&obj->file);

	matroska_close_file(&obj->file);
	for(i = 0; i < f->desc->ninputs; i++) {
		if(f->inputs[i] != NULL) ms_queue_flush(f->inputs[i]);
		if(obj->modulesList[i]) {
			module_free(obj->modulesList[i]);
			obj->modulesList[i] = NULL;
		}
		ms_free(obj->timeLoopCancelers[i]);
		obj->timeLoopCancelers[i] = NULL;
	}
	time_corrector_reset(&obj->timeCorrector);
	obj->tracksInitialized = FALSE;
	obj->state = MSRecorderClosed;
	ms_message("MKVRecorder: the file has been successfully closed");

end:
	ms_filter_unlock(f);
	return 0;
}

static int recorder_set_input_fmt(MSFilter *f, void *arg) {
	MKVRecorder *data = (MKVRecorder *)f->data;
	const MSPinFormat *pinFmt = (const MSPinFormat *)arg;

	ms_filter_lock(f);
	if(pinFmt->pin >= f->desc->ninputs) {
		ms_error("MKVRecorder: could not set pin #%d. Invalid pin number", pinFmt->pin);
		goto fail;
	}

	if(data->state != MSRecorderClosed) {
		if(data->tracksInitialized) {
			if(pinFmt->fmt == NULL) {
				ms_error("MKVRecorder: could not disable pin #%d. The file is opened", pinFmt->pin);
				goto fail;
			}
			if(data->inputDescsList[pinFmt->pin] == NULL) {
				ms_error("MKVRecorder: could not set pin #%d video size. That pin is not enabled", pinFmt->pin);
				goto fail;
			}
			if(pinFmt->fmt->type != MSVideo ||
			        strcmp(pinFmt->fmt->encoding, data->inputDescsList[pinFmt->pin]->encoding) != 0 ||
			        pinFmt->fmt->rate != data->inputDescsList[pinFmt->pin]->rate) {
				ms_error("MKVRecorder: could not set pin #%d video size. The specified format is not compatible with the current format. current={%s}, new={%s}",
				         pinFmt->pin,
				         ms_fmt_descriptor_to_string(data->inputDescsList[pinFmt->pin]),
				         ms_fmt_descriptor_to_string(pinFmt->fmt));
				goto fail;
			}
		}

		data->inputDescsList[pinFmt->pin] = pinFmt->fmt;
		ms_message("MKVRecorder: pin #%d video size set on %dx%d", pinFmt->pin, pinFmt->fmt->vsize.width, pinFmt->fmt->vsize.height);
	} else {
		if(pinFmt->fmt && find_module_desc_from_rfc_name(pinFmt->fmt->encoding) == NULL) {
			ms_error("MKVRecorder: could not set pin #%d. %s is not supported", pinFmt->pin, pinFmt->fmt->encoding);
			goto fail;
		}
		if(pinFmt->fmt == NULL) {
			data->inputDescsList[pinFmt->pin] = pinFmt->fmt;
			ms_message("MKVRecorder: pin #%d set as disabled", pinFmt->pin);
		} else {
			data->inputDescsList[pinFmt->pin] = pinFmt->fmt;
			ms_message("MKVRecorder: set pin #%d format. %s", pinFmt->pin, ms_fmt_descriptor_to_string(pinFmt->fmt));
		}
	}
	ms_filter_unlock(f);
	return 0;

fail:
	ms_filter_unlock(f);
	return -1;
}

static int recorder_get_state(MSFilter *f, void *data) {
	MKVRecorder *priv = (MKVRecorder *)f->data;
	ms_filter_lock(f);
	*(MSRecorderState *)data = priv->state;
	ms_filter_unlock(f);
	return 0;
}

static MSFilterMethod recorder_methods[] = {
	{	MS_RECORDER_OPEN            ,	recorder_open_file         },
	{	MS_RECORDER_CLOSE           ,	recorder_close             },
	{	MS_RECORDER_START           ,	recorder_start             },
	{	MS_RECORDER_PAUSE           ,	recorder_stop              },
	{	MS_FILTER_SET_INPUT_FMT     ,   recorder_set_input_fmt     },
	{	MS_RECORDER_GET_STATE	    ,	recorder_get_state         },
	{	0                           ,   NULL                       }
};

#ifdef _MSC_VER
MSFilterDesc ms_mkv_recorder_desc = {
	MS_MKV_RECORDER_ID,
	"MSMKVRecorder",
	"MKV file recorder",
	MS_FILTER_OTHER,
	NULL,
	2,
	0,
	recorder_init,
	recorder_preprocess,
	recorder_process,
	recorder_postprocess,
	recorder_uninit,
	recorder_methods,
	0
};
#else
MSFilterDesc ms_mkv_recorder_desc = {
	.id = MS_MKV_RECORDER_ID,
	.name = "MSMKVRecorder",
	.text = "MKV file recorder",
	.category = MS_FILTER_OTHER,
	.enc_fmt = NULL,
	.ninputs = 2,
	.noutputs = 0,
	.init = recorder_init,
	.preprocess = recorder_preprocess,
	.process = recorder_process,
	.postprocess = recorder_postprocess,
	.uninit = recorder_uninit,
	.methods = recorder_methods,
	.flags = 0
};
#endif

MS_FILTER_DESC_EXPORT(ms_mkv_recorder_desc)


/*********************************************************************************************
 * MKV Player Filter                                                                         *
 *********************************************************************************************/
typedef struct {
	bctbx_list_t *queue;
	int64_t min_timestamp;
	int64_t dts; // Decoding timestamp
} MKVBlockQueue;

static MKVBlockQueue *mkv_block_queue_new(void) {
	MKVBlockQueue *obj = (MKVBlockQueue *)ms_new0(MKVBlockQueue, 1);
	obj->min_timestamp = -1;
	return obj;
}

static void mkv_block_queue_flush(MKVBlockQueue *obj) {
	obj->queue = bctbx_list_free_with_data(obj->queue, (void( *)(void *))mkv_block_free);
	obj->min_timestamp = -1;
}

static void mkv_block_queue_free(MKVBlockQueue *obj) {
	mkv_block_queue_flush(obj);
	ms_free(obj);
}

static void mkv_block_queue_push(MKVBlockQueue *obj, MKVBlock *block) {
	obj->queue = bctbx_list_append(obj->queue, block);
	if(obj->min_timestamp < 0 || (int64_t)block->timestamp < obj->min_timestamp) {
		obj->min_timestamp = (int64_t)block->timestamp;
	}
}

static MKVBlock *mkv_block_queue_pull(MKVBlockQueue *obj) {
	if(obj->queue == NULL) {
		return NULL;
	} else {
		MKVBlock *block = (MKVBlock *)bctbx_list_nth_data(obj->queue, 0);
		obj->queue = bctbx_list_remove(obj->queue, block);
		if(obj->queue == NULL) {
			obj->min_timestamp = -1;
		}
		return block;
	}
}

static ms_bool_t mkv_block_queue_is_empty(const MKVBlockQueue *obj) {
	return (obj->queue == NULL);
}

typedef struct {
	MKVTrackReader *track_reader;
	MKVBlockQueue *queue;
	MKVBlock *waiting_block;
	uint32_t prev_timestamp;
	int64_t prev_min_ts;
} MKVBlockGroupMaker;

static MKVBlockGroupMaker *mkv_block_group_maker_new(MKVTrackReader *t_reader) {
	MKVBlockGroupMaker *obj = (MKVBlockGroupMaker *)ms_new0(MKVBlockGroupMaker, 1);
	obj->track_reader = t_reader;
	obj->queue = mkv_block_queue_new();
	obj->prev_timestamp = 0;
	obj->prev_min_ts = 0;
	return obj;
}

static void mkv_block_group_maker_reset(MKVBlockGroupMaker *obj) {
	mkv_block_queue_flush(obj->queue);
	if(obj->waiting_block) mkv_block_free(obj->waiting_block);
	obj->waiting_block = NULL;
	obj->prev_timestamp = 0;
	obj->prev_min_ts = 0;
}

static void mkv_block_group_maker_free(MKVBlockGroupMaker *obj) {
	mkv_block_queue_free(obj->queue);
	if(obj->waiting_block) mkv_block_free(obj->waiting_block);
	ms_free(obj);
}

static void mkv_block_group_maker_get_next_group(MKVBlockGroupMaker *obj, MKVBlockQueue *out_queue, ms_bool_t *eot) {
	MKVBlock *block = NULL;
	ms_bool_t _eot;
	do {
		mkv_track_reader_next_block(obj->track_reader, &block, &_eot);
		if(_eot) {
			*eot = TRUE;
			if(obj->waiting_block) mkv_block_queue_push(out_queue, obj->waiting_block);
			obj->waiting_block = NULL;
			return;
		}
		if(obj->waiting_block == NULL) {
			obj->waiting_block = block;
			obj->prev_timestamp = block->timestamp;
			block = NULL;
		} else {
			mkv_block_queue_push(out_queue, obj->waiting_block);
			obj->waiting_block = block;
		}
	} while(block == NULL || block->timestamp < obj->prev_timestamp);
	obj->prev_timestamp = block->timestamp;

	out_queue->dts = obj->prev_min_ts;
	obj->prev_min_ts = out_queue->min_timestamp;
	*eot = FALSE;
}

typedef struct {
	const MSFmtDescriptor *output_pin_desc;
	Module *module;
	const MKVTrack *track;
	ms_bool_t first_frame;
	MKVTrackReader *track_reader;
	MKVBlockQueue *block_queue;
	MKVBlockGroupMaker *group_maker;
	ms_bool_t eot; // end-of-track
} MKVTrackPlayer;

static MKVTrackPlayer *mkv_track_player_new(MSFactory *factory, MKVReader *reader, const MKVTrack *track) {
	MKVTrackPlayer *obj;
	const char *codec_name = codec_id_to_rfc_name(track->codec_id);

	if(codec_name == NULL) {
		ms_error("Cannot create MKVTrackPlayer. %s codec is not supported", track->codec_id);
		return NULL;
	}
	obj = ms_new0(MKVTrackPlayer, 1);
	obj->track = track;
	if(track->type == MKV_TRACK_TYPE_VIDEO) {
		MKVVideoTrack *v_track = (MKVVideoTrack *)track;
		MSVideoSize vsize;
		vsize.width = v_track->width;
		vsize.height = v_track->height;
		obj->output_pin_desc = ms_factory_get_video_format(factory, codec_name, vsize, (float)v_track->frame_rate, NULL);

	} else if(track->type == MKV_TRACK_TYPE_AUDIO) {
		MKVAudioTrack *a_track = (MKVAudioTrack *)track;
		obj->output_pin_desc = ms_factory_get_audio_format(factory, codec_name, a_track->sampling_freq, a_track->channels, NULL);
	} else {
		ms_error("MKVTrackPlayer: unsupported track type: %d", track->type);
		ms_free(obj);
		return NULL;
	}
	obj->module = module_new(factory, codec_name);
	if(obj->track->codec_private) {
		module_load_private_data(obj->module, track->codec_private, track->codec_private_length);
	}
	obj->first_frame = TRUE;
	obj->track_reader = mkv_reader_get_track_reader(reader, track->num);
	obj->block_queue = mkv_block_queue_new();
	obj->group_maker = mkv_block_group_maker_new(obj->track_reader);
	return obj;
}

static void mkv_track_player_free(MKVTrackPlayer *obj) {
	module_free(obj->module);
	mkv_block_queue_free(obj->block_queue);
	mkv_block_group_maker_free(obj->group_maker);
	ms_free(obj);
}

static void mkv_track_player_send_block(MSFactory *f, MKVTrackPlayer *obj, const MKVBlock *block, MSQueue *output) {
	mblk_t *tmp = allocb(block->data_length, 0);
	memcpy(tmp->b_wptr, block->data, block->data_length);
	tmp->b_wptr += block->data_length;
	mblk_set_timestamp_info(tmp, block->timestamp);
	changeClockRate(tmp, 1000, obj->output_pin_desc->rate);
	module_reverse(f, obj->module, tmp, output, obj->first_frame, block->codec_state_data, block->codec_state_size);
	if(obj->first_frame) {
		obj->first_frame = FALSE;
	}
}

static void mkv_track_player_reset(MKVTrackPlayer *obj) {
	mkv_block_queue_flush(obj->block_queue);
	mkv_block_group_maker_reset(obj->group_maker);
	obj->first_frame = TRUE;
	obj->eot = FALSE;
}

typedef struct {
	MKVReader *reader;
	MSPlayerState state;
	timecode_t time;
	MKVTrackPlayer *players[2];
	ms_bool_t position_changed;
	int loop_pause_interval, time_before_next_loop;
} MKVPlayer;

static ms_bool_t mkv_player_seek_ms(MKVPlayer *obj, int position) {
	int i;
	if(obj->state == MSPlayerClosed) {
		ms_error("MKVPlayer: cannot seek. No file open");
		return FALSE;
	}
	obj->time = position>0 ? mkv_reader_seek(obj->reader, position) : 0;
	for(i = 0; i < 2; i++) {
		if(obj->players[i]) {
			if(position == 0) mkv_track_reader_reset(obj->players[i]->track_reader);
			mkv_track_player_reset(obj->players[i]);
		}
	}
	obj->position_changed = TRUE;
	return TRUE;
}

static int player_close(MSFilter *f, void *arg);

static void player_init(MSFilter *f) {
	MKVPlayer *obj = (MKVPlayer *)ms_new0(MKVPlayer, 1);
	obj->state = MSPlayerClosed;
	obj->time = 0;
	obj->loop_pause_interval = -1;
	obj->time_before_next_loop = 0;
	f->data = obj;
}

static void player_uninit(MSFilter *f) {
	MKVPlayer *obj = (MKVPlayer *)f->data;
	int i;

	ms_filter_lock(f);
	if(obj->state != MSPlayerClosed) {
		mkv_reader_close(obj->reader);
	}
	for(i = 0; i < 2; i++) {
		if(obj->players[i]) {
			mkv_track_player_free(obj->players[i]);
		}
	}
	ms_free(obj);
	ms_filter_unlock(f);
}

static int player_open_file(MSFilter *f, void *arg) {
	MKVPlayer *obj = (MKVPlayer *)f->data;
	const char *filename = (const char *)arg;
	int typeList[2] = {MKV_TRACK_TYPE_VIDEO, MKV_TRACK_TYPE_AUDIO};
	int i;
	const MKVTrack *track = NULL;

	ms_filter_lock(f);
	if(obj->state != MSPlayerClosed) {
		ms_error("MKVPlayer: fail to open %s. A file is already opened", filename);
		goto fail;
	}
	ms_message("MKVPlayer: opening %s", filename);
	obj->reader = mkv_reader_open(filename);
	if(obj->reader == NULL) {
		ms_error("MKVPlayer: %s could not be opened in read-only mode", filename);
		goto fail;
	}
	for(i = 0; i < 2; i++) {
		const char *typeString[2] = {"video", "audio"};
		track = mkv_reader_get_default_track(obj->reader, typeList[i]);
		if(track == NULL) {
			ms_warning("MKVPlayer: no default %s track. Looking for first %s track", typeString[i], typeString[i]);
			track = mkv_reader_get_first_track(obj->reader, typeList[i]);
			if(track == NULL) {
				ms_warning("MKVPlayer: no %s track found", typeString[i]);
			}
		}
		if(track) {
			obj->players[i] = mkv_track_player_new(f->factory, obj->reader, track);
			if(obj->players[i] == NULL) {
				ms_warning("MKVPlayer: could not instanciate MKVTrackPlayer for track #%d", track->num);
			}
		}
	}
	if(obj->players[0] == NULL && obj->players[1] == NULL) {
		ms_error("MKVPlayer: no track found");
		goto fail;
	}
	obj->state = MSPlayerPaused;

	ms_filter_unlock(f);
	ms_filter_notify_no_arg(f, MS_FILTER_OUTPUT_FMT_CHANGED);
	return 0;

fail:
	ms_filter_unlock(f);
	return -1;
}

static void player_process(MSFilter *f) {
	int i;
	MKVPlayer *obj = (MKVPlayer *)f->data;
	ms_filter_lock(f);

	if(obj->state != MSPlayerPlaying) goto end;

	if(obj->time_before_next_loop > 0) {
		obj->time_before_next_loop -= f->ticker->interval;
		goto end;
	} else {
		obj->time_before_next_loop = -1;
	}

	obj->time += f->ticker->interval;

	if(obj->position_changed) {
		if(f->outputs[0] && strcasecmp(obj->players[0]->output_pin_desc->encoding, "h264") == 0) {
			// Send an empty buffer to notify the video decoder that it should reset itself
			ms_queue_put(f->outputs[0], allocb(0, 0));
		}
		obj->position_changed = FALSE;
	}

	for(i = 0; i < 2; i++) {
		MKVTrackPlayer *t_player = obj->players[i];
		if(t_player && f->outputs[i]) {
			if(mkv_block_queue_is_empty(t_player->block_queue)) {
				mkv_block_group_maker_get_next_group(t_player->group_maker, t_player->block_queue, &t_player->eot);
			}
			while(!t_player->eot && t_player->block_queue->dts <= obj->time) {
				MKVBlock *block;
				while((block = mkv_block_queue_pull(t_player->block_queue))) {
					mkv_track_player_send_block(f->factory, t_player, block, f->outputs[i]);
					mkv_block_free(block);
				}
				mkv_block_group_maker_get_next_group(t_player->group_maker, t_player->block_queue, &t_player->eot);
			}
		}
	}

	if((!obj->players[0] || !f->outputs[0] || obj->players[0]->eot)
	        && (!obj->players[1] || !f->outputs[1] || obj->players[1]->eot)) {

		ms_filter_notify_no_arg(f, MS_PLAYER_EOF);
		if(obj->loop_pause_interval < 0) {
			obj->state = MSPlayerPaused;
		} else {
			obj->time_before_next_loop = obj->loop_pause_interval;
			mkv_player_seek_ms(obj, 0);
		}
	}

end:
	ms_filter_unlock(f);
}

static int player_close(MSFilter *f, void *arg) {
	int i;
	MKVPlayer *obj = (MKVPlayer *)f->data;
	ms_filter_lock(f);
	if(obj->state != MSPlayerClosed) {
		mkv_reader_close(obj->reader);
		for(i = 0; i < f->desc->noutputs; i++) {
			if(obj->players[i]) mkv_track_player_free(obj->players[i]);
			obj->players[i] = NULL;
		}
		obj->time = 0;
		obj->state = MSPlayerClosed;
	}
	ms_filter_unlock(f);
	return 0;
}

static int player_seek_ms(MSFilter *f, void *arg) {
	MKVPlayer *obj = (MKVPlayer *)f->data;
	int target_position = *((int *)arg);
	int res;
	ms_filter_lock(f);
	res = mkv_player_seek_ms(obj, target_position) ? 0 : -1;
	ms_filter_unlock(f);
	return res;
}

static int player_start(MSFilter *f, void *arg) {
	MKVPlayer *obj = (MKVPlayer *)f->data;
	ms_filter_lock(f);
	if(obj->state == MSPlayerClosed) {
		goto fail;
	}
	obj->state = MSPlayerPlaying;
	ms_filter_unlock(f);
	return 0;

fail:
	ms_filter_unlock(f);
	return -1;
}

static int player_stop(MSFilter *f, void *arg) {
	MKVPlayer *obj = (MKVPlayer *)f->data;
	ms_filter_lock(f);
	if(obj->state == MSPlayerPlaying) {
		obj->state = MSPlayerPaused;
	}
	ms_filter_unlock(f);
	return 0;
}

static int player_get_output_fmt(MSFilter *f, void *arg) {
	MKVPlayer *obj = (MKVPlayer *)f->data;
	MSPinFormat *pinFmt = (MSPinFormat *)arg;
	ms_filter_lock(f);
	if(obj->state == MSPlayerClosed) {
		ms_error("MKVPlayer: cannot get pin format when player is closed");
		goto fail;
	}
	if(pinFmt->pin >= f->desc->noutputs) {
		ms_error("MKVPlayer: pin #%d does not exist", pinFmt->pin);
		goto fail;
	}
	if(obj->players[pinFmt->pin]) {
		pinFmt->fmt = obj->players[pinFmt->pin]->output_pin_desc;
	} else {
		pinFmt->fmt = NULL;
	}
	ms_filter_unlock(f);
	return 0;

fail:
	ms_filter_unlock(f);
	return -1;
}

static int player_get_state(MSFilter *f, void *arg) {
	MKVPlayer *obj = (MKVPlayer *)f->data;
	MSPlayerState *state = (MSPlayerState *)arg;
	ms_filter_lock(f);
	*state = obj->state;
	ms_filter_unlock(f);
	return 0;
}

static int player_set_loop(MSFilter *f, void *arg) {
	MKVPlayer *obj = (MKVPlayer *)f->data;
	ms_filter_lock(f);
	obj->loop_pause_interval = *(int *)arg;
	ms_filter_unlock(f);
	return 0;
}

static int player_get_duration(MSFilter *f, void *arg) {
	MKVPlayer *obj = (MKVPlayer *)f->data;
	ms_filter_lock(f);
	if(obj->state == MSPlayerClosed) {
		ms_error("MKVPlayer: cannot get duration. No file is open");
		goto fail;
	}
	*(int *)arg = (int)mkv_reader_get_segment_info(obj->reader)->duration;
	ms_filter_unlock(f);
	return 0;

fail:
	ms_filter_unlock(f);
	return -1;
}

static int player_get_current_position(MSFilter *f, void *arg) {
	MKVPlayer *obj = (MKVPlayer *)f->data;
	ms_filter_lock(f);
	if(obj->state == MSPlayerClosed) {
		ms_error("MKVPlayer: cannot get current duration. No file is open");
		goto fail;
	}
	*(int64_t *)arg = (int64_t)obj->time;
	ms_filter_unlock(f);
	return 0;

fail:
	ms_filter_unlock(f);
	return -1;
}

static MSFilterMethod player_methods[] = {
	{	MS_FILTER_GET_OUTPUT_FMT         ,	player_get_output_fmt        },
	{	MS_PLAYER_OPEN                   ,	player_open_file             },
	{	MS_PLAYER_CLOSE                  ,	player_close                 },
	{	MS_PLAYER_SEEK_MS                ,	player_seek_ms               },
	{	MS_PLAYER_START                  ,	player_start                 },
	{	MS_PLAYER_PAUSE                  ,	player_stop                  },
	{	MS_PLAYER_GET_STATE              ,	player_get_state             },
	{	MS_PLAYER_SET_LOOP               ,	player_set_loop              },
	{	MS_PLAYER_GET_DURATION           ,	player_get_duration          },
	{	MS_PLAYER_GET_CURRENT_POSITION   ,	player_get_current_position  },
	{	0                                ,	NULL                         }
};

#ifdef _MSC_VER
MSFilterDesc ms_mkv_player_desc = {
	MS_MKV_PLAYER_ID,
	"MSMKVPlayer",
	"MKV file player",
	MS_FILTER_OTHER,
	NULL,
	0,
	2,
	player_init,
	NULL,
	player_process,
	NULL,
	player_uninit,
	player_methods,
	0
};
#else
MSFilterDesc ms_mkv_player_desc = {
	.id = MS_MKV_PLAYER_ID,
	.name = "MSMKVPlayer",
	.text = "MKV file player",
	.category = MS_FILTER_OTHER,
	.enc_fmt = NULL,
	.ninputs = 0,
	.noutputs = 2,
	.init = player_init,
	.preprocess = NULL,
	.process = player_process,
	.postprocess = NULL,
	.uninit = player_uninit,
	.methods = player_methods,
	.flags = 0
};
#endif

MS_FILTER_DESC_EXPORT(ms_mkv_player_desc)
