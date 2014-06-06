#define bool_t ms_bool_t
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/rfc3984.h"
#include <mediastreamer2/msticker.h>
#include "../audiofilters/waveheader.h"
#undef bool_t

#define bool_t matroska_bool_t
#include <matroska/matroska.h>
#include <matroska/matroska_sem.h>
#undef bool_t

#define bool_t ambigous use ms_bool_t or matroska_bool_t

/*********************************************************************************************
 * AudioMetaData and VideoMediaData                                                          *
 *********************************************************************************************/
typedef struct
{
    int sampleRate;
    int bitrate;
    int nbChannels;
} AudioMetaData;

static void audio_meta_data_init(AudioMetaData *obj)
{
    obj->sampleRate = -1;
    obj->bitrate = -1;
    obj->nbChannels = -1;
}

typedef struct
{
    int bitrate;
    float fps;
    MSVideoSize definition;
} VideoMetaData;

static void video_meta_data_init(VideoMetaData *obj)
{
    obj->fps = -1.0;
    obj->bitrate = -1;
    obj->definition.height = -1;
    obj->definition.width = -1;
}

/*********************************************************************************************
 * Module interface                                                                          *
 *********************************************************************************************/
typedef void (*ModuleInitFunc)(void **obj);
typedef void (*ModuleUninitFunc)(void *obj);
typedef void (*ModuleSetFunc)(void *obj, const void *data);
typedef void (*ModulePreProFunc)(void *obj, MSQueue *input, MSQueue *output);
typedef mblk_t *(*ModuleProFunc)(void *obj, mblk_t *buffer, ms_bool_t *isKeyFrame);
typedef void (*ModulePrivateDataFunc)(const void *obj, uint8_t **data, size_t *data_size);

typedef struct
{
    const char *codecId;
    ModuleInitFunc init;
    ModuleUninitFunc uninit;
    ModuleSetFunc set;
    ModulePreProFunc preprocess;
    ModuleProFunc process;
    ModulePrivateDataFunc get_private_data;
} ModuleDesc;

/*********************************************************************************************
 * h264 module                                                                               *
 *********************************************************************************************/
/* H264Private */
typedef struct
{
    uint8_t profile;
    uint8_t level;
    uint8_t NALULenghtSizeMinusOne;
    MSList *sps_list;
    MSList *pps_list;
} H264Private;

static void H264Private_init(H264Private *obj)
{
    obj->NALULenghtSizeMinusOne = 0xFF;
    obj->pps_list = NULL;
    obj->sps_list = NULL;
}

static inline ms_bool_t mblk_is_equal(mblk_t *b1, mblk_t *b2)
{
    return msgdsize(b1) == msgdsize(b2) && memcmp(b1->b_rptr, b2->b_rptr, msgdsize(b1)) == 0;
}

static void H264Private_addSPS(H264Private *obj, mblk_t *sps)
{
    MSList *element;
    for(element = obj->sps_list; element != NULL && mblk_is_equal((mblk_t *)element->data, sps); element = element->next);
    if(element != NULL || obj->sps_list == NULL)
    {
        obj->profile = sps->b_rptr[1];
        obj->level = sps->b_rptr[3];
        obj->sps_list = ms_list_append(obj->sps_list, sps);
    }
}

static void H264Private_addPPS(H264Private *obj, mblk_t *pps)
{
    MSList *element;
    for(element = obj->pps_list; element != NULL && mblk_is_equal((mblk_t *)element->data, pps); element = element->next);
    if(element != NULL || obj->pps_list == NULL)
    {
        obj->pps_list = ms_list_append(obj->pps_list, pps);
    }
}

static void H264Private_serialize(const H264Private *obj, uint8_t **data, size_t *size)
{
    *size = 7;

    uint8_t nbSPS = ms_list_size(obj->sps_list);
    uint8_t nbPPS = ms_list_size(obj->pps_list);

    *size += (nbSPS + nbPPS) * 2;

    MSList *it;
    for(it=obj->sps_list;it!=NULL;it=it->next)
    {
        mblk_t *buff = (mblk_t*)(it->data);
        *size += msgdsize(buff);
    }
    for(it=obj->pps_list;it!=NULL;it=it->next)
    {
        mblk_t *buff = (mblk_t*)(it->data);
        *size += msgdsize(buff);
    }

    uint8_t *result = (uint8_t *)ms_new0(uint8_t, *size);
    result[0] = 0x01;
    result[1] = obj->profile;
    result[3] = obj->level;
    result[4] = obj->NALULenghtSizeMinusOne;
    result[5] = nbSPS & 0x1F;

    int i=6;
    for(it=obj->sps_list; it!=NULL; it=it->next)
    {
        mblk_t *buff = (mblk_t*)(it->data);
        size_t buff_size = msgdsize(buff);
        uint16_t buff_size_be = htons(buff_size);
        memcpy(&result[i], &buff_size_be, sizeof(buff_size_be));
        i+=sizeof(buff_size_be);
        memcpy(&result[i], buff->b_rptr, buff_size);
        i += buff_size;
    }

    result[i] = nbPPS;
    i++;

    for(it=obj->pps_list; it!=NULL; it=it->next)
    {
        mblk_t *buff = (mblk_t*)(it->data);
        int buff_size = msgdsize(buff);
        uint16_t buff_size_be = htons(buff_size);
        memcpy(&result[i], &buff_size_be, sizeof(buff_size_be));
        i+=sizeof(buff_size_be);
        memcpy(&result[i], buff->b_rptr, buff_size);
        i += buff_size;
    }
    *data = result;
}

