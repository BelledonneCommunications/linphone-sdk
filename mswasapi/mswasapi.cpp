/*
mswasapi.cpp

mediastreamer2 library - modular sound and video processing and streaming
Windows Audio Session API sound card plugin for mediastreamer2
Copyright (C) 2010-2013 Belledonne Communications, Grenoble, France

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


#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mssndcard.h"

#include "mswasapi.h"
#include "mswasapi_reader.h"
#include "mswasapi_writer.h"


const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
const IID IID_ISimpleAudioVolume = __uuidof(ISimpleAudioVolume);
#if defined(MS2_WINDOWS_PHONE) || defined(MS2_WINDOWS_UNIVERSAL)
const IID IID_IAudioClient2 = __uuidof(IAudioClient2);
#else
const IID IID_IAudioClient = __uuidof(IAudioClient);
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
#endif

/******************************************************************************
 * Methods to (de)initialize and run the WASAPI sound capture filter          *
 *****************************************************************************/

static void ms_wasapi_read_init(MSFilter *f) {
	MSWASAPIReaderPtr r = MSWASAPIReaderNew(f);
	f->data = r;
}

static void ms_wasapi_read_preprocess(MSFilter *f) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	r->activate();
}

static void ms_wasapi_read_process(MSFilter *f) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	if (!r->isStarted()) {
		r->start();
	}
	r->feed(f);
}

static void ms_wasapi_read_postprocess(MSFilter *f) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	r->stop();
	r->deactivate();
}

static void ms_wasapi_read_uninit(MSFilter *f) {
	MSWASAPIReaderDelete(static_cast<MSWASAPIReaderPtr>(f->data));
}


/******************************************************************************
 * Methods to configure the WASAPI sound capture filter                       *
 *****************************************************************************/

static int ms_wasapi_read_set_sample_rate(MSFilter *f, void *arg) {
	/* This is not supported: the Audio Client requires to use the native sample rate. */
	MS_UNUSED(f), MS_UNUSED(arg);
	return -1;
}

static int ms_wasapi_read_get_sample_rate(MSFilter *f, void *arg) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	*((int *)arg) = r->getRate();
	return 0;
}

static int ms_wasapi_read_set_nchannels(MSFilter *f, void *arg) {
	/* This is not supported: the Audio Client requires to use 1 channel. */
	MS_UNUSED(f), MS_UNUSED(arg);
	return -1;
}

static int ms_wasapi_read_get_nchannels(MSFilter *f, void *arg) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	*((int *)arg) = r->getNChannels();
	return 0;
}

static int ms_wasapi_read_set_volume_gain(MSFilter *f, void *arg) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	r->setVolumeLevel(*(float *)arg);
	return 0;
}

static int ms_wasapi_read_get_volume_gain(MSFilter *f, void *arg) {
	MSWASAPIReaderType r = MSWASAPI_READER(f->data);
	float *volume = (float *)arg;
	*volume = r->getVolumeLevel();
	return *volume >= 0.0f ? 0 : -1;
}

static MSFilterMethod ms_wasapi_read_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE,	ms_wasapi_read_set_sample_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE,	ms_wasapi_read_get_sample_rate	},
	{	MS_FILTER_SET_NCHANNELS,	ms_wasapi_read_set_nchannels	},
	{	MS_FILTER_GET_NCHANNELS,	ms_wasapi_read_get_nchannels	},
	{	MS_AUDIO_CAPTURE_SET_VOLUME_GAIN, ms_wasapi_read_set_volume_gain	},
	{	MS_AUDIO_CAPTURE_GET_VOLUME_GAIN, ms_wasapi_read_get_volume_gain	},
	{	0,				NULL				}
};


/******************************************************************************
 * Definition of the WASAPI sound capture filter                              *
 *****************************************************************************/

#define MS_WASAPI_READ_ID			MS_FILTER_PLUGIN_ID
#define MS_WASAPI_READ_NAME			"MSWASAPIRead"
#define MS_WASAPI_READ_DESCRIPTION	"Windows Audio Session sound capture"
#define MS_WASAPI_READ_CATEGORY		MS_FILTER_OTHER
#define MS_WASAPI_READ_ENC_FMT		NULL
#define MS_WASAPI_READ_NINPUTS		0
#define MS_WASAPI_READ_NOUTPUTS		1
#define MS_WASAPI_READ_FLAGS		0

#ifndef _MSC_VER

MSFilterDesc ms_wasapi_read_desc = {
	.id = MS_WASAPI_READ_ID,
	.name = MS_WASAPI_READ_NAME,
	.text = MS_WASAPI_READ_DESCRIPTION,
	.category = MS_WASAPI_READ_CATEGORY,
	.enc_fmt = MS_WASAPI_READ_ENC_FMT,
	.ninputs = MS_WASAPI_READ_NINPUTS,
	.noutputs = MS_WASAPI_READ_NOUTPUTS,
	.init = ms_wasapi_read_init,
	.preprocess = ms_wasapi_read_preprocess,
	.process = ms_wasapi_read_process,
	.postprocess = ms_wasapi_read_postprocess,
	.uninit = ms_wasapi_read_uninit,
	.methods = ms_wasapi_read_methods,
	.flags = MS_WASAPI_READ_FLAGS
};

#else

MSFilterDesc ms_wasapi_read_desc = {
	MS_WASAPI_READ_ID,
	MS_WASAPI_READ_NAME,
	MS_WASAPI_READ_DESCRIPTION,
	MS_WASAPI_READ_CATEGORY,
	MS_WASAPI_READ_ENC_FMT,
	MS_WASAPI_READ_NINPUTS,
	MS_WASAPI_READ_NOUTPUTS,
	ms_wasapi_read_init,
	ms_wasapi_read_preprocess,
	ms_wasapi_read_process,
	ms_wasapi_read_postprocess,
	ms_wasapi_read_uninit,
	ms_wasapi_read_methods,
	MS_WASAPI_READ_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_wasapi_read_desc)




/******************************************************************************
 * Methods to (de)initialize and run the WASAPI sound output filter           *
 *****************************************************************************/

static void ms_wasapi_write_init(MSFilter *f) {
	MSWASAPIWriterPtr w = MSWASAPIWriterNew();
	f->data = w;
}

static void ms_wasapi_write_preprocess(MSFilter *f) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	w->activate();
}

static void ms_wasapi_write_process(MSFilter *f) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	if (!w->isStarted()) {
		w->start();
	}
	w->feed(f);
}

static void ms_wasapi_write_postprocess(MSFilter *f) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	w->stop();
	w->deactivate();
}

static void ms_wasapi_write_uninit(MSFilter *f) {
	MSWASAPIWriterDelete(static_cast<MSWASAPIWriterPtr>(f->data));
}


/******************************************************************************
 * Methods to configure the WASAPI sound output filter                        *
 *****************************************************************************/

static int ms_wasapi_write_set_sample_rate(MSFilter *f, void *arg) {
	/* This is not supported: the Audio Client requires to use the native sample rate. */
	MS_UNUSED(f), MS_UNUSED(arg);
	return -1;
}

static int ms_wasapi_write_get_sample_rate(MSFilter *f, void *arg) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	*((int *)arg) = w->getRate();
	return 0;
}

static int ms_wasapi_write_set_nchannels(MSFilter *f, void *arg) {
	/* This is not supported: the Audio Client requires to use 2 channels. */
	MS_UNUSED(f), MS_UNUSED(arg);
	return -1;
}

static int ms_wasapi_write_get_nchannels(MSFilter *f, void *arg) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	*((int *)arg) = w->getNChannels();
	return 0;
}

static int ms_wasapi_write_set_volume_gain(MSFilter *f, void *arg) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	w->setVolumeLevel(*(float *)arg);
	return 0;
}

static int ms_wasapi_write_get_volume_gain(MSFilter *f, void *arg) {
	MSWASAPIWriterType w = MSWASAPI_WRITER(f->data);
	float *volume = (float *)arg;
	*volume = w->getVolumeLevel();
	return *volume >= 0.0f ? 0 : -1;
}