static void H264Private_uninit(H264Private *obj)
{
    ms_list_free_with_data(obj->sps_list,(void (*)(void *))freemsg);
    ms_list_free_with_data(obj->pps_list,(void (*)(void *))freemsg);
}

/* h264 module */
typedef struct
{
    Rfc3984Context rfc3984Context;
    H264Private codecPrivate;
} H264Module;

static void h264_module_init(void **data)
{

    H264Module *mod = ms_new(H264Module, 1);
    rfc3984_init(&mod->rfc3984Context);
    H264Private_init(&mod->codecPrivate);
    *data = mod;
}

static void h264_module_uninit(void *data)
{
    H264Module *obj = (H264Module *)data;
    rfc3984_uninit(&obj->rfc3984Context);
    H264Private_uninit(&obj->codecPrivate);
    ms_free(obj);
}

static void h264_module_preprocessing(void *data, MSQueue *input, MSQueue *output)
{
    H264Module *obj = (H264Module *)data;
    MSQueue queue;
    mblk_t *inputBuffer;

    ms_queue_init(&queue);
    while((inputBuffer = ms_queue_get(input)) != NULL)
    {
        rfc3984_unpack(&obj->rfc3984Context, inputBuffer, &queue);
        if(!ms_queue_empty(&queue))
        {
            mblk_t *frame = ms_queue_get(&queue);
            mblk_t *end = frame;
            while(!ms_queue_empty(&queue))
            {
                end = concatb(end, ms_queue_get(&queue));
            }
            ms_queue_put(output, frame);
        }
    }
}

static void nalus_to_frame(mblk_t *buffer, mblk_t **frame, mblk_t **sps, mblk_t **pps, ms_bool_t *isKeyFrame)
{
    mblk_t *curNalu;
    *frame = NULL;
    *sps = NULL;
    *pps = NULL;
    *isKeyFrame = FALSE;

    for(curNalu = buffer; curNalu != NULL;)
    {
        mblk_t *buff = curNalu;
        curNalu = curNalu->b_cont;
        buff->b_cont = NULL;

        int type = (buff->b_rptr[0]) & ((1<<5)-1);

        switch(type)
        {
        case 7:
            if(*sps == NULL)
                *sps = buff;
            else
                concatb(*sps, buff);
            break;

        case 8:
            if(*pps == NULL)
                *pps = buff;
            else
                concatb(*pps, buff);
            break;

        default:
            if(type == 5)
            {
                *isKeyFrame = TRUE;
            }
            uint32_t bufferSize = htonl(msgdsize(buff));
            mblk_t *size = allocb(4, 0);
            memcpy(size->b_wptr, &bufferSize, sizeof(bufferSize));
            size->b_wptr = size->b_wptr + sizeof(bufferSize);
            mblk_set_timestamp_info(size, mblk_get_timestamp_info(buff));
            concatb(size, buff);
            buff = size;

            if(*frame == NULL)
                *frame = buff;
            else
                concatb(*frame, buff);
        }
    }

    if(*frame != NULL)
    {
        msgpullup(*frame, -1);
    }
    if(*sps != NULL)
    {
        msgpullup(*sps, -1);
    }
    if(*pps != NULL)
    {
        msgpullup(*pps, -1);
    }
}

static mblk_t *h264_module_processing(void *data, mblk_t *nalus, ms_bool_t *isKeyFrame)
{
    H264Module *obj = (H264Module *)data;
    mblk_t *frame, *sps=NULL, *pps=NULL;
    nalus_to_frame(nalus, &frame, &sps, &pps, isKeyFrame);
    if(sps != NULL)
    {
        H264Private_addSPS(&obj->codecPrivate, sps);
    }
    if(pps != NULL)
    {
        H264Private_addPPS(&obj->codecPrivate, pps);
    }
    return frame;
}

static void h264_module_get_private_data(const void *o, uint8_t **data, size_t *data_size)
{
    const H264Module *obj = (const H264Module *)o;
    H264Private_serialize(&obj->codecPrivate, data, data_size);
}

/* h264 module description */
const ModuleDesc h264_module_desc = {
    "V_MPEG4/ISO/AVC",
    h264_module_init,
    h264_module_uninit,
    NULL,
    h264_module_preprocessing,
    h264_module_processing,
    h264_module_get_private_data
};

/*********************************************************************************************
 * µLaw module                                                                               *
 *********************************************************************************************/
/* WavPrivate */
typedef struct
{
    uint16_t wFormatTag;
    uint16_t nbChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    uint16_t cbSize;
} WavPrivate;

static void wav_private_set(const AudioMetaData *obj, WavPrivate *data)
{
    uint16_t bitsPerSample = 8;
    uint16_t nbBlockAlign = (bitsPerSample * obj->nbChannels)/8;
    uint32_t bitrate = bitsPerSample * obj->nbChannels * obj->sampleRate;

    data->wFormatTag = le_uint16((uint16_t)7);
    data->nbChannels = le_uint16((uint16_t)obj->nbChannels);
    data->nSamplesPerSec = le_uint32((uint32_t)obj->sampleRate);
    data->nAvgBytesPerSec = le_uint32(bitrate);
    data->nBlockAlign = le_uint16((uint16_t)nbBlockAlign);
    data->wBitsPerSample = le_uint16(bitsPerSample);
    data->cbSize = 0;
}

static void wav_private_serialize(const WavPrivate *obj, uint8_t **data, size_t *size)
{
    *size = sizeof(obj->wFormatTag)
            + sizeof(obj->nbChannels)
            + sizeof(obj->nSamplesPerSec)
            + sizeof(obj->nAvgBytesPerSec)
            + sizeof(obj->nBlockAlign)
            + sizeof(obj->wBitsPerSample)
            + sizeof(obj->cbSize);
    *data = (uint8_t *)ms_new0(uint8_t, *size);
    uint8_t *it = *data;
    memcpy(it, &obj->wFormatTag, sizeof(obj->wFormatTag));
    it+=sizeof(obj->wFormatTag);
    memcpy(it, &obj->nbChannels, sizeof(obj->nbChannels));
    it+=sizeof(obj->nbChannels);
    memcpy(it, &obj->nSamplesPerSec, sizeof(obj->nSamplesPerSec));
    it+=sizeof(obj->nSamplesPerSec);
    memcpy(it, &obj->nAvgBytesPerSec, sizeof(obj->nAvgBytesPerSec));
    it+=sizeof(obj->nAvgBytesPerSec);
    memcpy(it, &obj->nBlockAlign, sizeof(obj->nBlockAlign));
    it+=sizeof(obj->nBlockAlign);
    memcpy(it, &obj->wBitsPerSample, sizeof(obj->wBitsPerSample));
    it+=sizeof(obj->wBitsPerSample);
    memcpy(it, &obj->cbSize, sizeof(obj->cbSize));
}

/* µLaw module */
typedef struct
{
    WavPrivate codecPrivate;
} MuLawModule;

static void mu_law_module_init(void **o)
{
    *o = ms_new(MuLawModule, 1);
}

static void mu_law_module_uninit(void *o)
{
    ms_free(o);
}

static void mu_law_module_set(void *o, const void *data)
{
    MuLawModule *obj = (MuLawModule *)o;
    const AudioMetaData *aMeta = (const AudioMetaData *)data;
    wav_private_set(aMeta, &obj->codecPrivate);
}

static void mu_law_module_get_private_data(const void *o, uint8_t **data, size_t *data_size)
{
    const MuLawModule *obj = (const MuLawModule *)o;
    wav_private_serialize(&obj->codecPrivate, data, data_size);
}

/* µLaw module description */
const ModuleDesc mu_law_module_desc = {
    "A_MS/ACM",
    mu_law_module_init,
    mu_law_module_uninit,
    mu_law_module_set,
    NULL,
    NULL,
    mu_law_module_get_private_data
};

/*********************************************************************************************
 * Available modules                                                                         *
 *********************************************************************************************/
typedef enum {
    H264_MOD_ID,
    MU_LAW_MOD_ID
} ModuleId;

const ModuleDesc const *modules[] = {
    &h264_module_desc,
    &mu_law_module_desc,
    NULL
};

/*********************************************************************************************
 * Matroska                                                                                  *
 *********************************************************************************************/