static MSFilterMethod ms_wasapi_write_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE,	ms_wasapi_write_set_sample_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE,	ms_wasapi_write_get_sample_rate	},
	{	MS_FILTER_SET_NCHANNELS,	ms_wasapi_write_set_nchannels	},
	{	MS_FILTER_GET_NCHANNELS,	ms_wasapi_write_get_nchannels	},
	{	MS_AUDIO_PLAYBACK_SET_VOLUME_GAIN, ms_wasapi_write_set_volume_gain	},
	{	MS_AUDIO_PLAYBACK_GET_VOLUME_GAIN, ms_wasapi_write_get_volume_gain	},
	{	0,							NULL							}
};


/******************************************************************************
 * Definition of the WASAPI sound output filter                               *
 *****************************************************************************/

#define MS_WASAPI_WRITE_ID				MS_FILTER_PLUGIN_ID
#define MS_WASAPI_WRITE_NAME			"MSWASAPIWrite"
#define MS_WASAPI_WRITE_DESCRIPTION		"Windows Audio Session sound output"
#define MS_WASAPI_WRITE_CATEGORY		MS_FILTER_OTHER
#define MS_WASAPI_WRITE_ENC_FMT			NULL
#define MS_WASAPI_WRITE_NINPUTS			1
#define MS_WASAPI_WRITE_NOUTPUTS		0
#define MS_WASAPI_WRITE_FLAGS			0

#ifndef _MSC_VER

MSFilterDesc ms_wasapi_write_desc = {
	.id = MS_WASAPI_WRITE_ID,
	.name = MS_WASAPI_WRITE_NAME,
	.text = MS_WASAPI_WRITE_DESCRIPTION,
	.category = MS_WASAPI_WRITE_CATEGORY,
	.enc_fmt = MS_WASAPI_WRITE_ENC_FMT,
	.ninputs = MS_WASAPI_WRITE_NINPUTS,
	.noutputs = MS_WASAPI_WRITE_NOUTPUTS,
	.init = ms_wasapi_write_init,
	.preprocess = ms_wasapi_write_preprocess,
	.process = ms_wasapi_write_process,
	.postprocess = ms_wasapi_write_postprocess,
	.uninit = ms_wasapi_write_uninit,
	.methods = ms_wasapi_write_methods,
	.flags = MS_WASAPI_WRITE_FLAGS
};

#else

MSFilterDesc ms_wasapi_write_desc = {
	MS_WASAPI_WRITE_ID,
	MS_WASAPI_WRITE_NAME,
	MS_WASAPI_WRITE_DESCRIPTION,
	MS_WASAPI_WRITE_CATEGORY,
	MS_WASAPI_WRITE_ENC_FMT,
	MS_WASAPI_WRITE_NINPUTS,
	MS_WASAPI_WRITE_NOUTPUTS,
	ms_wasapi_write_init,
	ms_wasapi_write_preprocess,
	ms_wasapi_write_process,
	ms_wasapi_write_postprocess,
	ms_wasapi_write_uninit,
	ms_wasapi_write_methods,
	MS_WASAPI_WRITE_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_wasapi_write_desc)



static void ms_wasapi_snd_card_detect(MSSndCardManager *m);
static MSFilter *ms_wasapi_snd_card_create_reader(MSSndCard *card);
static MSFilter *ms_wasapi_snd_card_create_writer(MSSndCard *card);

static void ms_wasapi_snd_card_init(MSSndCard *card) {
	WasapiSndCard *c = static_cast<WasapiSndCard *>(ms_new0(WasapiSndCard, 1));
	card->data = c;
}

static void ms_wasapi_snd_card_uninit(MSSndCard *card) {
	WasapiSndCard *c = static_cast<WasapiSndCard *>(card->data);
	if (c->id_vector) delete c->id_vector;
	ms_free(c);
}

static MSSndCardDesc ms_wasapi_snd_card_desc = {
	"WASAPI",
	ms_wasapi_snd_card_detect,
	ms_wasapi_snd_card_init,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	ms_wasapi_snd_card_create_reader,
	ms_wasapi_snd_card_create_writer,
	ms_wasapi_snd_card_uninit,
	NULL,
	NULL
};