#define WRITE_DEFAULT_ELEMENT TRUE

static const timecode_t MKV_TIMECODE_SCALE = 1000000;
static const int MKV_DOCTYPE_VERSION = 2;
static const int MKV_DOCTYPE_READ_VERSION = 2;
static const int MKV_CLUSTER_DURATION = 5000;

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

static void loadModules(nodemodule *modules)
{
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

typedef struct
{
    parsercontext *p;
    stream *output;
    ebml_element *header;
    ebml_master *segment, *cluster, *info, *tracks, *metaSeek, *cues, *firstCluster, *videoTrack, *audioTrack;
    matroska_seekpoint *infoMeta, *tracksMeta, *cuesMeta;
    timecode_t timecodeScale;
    MSList *voids;
} Matroska;

static void matroska_init(Matroska *obj)
{
    obj->p = (parsercontext *)ms_new(parsercontext, 1);
    ParserContext_Init(obj->p, NULL, NULL, NULL);
    loadModules((nodemodule*)obj->p);
    MATROSKA_Init((nodecontext*)obj->p);

    obj->output = NULL;
    obj->header = EBML_ElementCreate(obj->p, &EBML_ContextHead, TRUE, NULL);
    obj->segment = (ebml_master *)EBML_ElementCreate(obj->p, &MATROSKA_ContextSegment, TRUE, NULL);
    obj->metaSeek = (ebml_master *)EBML_MasterAddElt(obj->segment, &MATROSKA_ContextSeekHead, FALSE);
    obj->infoMeta = (matroska_seekpoint *)EBML_MasterAddElt(obj->metaSeek, &MATROSKA_ContextSeek, TRUE);
    obj->tracksMeta = (matroska_seekpoint *)EBML_MasterAddElt(obj->metaSeek, &MATROSKA_ContextSeek, TRUE);
    obj->cuesMeta = (matroska_seekpoint *)EBML_MasterAddElt(obj->metaSeek, &MATROSKA_ContextSeek, TRUE);
    obj->info = (ebml_master *)EBML_MasterAddElt(obj->segment, &MATROSKA_ContextInfo, TRUE);
    obj->tracks = (ebml_master *)EBML_MasterAddElt(obj->segment, &MATROSKA_ContextTracks, TRUE);
    obj->videoTrack = (ebml_master *)EBML_MasterAddElt(obj->tracks, &MATROSKA_ContextTrackEntry, FALSE);
    obj->audioTrack = (ebml_master *)EBML_MasterAddElt(obj->tracks, &MATROSKA_ContextTrackEntry, FALSE);
    obj->cues = (ebml_master *)EBML_MasterAddElt(obj->segment, &MATROSKA_ContextCues, FALSE);
    obj->timecodeScale = MKV_TIMECODE_SCALE;
    obj->firstCluster = NULL;

    int i;
    obj->voids = NULL;
    for(i=0; i<3; i++)
    {
        ebml_element *elt = EBML_MasterAddElt(obj->segment, &EBML_ContextEbmlVoid, FALSE);
        EBML_VoidSetFullSize(elt, 256);
        obj->voids = ms_list_append(obj->voids, elt);
    }

    MATROSKA_LinkMetaSeekElement(obj->infoMeta, (ebml_element *)obj->info);
    MATROSKA_LinkMetaSeekElement(obj->tracksMeta, (ebml_element *)obj->tracks);
    MATROSKA_LinkMetaSeekElement(obj->cuesMeta, (ebml_element *)obj->cues);
}

static void matroska_uninit(Matroska *obj)
{
    if(obj->output != NULL)
    {
        StreamClose(obj->output);
        obj->output = NULL;
    }

    NodeDelete((node*)obj->header);
    NodeDelete((node*)obj->segment);
    ms_list_free(obj->voids);

    MATROSKA_Done((nodecontext*)obj->p);
    ParserContext_Done(obj->p);
    ms_free(obj->p);
}

static ms_bool_t matroska_open_file(Matroska *obj, const char path[])
{
    obj->output = StreamOpen(obj->p, path, SFLAG_WRONLY | SFLAG_CREATE);
    return (obj->output != NULL);
}

static void matroska_close_file(Matroska *obj)
{
    StreamClose(obj->output);
    obj->output = NULL;
}

static void matroska_set_doctype_version(Matroska *obj, int doctypeVersion, int doctypeReadVersion)
{
    EBML_IntegerSetValue((ebml_integer*)EBML_MasterFindChild(obj->header, &EBML_ContextDocTypeVersion), doctypeVersion);
    EBML_IntegerSetValue((ebml_integer*)EBML_MasterFindChild(obj->header, &EBML_ContextDocTypeReadVersion), doctypeReadVersion);
}

static inline void matroska_write_ebml_header(Matroska *obj)
{
    EBML_ElementRender(obj->header, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
}

static void matroska_set_segment_info(Matroska *obj, const char writingApp[], const char muxingApp[], double duration)
{
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(obj->info, &MATROSKA_ContextTimecodeScale), obj->timecodeScale);
    EBML_StringSetValue((ebml_string*)EBML_MasterGetChild(obj->info, &MATROSKA_ContextMuxingApp), muxingApp);
    EBML_StringSetValue((ebml_string*)EBML_MasterGetChild(obj->info, &MATROSKA_ContextWritingApp), writingApp);
    EBML_FloatSetValue((ebml_float *)EBML_MasterGetChild(obj->info, &MATROSKA_ContextDuration), duration);
}

static void updateElementHeader(ebml_element *element, stream *file)
{
    filepos_t initial_pos = Stream_Seek(file, 0, SEEK_CUR);
    Stream_Seek(file, EBML_ElementPosition(element), SEEK_SET);
    EBML_ElementUpdateSize(element, WRITE_DEFAULT_ELEMENT, FALSE);
    EBML_ElementRenderHead(element, file, FALSE, NULL);
    Stream_Seek(file, initial_pos, SEEK_SET);
}

static int replace_void(ebml_element *voidElt, ebml_element *newElt, stream *file)
{
    EBML_ElementUpdateSize(newElt, WRITE_DEFAULT_ELEMENT, FALSE);
    filepos_t newVoidSize = EBML_ElementFullSize(voidElt, WRITE_DEFAULT_ELEMENT) - EBML_ElementFullSize(newElt, WRITE_DEFAULT_ELEMENT);

    if(newVoidSize <= 1)
    {
        return -1;
    }

    filepos_t initial_pos = Stream_Seek(file, 0, SEEK_CUR);
    Stream_Seek(file, EBML_ElementPosition(voidElt), SEEK_SET);
    EBML_ElementRender(newElt, file, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
    EBML_VoidSetFullSize(voidElt, newVoidSize);
    EBML_ElementRenderHead(voidElt, file, FALSE, NULL);
    Stream_Seek(file, initial_pos, SEEK_SET);
    return 0;
}

static inline void matroska_start_segment(Matroska *obj)
{
    EBML_ElementSetSizeLength((ebml_element *)obj->segment, 8);
    EBML_ElementRenderHead((ebml_element *)obj->segment, obj->output, FALSE, NULL);
}

static void matroska_make_void(Matroska *obj)
{
    MSList *elt;
    for(elt = obj->voids; elt != NULL; elt = elt->next)
    {
        EBML_ElementRender((ebml_element *)elt->data, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
    }
}

static inline void matroska_close_segment(Matroska *obj)
{
    updateElementHeader((ebml_element *)obj->segment, obj->output);
}

static void matroska_start_cluster(Matroska *obj, timecode_t clusterTimecode)
{
    obj->cluster = (ebml_master *)EBML_MasterAddElt(obj->segment, &MATROSKA_ContextCluster, TRUE);
    EBML_ElementSetSizeLength((ebml_element *)obj->cluster, 8);
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(obj->cluster, &MATROSKA_ContextTimecode), clusterTimecode);
    EBML_ElementRender((ebml_element *)obj->cluster, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
    if(obj->firstCluster == NULL)
    {
        obj->firstCluster = obj->cluster;
    }
}

static inline void matroska_close_cluster(Matroska *obj)
{
    updateElementHeader((ebml_element *)obj->cluster, obj->output);
}

static void matroska_set_track(ebml_master *track, int trackNum, int trackType, const char codecID[], const uint8_t codecPrivateData[], size_t codecPrivateSize)
{
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextTrackNumber), trackNum);
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextTrackUID), trackNum);
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextTrackType), trackType);
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextFlagEnabled), 1);
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextFlagDefault), 1);
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextFlagForced), 0);
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextFlagLacing), 0);
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextMinCache), 1);
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextMaxBlockAdditionID), 0);
    EBML_StringSetValue((ebml_string *)EBML_MasterGetChild(track, &MATROSKA_ContextCodecID), codecID);
    if(codecPrivateData != NULL)
    {
        EBML_BinarySetData((ebml_binary *)EBML_MasterGetChild(track, &MATROSKA_ContextCodecPrivate), codecPrivateData, codecPrivateSize);
    }
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextCodecDecodeAll), 0);
}

static void matroska_set_video_track(Matroska *obj, const char codecId[], const uint8_t codecPrivateData[], size_t codecPrivateSize, const VideoMetaData *vMeta)
{
    matroska_set_track(obj->videoTrack, 1, TRACK_TYPE_VIDEO, codecId, codecPrivateData, codecPrivateSize);
    ebml_master *metaData = (ebml_master *)EBML_MasterGetChild(obj->videoTrack, &MATROSKA_ContextVideo);
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(metaData, &MATROSKA_ContextFlagInterlaced), 0);
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(metaData, &MATROSKA_ContextPixelWidth), vMeta->definition.width);
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(metaData, &MATROSKA_ContextPixelHeight), vMeta->definition.height);
}

static void matroska_set_audio_track(Matroska *obj, const char codecId[], const uint8_t codecPrivateData[], size_t codecPrivateSize, const AudioMetaData *aMeta)
{
    matroska_set_track(obj->audioTrack, 2, TRACK_TYPE_AUDIO, codecId, codecPrivateData, codecPrivateSize);
    ebml_master *metaData = (ebml_master *)EBML_MasterGetChild(obj->audioTrack, &MATROSKA_ContextAudio);
    EBML_FloatSetValue((ebml_float *)EBML_MasterGetChild(metaData, &MATROSKA_ContextSamplingFrequency), aMeta->sampleRate);
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(metaData, &MATROSKA_ContextChannels), aMeta->nbChannels);
}