#if defined(MS2_WINDOWS_PHONE)
static MSSndCard *ms_wasapi_phone_snd_card_new(void) {
	MSSndCard *card = ms_snd_card_new(&ms_wasapi_snd_card_desc);
	card->name = ms_strdup("WASAPI sound card");
	card->latency = 250;
	return card;
}

#elif defined(MS2_WINDOWS_UNIVERSAL)

class MSWASAPIDeviceEnumerator
{
public:
	MSWASAPIDeviceEnumerator() : _l(NULL) {
		_DetectEvent = CreateEventEx(NULL, NULL, 0, EVENT_ALL_ACCESS);
		if (!_DetectEvent) {
			ms_error("mswasapi: Could not create detect event [%i]", GetLastError());
			return;
		}
	}
	~MSWASAPIDeviceEnumerator() {
		bctbx_list_free(_l);
		if (_DetectEvent != INVALID_HANDLE_VALUE) {
			CloseHandle(_DetectEvent);
			_DetectEvent = INVALID_HANDLE_VALUE;
		}
	}

	MSSndCard *NewCard(String^ DeviceId, const char *name, uint8_t capabilities) {
		const wchar_t *id = DeviceId->Data();
		MSSndCard *card = ms_snd_card_new(&ms_wasapi_snd_card_desc);
		WasapiSndCard *wasapicard = static_cast<WasapiSndCard *>(card->data);
		card->name = ms_strdup(name);
		card->capabilities = capabilities;
		wasapicard->id_vector = new std::vector<wchar_t>(wcslen(id) + 1);
		wcscpy_s(&wasapicard->id_vector->front(), wasapicard->id_vector->size(), id);
		wasapicard->id = &wasapicard->id_vector->front();
		return card;
	}

	void AddOrUpdateCard(String^ DeviceId, String^ DeviceName, DeviceClass dc) {
		char *name;
		const bctbx_list_t *elem = _l;
		size_t inputlen;
		size_t returnlen;
		uint8_t capabilities = 0;

		inputlen = wcslen(DeviceName->Data()) + 1;
		name = (char *)ms_malloc(inputlen);
		if (wcstombs_s(&returnlen, name, inputlen, DeviceName->Data(), inputlen) != 0) {
			ms_error("mswasapi: Cannot convert card name to multi-byte string.");
			return;
		}
		switch (dc) {
		case DeviceClass::AudioRender:
			capabilities = MS_SND_CARD_CAP_PLAYBACK;
			break;
		case DeviceClass::AudioCapture:
			capabilities = MS_SND_CARD_CAP_CAPTURE;
			break;
		}

		for (; elem != NULL; elem = elem->next) {
			MSSndCard *card = static_cast<MSSndCard *>(elem->data);
			WasapiSndCard *d = static_cast<WasapiSndCard *>(card->data);
			if (wcscmp(d->id, DeviceId->Data()) == 0) {
				/* Update an existing card. */
				if (card->name != NULL) {
					ms_free(card->name);
					card->name = ms_strdup(name);
				}
				card->capabilities |= capabilities;
				ms_free(name);
				return;
			}
		}

		/* Add a new card. */
		_l = bctbx_list_append(_l, NewCard(DeviceId, name, capabilities));
		ms_free(name);
	}

	void Detect(DeviceClass dc) {
		_dc = dc;
		String ^DefaultId;
		String ^DefaultName = "Default audio card";
		if (_dc == DeviceClass::AudioCapture) {
			DefaultId = MediaDevice::GetDefaultAudioCaptureId(AudioDeviceRole::Communications);
		} else {
			DefaultId = MediaDevice::GetDefaultAudioRenderId(AudioDeviceRole::Communications);
		}
		AddOrUpdateCard(DefaultId, DefaultName, _dc);
		Windows::Foundation::IAsyncOperation<DeviceInformationCollection^>^ op = DeviceInformation::FindAllAsync(_dc);
		op->Completed = ref new Windows::Foundation::AsyncOperationCompletedHandler<DeviceInformationCollection^>(
				[this](Windows::Foundation::IAsyncOperation<DeviceInformationCollection^>^ asyncOp, Windows::Foundation::AsyncStatus asyncStatus) {
			if (asyncStatus == Windows::Foundation::AsyncStatus::Completed) {
				DeviceInformationCollection^ deviceInfoCollection = asyncOp->GetResults();
				if ((deviceInfoCollection == nullptr) || (deviceInfoCollection->Size == 0)) {
					ms_error("mswasapi: No audio device found");
				}
				else {
					try {
						for (unsigned int i = 0; i < deviceInfoCollection->Size; i++) {
							DeviceInformation^ deviceInfo = deviceInfoCollection->GetAt(i);
							AddOrUpdateCard(deviceInfo->Id, deviceInfo->Name, _dc);
						}
					}
					catch (Platform::Exception^ e) {
						ms_error("mswaspi: Error of audio device detection");
					}
				}
			} else {
				ms_error("mswasapi: DeviceInformation::FindAllAsync failed");
			}
			SetEvent(_DetectEvent);
		});
		WaitForSingleObjectEx(_DetectEvent, INFINITE, FALSE);
	}