static inline void matroska_write_segment_info(Matroska *obj)
{
    replace_void((ebml_element *)ms_list_nth_data(obj->voids, 1), (ebml_element *)obj->info, obj->output);
    MATROSKA_MetaSeekUpdate(obj->infoMeta);
}

static inline void matroska_write_tracks(Matroska *obj)
{
    replace_void((ebml_element *)ms_list_nth_data(obj->voids, 2), (ebml_element *)obj->tracks, obj->output);
    MATROSKA_MetaSeekUpdate(obj->tracksMeta);
}

static inline void matroska_write_cues(Matroska *obj)
{
    EBML_ElementRender((ebml_element *)obj->cues, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
    MATROSKA_MetaSeekUpdate(obj->cuesMeta);
}

static inline void matroska_write_metaSeek(Matroska *obj)
{
    replace_void((ebml_element *)ms_list_nth_data(obj->voids, 0), (ebml_element *)obj->metaSeek, obj->output);
}

static inline timecode_t matroska_get_current_cluster_timestamp(const Matroska *obj)
{
    return (timecode_t)EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild(obj->cluster, &MATROSKA_ContextTimecode));
}

static matroska_block *matroska_write_block(Matroska *obj, const matroska_frame *m_frame, int trackNum, ms_bool_t isKeyFrame)
{
    matroska_block *block = (matroska_block *)EBML_MasterAddElt(obj->cluster, &MATROSKA_ContextSimpleBlock, FALSE);
    block->TrackNumber = trackNum;
    MATROSKA_LinkBlockWithReadTracks(block, obj->tracks, TRUE);
    MATROSKA_LinkBlockWriteSegmentInfo(block, obj->info);
    MATROSKA_BlockSetKeyframe(block, isKeyFrame);
    MATROSKA_BlockSetDiscardable(block, FALSE);
    timecode_t clusterTimecode = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild(obj->cluster, &MATROSKA_ContextTimecode));
    MATROSKA_BlockAppendFrame(block, m_frame, clusterTimecode * obj->timecodeScale);
    EBML_ElementRender((ebml_element *)block, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
    MATROSKA_BlockReleaseData(block, TRUE);
    return block;
}

/*********************************************************************************************
 * MKV Writer Filter                                                                         *
 *********************************************************************************************/
typedef struct
{
    Matroska file;
    VideoMetaData vMeta;
    AudioMetaData aMeta;
    char *output_path;
    ModuleId videoModuleId, audioModuleId;
    void *videoModule, *audioModule;
    timecode_t duration;
    MSQueue internalAudio, internalVideo;
} MKVWriter;

static void writer_init(MSFilter *f)
{
    MKVWriter *obj = (MKVWriter *)ms_new(MKVWriter, 1);

    matroska_init(&obj->file);

    video_meta_data_init(&obj->vMeta);
    audio_meta_data_init(&obj->aMeta);

    obj->videoModuleId = H264_MOD_ID;
    obj->audioModuleId = MU_LAW_MOD_ID;
    modules[obj->videoModuleId]->init(&obj->videoModule);
    modules[obj->audioModuleId]->init(&obj->audioModule);

    obj->output_path = "test.mkv";
    obj->duration = 0;

    ms_queue_init(&obj->internalVideo);
    ms_queue_init(&obj->internalAudio);

    f->data=obj;
}

static void writer_preprocess(MSFilter *f)
{
    MKVWriter *obj = (MKVWriter *)f->data;

    matroska_open_file(&obj->file, obj->output_path);
    matroska_set_doctype_version(&obj->file, MKV_DOCTYPE_VERSION, MKV_DOCTYPE_READ_VERSION);
    matroska_write_ebml_header(&obj->file);
    matroska_start_segment(&obj->file);
    matroska_make_void(&obj->file);
    matroska_start_cluster(&obj->file, 0);

    matroska_set_video_track(&obj->file, modules[obj->videoModuleId]->codecId, NULL, 0, &obj->vMeta);
    if(f->inputs[1] != NULL)
    {
        matroska_set_audio_track(&obj->file, modules[obj->audioModuleId]->codecId, NULL, 0, &obj->aMeta);
        modules[obj->audioModuleId]->set(obj->audioModule, &obj->aMeta);
    }
}

static matroska_block *write_frame(MKVWriter *obj, mblk_t *buffer, int trackNum, ModuleId moduleId, void *module)
{
    ms_bool_t isKeyFrame;
    mblk_t *frame;

    if(modules[moduleId]->process != NULL)
        frame = modules[moduleId]->process(module, buffer, &isKeyFrame);
    else
    {
        frame = buffer;
        isKeyFrame = TRUE;
    }

    timecode_t bufferTimecode = ((int64_t)mblk_get_timestamp_info(frame))*1000000LL;

    matroska_frame m_frame;
    m_frame.Timecode = bufferTimecode;
    m_frame.Size = msgdsize(frame);
    m_frame.Data = frame->b_rptr;

    matroska_block *block = matroska_write_block(&obj->file, &m_frame, trackNum, isKeyFrame);
    freemsg(frame);
    return block;
}

static void changeClockRate(mblk_t *buffer, uint32_t oldClockRate, uint32_t newClockRate)
{
    mblk_t *curBuff;
    for(curBuff = buffer; curBuff != NULL; curBuff = curBuff->b_cont)
    {
        mblk_set_timestamp_info(curBuff, mblk_get_timestamp_info(curBuff) * newClockRate / oldClockRate);
    }
}

static void moveBuffers(MSQueue *input, MSQueue *output)
{
    mblk_t *buffer;
    while((buffer = ms_queue_get(input)) != NULL)
    {
        ms_queue_put(output, buffer);
    }
}

static void writer_process(MSFilter *f)
{
    MKVWriter *obj = f->data;
    mblk_t *inputBuffer;
    MSQueue frames, audioBuffers;

    timecode_t clusterTimecode = matroska_get_current_cluster_timestamp(&obj->file);
    if(obj->duration - clusterTimecode >= MKV_CLUSTER_DURATION)
    {
        matroska_close_cluster(&obj->file);
        matroska_start_cluster(&obj->file, obj->duration);
        clusterTimecode = obj->duration;
    }

    ms_queue_init(&frames);
    if(modules[obj->videoModuleId]->preprocess != NULL)
        modules[obj->videoModuleId]->preprocess(obj->videoModule, f->inputs[0], &frames);
    else
        moveBuffers(f->inputs[0], &frames);

    ms_queue_init(&audioBuffers);
    if(modules[obj->audioModuleId]->preprocess != NULL)
        modules[obj->audioModuleId]->preprocess(obj->audioModule, f->inputs[1], &audioBuffers);
    else
        moveBuffers(f->inputs[1], &audioBuffers);

    while((inputBuffer = ms_queue_get(&frames)) != NULL)
    {
        changeClockRate(inputBuffer, 90000, 1000);
        ms_queue_put(&obj->internalVideo, inputBuffer);
    }

    while((inputBuffer = ms_queue_get(&audioBuffers)) != NULL)
    {
        changeClockRate(inputBuffer, obj->aMeta.sampleRate, 1000);
        ms_queue_put(&obj->internalAudio, inputBuffer);
    }

    while(!ms_queue_empty(&obj->internalVideo) && !ms_queue_empty(&obj->internalAudio))
    {
        matroska_block *block;
        timecode_t videoTime = mblk_get_timestamp_info(qfirst(&obj->internalVideo.q));
        timecode_t audioTime = mblk_get_timestamp_info(qfirst(&obj->internalAudio.q));

        if(videoTime <= audioTime)
        {
            block = write_frame(obj, ms_queue_get(&obj->internalVideo), 1, obj->videoModuleId, obj->videoModule);

            matroska_cuepoint *cue = (matroska_cuepoint *)EBML_MasterAddElt(obj->file.cues, &MATROSKA_ContextCuePoint, 1);
            MATROSKA_LinkCuePointBlock(cue, block);
            MATROSKA_LinkCueSegmentInfo(cue, obj->file.info);
            MATROSKA_CuePointUpdate(cue, (ebml_element *)obj->file.segment);

            obj->duration = videoTime;
        }
        else
        {
            block = write_frame(obj, ms_queue_get(&obj->internalAudio), 2, obj->audioModuleId, obj->audioModule);
            obj->duration = audioTime;
        }
    }
}