	void Detect() {
		Detect(DeviceClass::AudioCapture);
		Detect(DeviceClass::AudioRender);
	}

	bctbx_list_t *GetList() { return _l; }

private:
	bctbx_list_t *_l;
	DeviceClass _dc;
	HANDLE _DetectEvent;
};

#else

static MSSndCard *ms_wasapi_snd_card_new(LPWSTR id, const char *name, uint8_t capabilities) {
	MSSndCard *card = ms_snd_card_new(&ms_wasapi_snd_card_desc);
	WasapiSndCard *wasapicard = static_cast<WasapiSndCard *>(card->data);
	card->name = ms_strdup(name);
	card->capabilities = capabilities;
	card->latency = (capabilities & MS_SND_CARD_CAP_CAPTURE) ? 70 : 0;
	wasapicard->id_vector = new std::vector<wchar_t>(wcslen(id) + 1);
	wcscpy_s(&wasapicard->id_vector->front(), wasapicard->id_vector->size(), id);
	wasapicard->id = &wasapicard->id_vector->front();
	return card;
}

static void add_or_update_card(MSSndCardManager *m, bctbx_list_t **l, LPWSTR id, LPWSTR wname, EDataFlow data_flow) {
	MSSndCard *card;
	const bctbx_list_t *elem = *l;
	uint8_t capabilities = 0;
	char *name;
	size_t inputlen;
	size_t returnlen;
	int err;

	inputlen = wcslen(wname);
	returnlen = inputlen * 2;
	name = (char *)ms_malloc(returnlen);
	if ((err = WideCharToMultiByte(CP_ACP, 0, wname, -1, name, (int)returnlen, NULL, NULL)) == 0) {
		ms_error("mswasapi: Cannot convert card name to multi-byte string.");
		return;
	}
	switch (data_flow) {
	case eRender:
		capabilities = MS_SND_CARD_CAP_PLAYBACK;
		break;
	case eCapture:
		capabilities = MS_SND_CARD_CAP_CAPTURE;
		break;
	case eAll:
	default:
		capabilities = MS_SND_CARD_CAP_PLAYBACK | MS_SND_CARD_CAP_CAPTURE;
		break;
	}

	for (; elem != NULL; elem = elem->next){
		card = static_cast<MSSndCard *>(elem->data);
		if (strcmp(card->name, name) == 0){
			/* Update an existing card. */
			WasapiSndCard *d = static_cast<WasapiSndCard *>(card->data);
			card->capabilities |= capabilities;
			ms_free(name);
			return;
		}
	}

	/* Add a new card. */
	*l = bctbx_list_append(*l, ms_wasapi_snd_card_new(id, name, capabilities));
	ms_free(name);
}

static void add_endpoint(MSSndCardManager *m, EDataFlow data_flow, bctbx_list_t **l, IMMDevice *pEndpoint) {
	IPropertyStore *pProps = NULL;
	LPWSTR pwszID = NULL;
	HRESULT result = pEndpoint->GetId(&pwszID);
	REPORT_ERROR("mswasapi: Could not get ID of audio endpoint", result);
	result = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
	REPORT_ERROR("mswasapi: Could not open property store", result);
	PROPVARIANT varName;
	PropVariantInit(&varName);
	result = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
	REPORT_ERROR("mswasapi: Could not get friendly-name of audio endpoint", result);
	add_or_update_card(m, l, pwszID, varName.pwszVal, data_flow);
	CoTaskMemFree(pwszID);
	pwszID = NULL;
	PropVariantClear(&varName);
	SAFE_RELEASE(pProps);
error:
	CoTaskMemFree(pwszID);
	SAFE_RELEASE(pProps);
}