static void writer_postprocess(MSFilter *f)
{
    MKVWriter *obj = (MKVWriter *)f->data;

    matroska_close_cluster(&obj->file);

    ms_queue_flush(f->inputs[0]);
    ms_queue_flush(f->inputs[1]);
    ms_queue_flush(&obj->internalVideo);
    ms_queue_flush(&obj->internalAudio);

    matroska_set_segment_info(&obj->file, "libmediastreamer2", "libmediastreamer2", obj->duration);
    matroska_write_segment_info(&obj->file);

    uint8_t *codecPrivateData;
    size_t codecPrivateDataSize;
    modules[obj->videoModuleId]->get_private_data(obj->videoModule, &codecPrivateData, &codecPrivateDataSize);
    matroska_set_video_track(&obj->file, modules[obj->videoModuleId]->codecId, codecPrivateData, codecPrivateDataSize, &obj->vMeta);
    ms_free(codecPrivateData);
    if(f->inputs[1] != NULL)
    {
        modules[obj->audioModuleId]->get_private_data(obj->audioModule, &codecPrivateData, &codecPrivateDataSize);
        matroska_set_audio_track(&obj->file, modules[obj->audioModuleId]->codecId, codecPrivateData, codecPrivateDataSize, &obj->aMeta);
        ms_free(codecPrivateData);
    }
    matroska_write_tracks(&obj->file);

    matroska_write_cues(&obj->file);

    matroska_write_metaSeek(&obj->file);

    matroska_close_segment(&obj->file);
    matroska_close_file(&obj->file);
}

static void writer_uninit(MSFilter *f){
    MKVWriter *obj = (MKVWriter *)f->data;
    matroska_uninit(&obj->file);
    modules[obj->videoModuleId]->uninit(obj->videoModule);
    modules[obj->audioModuleId]->uninit(obj->audioModule);
    ms_free(obj);
}

static int setBitrate(MSFilter *f, void *arg)
{
    MKVWriter *data=(MKVWriter *)f->data;
    data->vMeta.bitrate = *(int *)arg;
    return 1;
}

static int setFps(MSFilter *f, void *arg)
{
    MKVWriter *data=(MKVWriter *)f->data;
    data->vMeta.fps = *(float *)arg;
    return 1;
}

static int setNbChannels(MSFilter *f, void *arg)
{
    MKVWriter *data=(MKVWriter *)f->data;
    data->aMeta.nbChannels = *(int *)arg;
    return 1;
}

static int setSampleRate(MSFilter *f, void *arg)
{
    MKVWriter *data=(MKVWriter *)f->data;
    data->aMeta.sampleRate = *(int *)arg;
    return 1;
}

static int setVideoSize(MSFilter *f, void *arg)
{
    MKVWriter *data=(MKVWriter *)f->data;
    data->vMeta.definition = *(MSVideoSize *)arg;
    return 1;
}

static MSFilterMethod writer_methods[]= {
    { MS_FILTER_SET_BITRATE, setBitrate},
    { MS_FILTER_SET_FPS, setFps},
    { MS_FILTER_SET_NCHANNELS, setNbChannels },
    { MS_FILTER_SET_SAMPLE_RATE, setSampleRate },
    { MS_FILTER_SET_VIDEO_SIZE, setVideoSize },
    { 0, NULL}
};

#ifdef _MSC_VER
MSFilterDesc ms_mkv_writer_desc={
    MS_MKV_WRITER_ID,
    "MSMKVWriter",
    "Writer of MKV files",
    MS_FILTER_OTHER,
    2,
    0,
    writer_init,
    writer_preprocess,
    writer_process,
    writer_postprocess,
    writer_uninit,
    writer_methods
};
#else
MSFilterDesc ms_mkv_writer_desc={
    .id=MS_MKV_WRITER_ID,
    .name="MSMKVWriter",
    .text="Writer of MKV files",
    .category=MS_FILTER_OTHER,
    .ninputs=2,
    .noutputs=0,
    .init=writer_init,
    .preprocess=writer_preprocess,
    .process=writer_process,
    .postprocess=writer_postprocess,
    .uninit=writer_uninit,
    .methods=writer_methods
};
#endif

MS_FILTER_DESC_EXPORT(ms_mkv_writer_desc)