static void ms_wasapi_snd_card_detect_with_data_flow(MSSndCardManager *m, EDataFlow data_flow, bctbx_list_t **l) {
	IMMDeviceEnumerator *pEnumerator = NULL;
	IMMDeviceCollection *pCollection = NULL;
	IMMDevice *pEndpoint = NULL;
	HRESULT result = CoInitialize(NULL);
	result = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
	REPORT_ERROR("mswasapi: Could not create an instance of the device enumerator", result);
	result = pEnumerator->GetDefaultAudioEndpoint(data_flow, eCommunications, &pEndpoint);
	if (result == S_OK) {
		add_endpoint(m, data_flow, l, pEndpoint);
		SAFE_RELEASE(pEndpoint);
	}
	result = pEnumerator->EnumAudioEndpoints(data_flow, DEVICE_STATE_ACTIVE, &pCollection);
	REPORT_ERROR("mswasapi: Could not enumerate audio endpoints", result);
	UINT count;
	result = pCollection->GetCount(&count);
	REPORT_ERROR("mswasapi: Could not get the number of audio endpoints", result);
	if (count == 0) {
		ms_warning("mswasapi: No audio endpoint found");
		return;
	}
	for (ULONG i = 0; i < count; i++) {
		result = pCollection->Item(i, &pEndpoint);
		REPORT_ERROR("mswasapi: Could not get pointer to audio endpoint", result);
		add_endpoint(m, data_flow, l, pEndpoint);
		SAFE_RELEASE(pEndpoint);
	}
error:
	SAFE_RELEASE(pEnumerator);
	SAFE_RELEASE(pCollection);
	SAFE_RELEASE(pEndpoint);
}
#endif

static void ms_wasapi_snd_card_detect(MSSndCardManager *m) {
#if defined(MS2_WINDOWS_PHONE)
	MSSndCard *card = ms_wasapi_phone_snd_card_new();
	ms_snd_card_set_manager(m, card);
	ms_snd_card_manager_add_card(m, card);
#elif defined(MS2_WINDOWS_UNIVERSAL)
	MSWASAPIDeviceEnumerator *enumerator = new MSWASAPIDeviceEnumerator();
	enumerator->Detect();
	ms_snd_card_manager_prepend_cards(m, enumerator->GetList());
	delete enumerator;
#else
	bctbx_list_t *l = NULL;
	ms_wasapi_snd_card_detect_with_data_flow(m, eCapture, &l);
	ms_wasapi_snd_card_detect_with_data_flow(m, eRender, &l);
	ms_snd_card_manager_prepend_cards(m, l);
	bctbx_list_free(l);
#endif
}

static MSFilter *ms_wasapi_snd_card_create_reader(MSSndCard *card) {
	MSFilter *f = ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card), &ms_wasapi_read_desc);
	WasapiSndCard *wasapicard = static_cast<WasapiSndCard *>(card->data);
	MSWASAPIReaderType reader = MSWASAPI_READER(f->data);
	reader->init(wasapicard->id);
	return f;
}

static MSFilter *ms_wasapi_snd_card_create_writer(MSSndCard *card) {
	MSFilter *f = ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card), &ms_wasapi_write_desc);
	WasapiSndCard *wasapicard = static_cast<WasapiSndCard *>(card->data);
	MSWASAPIWriterType writer = MSWASAPI_WRITER(f->data);
	writer->init(wasapicard->id, f);
	return f;
}




#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) extern "C" __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) extern "C" type
#endif

MS_PLUGIN_DECLARE(void) libmswasapi_init(MSFactory *factory) {
	MSSndCardManager *manager = ms_factory_get_snd_card_manager(factory);
	ms_snd_card_manager_register_desc(manager, &ms_wasapi_snd_card_desc);
	ms_message("libmswasapi plugin loaded");
}
