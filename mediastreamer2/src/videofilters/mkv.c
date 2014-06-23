#define bool_t ms_bool_t
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/rfc3984.h"
#include "mediastreamer2/msticker.h"
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
    obj->sampleRate = 8000;
    obj->nbChannels = 1;
    obj->bitrate = obj->nbChannels * obj->sampleRate * 16;
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
typedef void (*ModulePrivateDataLoadFunc)(void *obj, const uint8_t *data);
typedef ms_bool_t (*ModuleIsKeyFrameFunc)(const mblk_t *frame);

typedef struct
{
    const char *codecId;
    ModuleInitFunc init;
    ModuleUninitFunc uninit;
    ModuleSetFunc set;
    ModulePreProFunc preprocess;
    ModuleProFunc process;
    ModulePrivateDataFunc get_private_data;
    ModulePrivateDataLoadFunc load_private_data;
    ModuleIsKeyFrameFunc is_key_frame;
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

static void H264Private_load(H264Private *obj, const uint8_t *data)
{
    int i;

    obj->profile = data[1];
    obj->level = data[3];
    obj->NALULenghtSizeMinusOne = data[4];

    int nbSPS = data[5] & 0x1F;
    const uint8_t *r_ptr = data + 6;
    for(i=0;i<nbSPS;i++)
    {
        uint16_t nalu_size;
        memcpy(&nalu_size, r_ptr, sizeof(uint16_t)); r_ptr += sizeof(uint16_t);
        nalu_size = ntohs(nalu_size);
        mblk_t *nalu = allocb(nalu_size, 0);
        memcpy(nalu->b_wptr, r_ptr, nalu_size); nalu->b_wptr += nalu_size; r_ptr += nalu_size;
        obj->sps_list = ms_list_append(obj->sps_list, nalu);
    }

    int nbPPS = *r_ptr; r_ptr += 1;
    for(i=0;i<nbPPS;i++)
    {
        uint16_t nalu_size;
        memcpy(&nalu_size, r_ptr, sizeof(uint16_t)); r_ptr += sizeof(uint16_t);
        nalu_size = ntohs(nalu_size);
        mblk_t *nalu = allocb(nalu_size, 0);
        memcpy(nalu->b_wptr, r_ptr, nalu_size); nalu->b_wptr += nalu_size; r_ptr += nalu_size;
        obj->pps_list = ms_list_append(obj->pps_list, nalu);
    }
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

static inline int h264_nalu_type(const mblk_t *nalu)
{
    return (nalu->b_rptr[0]) & ((1<<5)-1);
}

static ms_bool_t h264_is_key_frame(const mblk_t *frame)
{
    const mblk_t *curNalu;
    for(curNalu = frame; curNalu != NULL && h264_nalu_type(curNalu) != 5; curNalu = curNalu->b_cont);
    return curNalu != NULL;
}

static void nalus_to_frame(mblk_t *buffer, mblk_t **frame, mblk_t **sps, mblk_t **pps, ms_bool_t *isKeyFrame)
{
    mblk_t *curNalu;
    *frame = NULL;
    *sps = NULL;
    *pps = NULL;
    *isKeyFrame = FALSE;
    uint32_t timecode = mblk_get_timestamp_info(buffer);

    for(curNalu = buffer; curNalu != NULL;)
    {
        mblk_t *buff = curNalu;
        curNalu = curNalu->b_cont;
        buff->b_cont = NULL;

        int type = h264_nalu_type(buff);
        switch(h264_nalu_type(buff))
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
        mblk_set_timestamp_info(*frame, timecode);
    }
    if(*sps != NULL)
    {
        msgpullup(*sps, -1);
        mblk_set_timestamp_info(*sps, timecode);
    }
    if(*pps != NULL)
    {
        msgpullup(*pps, -1);
        mblk_set_timestamp_info(*pps, timecode);
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

static void h264_module_load_private_data(void *o, const uint8_t *data)
{
    H264Module *obj = (H264Module *)o;
    H264Private_load(&obj->codecPrivate, data);
}

/* h264 module description */
const ModuleDesc h264_module_desc = {
    "V_MPEG4/ISO/AVC",
    h264_module_init,
    h264_module_uninit,
    NULL,
    h264_module_preprocessing,
    h264_module_processing,
    h264_module_get_private_data,
    h264_module_load_private_data,
    h264_is_key_frame
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
    *size = 22;
    *data = (uint8_t *)ms_new0(uint8_t, *size);
    memcpy(*data, obj, *size);
}

static inline void wav_private_load(WavPrivate *obj, const uint8_t *data)
{
    memcpy(obj, data, 22);
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

static void mu_law_module_load_private(void *o, const uint8_t *data)
{
    MuLawModule *obj = (MuLawModule *)o;
    wav_private_load(&obj->codecPrivate, data);
}

/* µLaw module description */
const ModuleDesc mu_law_module_desc = {
    "A_MS/ACM",
    mu_law_module_init,
    mu_law_module_uninit,
    mu_law_module_set,
    NULL,
    NULL,
    mu_law_module_get_private_data,
    mu_law_module_load_private,
    NULL
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
    ebml_master *segment, *cluster, *info, *tracks, *metaSeek, *cues;
    matroska_seekpoint *infoMeta, *tracksMeta, *cuesMeta;
    timecode_t timecodeScale;
    filepos_t segmentInfoPosition;
    int nbClusters;
} Matroska;

static void matroska_init(Matroska *obj)
{
    obj->p = NULL;
    obj->output = NULL;
    obj->header = NULL;
    obj->segment = NULL;
    obj->metaSeek = NULL;
    obj->infoMeta = NULL;
    obj->tracksMeta = NULL;
    obj->cuesMeta = NULL;
    obj->info = NULL;
    obj->tracks = NULL;
    obj->cues = NULL;
    obj->cluster = NULL;
    obj->nbClusters = 0;
}

static int ebml_reading_profile(const ebml_master *head)
{
    size_t length = EBML_ElementDataSize((ebml_element *)head, FALSE);
    char *docType = ms_new(char, length);
    EBML_StringGet((ebml_string *)EBML_MasterFindChild(head, &EBML_ContextDocType), docType, length);
    int docTypeReadVersion = EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(head, &EBML_ContextDocTypeReadVersion));
    int profile;

    if(strcmp(docType, "matroska")==0)
    {
        switch (docTypeReadVersion)
        {
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
    }
    else if(strcmp(docType, "webm"))
    {
        profile = PROFILE_WEBM;
    }
    else
    {
        profile = -1;
    }

    ms_free(docType);
    return profile;
}

static ms_bool_t matroska_create_file(Matroska *obj, const char path[])
{
    obj->p = (parsercontext *)ms_new(parsercontext, 1);
    ParserContext_Init(obj->p, NULL, NULL, NULL);
    loadModules((nodemodule*)obj->p);
    MATROSKA_Init((nodecontext*)obj->p);

    obj->output = StreamOpen(obj->p, path, SFLAG_WRONLY | SFLAG_CREATE);

    obj->header = EBML_ElementCreate(obj->p, &EBML_ContextHead, TRUE, NULL);
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

    return (obj->output != NULL);
}

static ms_bool_t matroska_load_file(Matroska *obj, const char path[])
{   
    int upperLevels = 0;
    ebml_parser_context readContext;
    readContext.Context = &MATROSKA_ContextStream;
    readContext.EndPosition = INVALID_FILEPOS_T;
    readContext.Profile = 0;
    readContext.UpContext = NULL;

    obj->p = (parsercontext *)ms_new(parsercontext, 1);
    ParserContext_Init(obj->p, NULL, NULL, NULL);
    loadModules((nodemodule*)obj->p);
    MATROSKA_Init((nodecontext*)obj->p);

    obj->output = StreamOpen(obj->p, path, SFLAG_REOPEN);

    obj->header = EBML_FindNextElement(obj->output, &readContext, &upperLevels, FALSE);
    EBML_ElementReadData(obj->header, obj->output, &readContext, FALSE, SCOPE_ALL_DATA, 0);
    readContext.Profile = ebml_reading_profile((ebml_master *)obj->header);

    obj->segment = (ebml_master *)EBML_FindNextElement(obj->output, &readContext, &upperLevels, FALSE);
    readContext.EndPosition = EBML_ElementPositionEnd((ebml_element *)obj->segment);

    ebml_parser_context readSegmentContext;
    readSegmentContext.Context = EBML_ElementContext((ebml_element *)obj->segment);
    readSegmentContext.EndPosition = EBML_ElementPositionEnd((ebml_element *)obj->segment);
    readSegmentContext.UpContext = &readContext;
    readSegmentContext.Profile = ebml_reading_profile((ebml_master *)obj->header);

    ebml_element *elt;
    for(elt = EBML_FindNextElement(obj->output, &readSegmentContext, &upperLevels, FALSE); elt != NULL; elt = EBML_FindNextElement(obj->output, &readSegmentContext, &upperLevels, FALSE))
    {
        if(EBML_ElementIsType(elt, &MATROSKA_ContextSeekHead))
        {
            EBML_ElementReadData(elt, obj->output, &readSegmentContext, FALSE, SCOPE_ALL_DATA, 0);
            EBML_MasterAppend(obj->segment, elt);
            obj->metaSeek = (ebml_master*)elt;
            matroska_seekpoint *seekPoint;
            for(seekPoint = (matroska_seekpoint *)EBML_MasterChildren(obj->metaSeek); seekPoint != NULL; seekPoint = (matroska_seekpoint *)EBML_MasterNext(seekPoint))
            {
                if(MATROSKA_MetaSeekIsClass(seekPoint, &MATROSKA_ContextInfo))
                {
                    obj->infoMeta = seekPoint;
                }
                else if(MATROSKA_MetaSeekIsClass(seekPoint, &MATROSKA_ContextTracks))
                {
                    obj->tracksMeta = seekPoint;
                }
                else if(MATROSKA_MetaSeekIsClass(seekPoint, &MATROSKA_ContextCues))
                {
                    obj->cuesMeta = seekPoint;
                }
            }
        }
        else if(EBML_ElementIsType(elt, &MATROSKA_ContextInfo))
        {
            EBML_ElementReadData(elt, obj->output, &readSegmentContext, FALSE, SCOPE_ALL_DATA, 0);
            EBML_MasterAppend(obj->segment, elt);
            obj->info = (ebml_master*)elt;
            obj->timecodeScale = EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(obj->info, &MATROSKA_ContextTimecodeScale));
            MATROSKA_LinkMetaSeekElement(obj->infoMeta, (ebml_element *)obj->info);
        }
        else if(EBML_ElementIsType(elt, &MATROSKA_ContextTracks))
        {
            EBML_ElementReadData(elt, obj->output, &readSegmentContext, FALSE, SCOPE_ALL_DATA, 0);
            EBML_MasterAppend(obj->segment, elt);
            obj->tracks = (ebml_master*)elt;
            MATROSKA_LinkMetaSeekElement(obj->tracksMeta, (ebml_element *)obj->tracks);
        }
        else if(EBML_ElementIsType(elt, &MATROSKA_ContextCues))
        {
            EBML_ElementReadData(elt, obj->output, &readSegmentContext, FALSE, SCOPE_ALL_DATA, 0);
            EBML_MasterAppend(obj->segment, elt);
            obj->cues = (ebml_master*)elt;
            MATROSKA_LinkMetaSeekElement(obj->cuesMeta, (ebml_element *)obj->cues);
        }
        else if(EBML_ElementIsType(elt, &MATROSKA_ContextCluster))
        {
            EBML_ElementReadData(elt, obj->output, &readSegmentContext, FALSE, SCOPE_PARTIAL_DATA, 0);
            EBML_MasterAppend(obj->segment, elt);
            obj->cluster = (ebml_master *)elt;
            MATROSKA_LinkClusterBlocks((matroska_cluster *)obj->cluster, obj->segment, obj->tracks, FALSE);
            obj->nbClusters++;
        }
    }
    return TRUE;
}

static ms_bool_t matroska_open_file(Matroska *obj, const char path[], ms_bool_t appendMode)
{
    ms_bool_t success;
    if(appendMode)
    {
        success = matroska_load_file(obj, path);
        if(success)
        {
            if(obj->cues == NULL)
            {
                obj->cues = (ebml_master *)EBML_ElementCreate(obj->p, &MATROSKA_ContextCues, FALSE, NULL);
            }
            if(obj->cluster == NULL)
            {
                Stream_Seek(obj->output, 0, SEEK_END);
            }
            else
            {
                Stream_Seek(obj->output, EBML_ElementPositionEnd((ebml_element *)obj->cluster), SEEK_SET);
            }
        }
    }
    else
    {
        success = matroska_create_file(obj, path);
    }
    return success;
}

static void matroska_close_file(Matroska *obj)
{
    StreamClose(obj->output);
    NodeDelete((node *)obj->header);
    NodeDelete((node *)obj->segment);

    MATROSKA_Done((nodecontext*)obj->p);
    ParserContext_Done(obj->p);
    ms_free(obj->p);
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

static inline timecode_t matroska_get_duration(const Matroska *obj)
{
    return (timecode_t)EBML_FloatValue((ebml_float *)EBML_MasterFindChild(obj->info, &MATROSKA_ContextDuration));
}

//static timecode_t matroska_last_block_timecode(const Matroska *obj)
//{
//    if(obj->cluster == NULL)
//    {
//        return -1;
//    }
//    else
//    {
//        ebml_element *block, *lastBlock = NULL;
//        for(block = EBML_MasterFindFirstElt(obj->cluster, &MATROSKA_ContextSimpleBlock, FALSE, FALSE);
//            block != NULL;
//            block = EBML_MasterFindNextElt(obj->cluster, block, FALSE, FALSE))
//        {
//            lastBlock = block;
//        }
//        if(lastBlock == NULL)
//        {
//            return -1;
//        }
//        else
//        {
//            return MATROSKA_BlockTimecode((matroska_block *)lastBlock)/obj->timecodeScale;
//        }
//    }
//}

static void updateElementHeader(ebml_element *element, stream *file)
{
    filepos_t initial_pos = Stream_Seek(file, 0, SEEK_CUR);
    Stream_Seek(file, EBML_ElementPosition(element), SEEK_SET);
    EBML_ElementUpdateSize(element, WRITE_DEFAULT_ELEMENT, FALSE);
    EBML_ElementRenderHead(element, file, FALSE, NULL);
    Stream_Seek(file, initial_pos, SEEK_SET);
}

static inline void matroska_start_segment(Matroska *obj)
{
    EBML_ElementSetSizeLength((ebml_element *)obj->segment, 8);
    EBML_ElementRenderHead((ebml_element *)obj->segment, obj->output, FALSE, NULL);
}

static int matroska_write_zeros(Matroska *obj, size_t nbZeros)
{
    uint8_t *data = (uint8_t *)ms_new0(uint8_t, nbZeros);
    if(data == NULL)
    {
        return 0;
    }

    size_t written;
    Stream_Write(obj->output, data, nbZeros, &written);
    ms_free(data);
    return written;
}

static inline void matroska_mark_segment_info_position(Matroska *obj)
{
    obj->segmentInfoPosition = Stream_Seek(obj->output, 0, SEEK_CUR);
}

static inline void matroska_go_to_segment_info_mark(Matroska *obj)
{
    Stream_Seek(obj->output, obj->segmentInfoPosition, SEEK_SET);
}

static inline void matroska_go_to_file_end(Matroska *obj)
{
    Stream_Seek(obj->output, 0, SEEK_END);
}

static inline void matroska_go_to_segment_begin(Matroska *obj)
{
    Stream_Seek(obj->output, EBML_ElementPositionData((ebml_element *)obj->segment), SEEK_SET);
}

static inline void matroska_go_to_last_cluster_end(Matroska *obj)
{
    Stream_Seek(obj->output, EBML_ElementPositionEnd((ebml_element *)obj->cluster), SEEK_SET);
}

static inline void matroska_go_to_segment_info_begin(Matroska *obj)
{
    Stream_Seek(obj->output, EBML_ElementPosition((ebml_element *)obj->info), SEEK_SET);
}

static int ebml_element_cmp_position(const void *a, const void *b)
{
    return EBML_ElementPosition((ebml_element *)a) - EBML_ElementPosition((ebml_element *)b);
}

static void ebml_master_sort(ebml_master *master_elt)
{
    MSList *elts = NULL;
    ebml_element *elt;
    for(elt = EBML_MasterChildren(master_elt); elt != NULL; elt = EBML_MasterNext(elt))
    {
        elts = ms_list_insert_sorted(elts, elt, (MSCompareFunc)ebml_element_cmp_position);
    }
    EBML_MasterClear(master_elt);
    MSList *it;
    for(it = elts; it != NULL; it = ms_list_next(it))
    {
        EBML_MasterAppend(master_elt, (ebml_element *)it->data);
    }
    ms_list_free(elts);
}

static int ebml_master_fill_blanks(stream *output, ebml_master *master)
{
    MSList *voids = NULL;
    ebml_element *elt1, *elt2;
    for(elt1 = EBML_MasterChildren(master), elt2 = EBML_MasterNext(elt1); elt2 != NULL; elt1 = EBML_MasterNext(elt1), elt2 = EBML_MasterNext(elt2))
    {
        filepos_t elt1_end_pos = EBML_ElementPositionEnd(elt1);
        filepos_t elt2_pos = EBML_ElementPosition(elt2);
        int interval = elt2_pos - elt1_end_pos;
        if(interval < 0)
        {
            return -1; // Elements are neither contigus or distinct.
        }
        else if(interval == 0)
        {
            // Nothing to do. Elements are contigus.
        }
        else if(interval > 0 && interval < 2)
        {
            return -2; // Not enough space to write a void element.
        }
        else
        {
            ebml_element *voidElt = EBML_ElementCreate(master, &EBML_ContextEbmlVoid, TRUE, NULL);
            EBML_VoidSetFullSize(voidElt, interval);
            Stream_Seek(output, elt1_end_pos, SEEK_SET);
            EBML_ElementRender(voidElt, output, FALSE, FALSE, FALSE, NULL);
            voids = ms_list_append(voids, voidElt);
        }
    }

    MSList *it;
    for(it = voids; it != NULL; it = ms_list_next(it))
    {
        EBML_MasterAppend(master, (ebml_element *)it->data);
    }
    ms_list_free(voids);
    return 0;
}

static void ebml_master_delete_empty_elements(ebml_master *master)
{
    ebml_element *child;
    for(child = EBML_MasterChildren(master); child != NULL; child = EBML_MasterNext(child))
    {
        if(EBML_ElementDataSize(child, WRITE_DEFAULT_ELEMENT) <= 0)
        {
            EBML_MasterRemove(master, child);
            NodeDelete((node *)child);
        }
    }
}

static int matroska_close_segment(Matroska *obj)
{
    filepos_t initialPos = Stream_Seek(obj->output, 0, SEEK_CUR);
    EBML_ElementUpdateSize(obj->segment, WRITE_DEFAULT_ELEMENT, FALSE);
    ebml_master_delete_empty_elements(obj->segment);
    ebml_master_sort(obj->segment);
    if(ebml_master_fill_blanks(obj->output, obj->segment) < 0)
    {
        return -1;
    }
    updateElementHeader((ebml_element *)obj->segment, obj->output);
    Stream_Seek(obj->output, initialPos, SEEK_SET);
    return 0;
}

static ebml_master *matroska_find_track_entry(const Matroska *obj, int trackNum)
{
    ebml_element *trackEntry;
    for(trackEntry = EBML_MasterChildren(obj->tracks);
        trackEntry != NULL && EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild((ebml_master *)trackEntry, &MATROSKA_ContextTrackNumber)) != trackNum;
        trackEntry = EBML_MasterNext(trackEntry));
    return (ebml_master *)trackEntry;
}

static ms_bool_t matroska_get_codec_private(const Matroska *obj, int trackNum, const uint8_t **data, size_t *length)
{
    ebml_master *trackEntry = matroska_find_track_entry(obj, trackNum);
    if(trackEntry == NULL)
        return FALSE;
    ebml_binary *codecPrivate = (ebml_binary *)EBML_MasterFindChild(trackEntry, &MATROSKA_ContextCodecPrivate);
    if(codecPrivate == NULL)
        return FALSE;

    *length = EBML_ElementDataSize((ebml_element *)codecPrivate, FALSE);
    *data =  EBML_BinaryGetData(codecPrivate);
    if(*data == NULL)
        return FALSE;
    else
        return TRUE;
}

static void matroska_start_cluster(Matroska *obj, timecode_t clusterTimecode)
{
    obj->cluster = (ebml_master *)EBML_MasterAddElt(obj->segment, &MATROSKA_ContextCluster, TRUE);
    EBML_ElementSetSizeLength((ebml_element *)obj->cluster, 8);
    EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(obj->cluster, &MATROSKA_ContextTimecode), clusterTimecode);
    EBML_ElementRender((ebml_element *)obj->cluster, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
    obj->nbClusters++;
}

static void matroska_close_cluster(Matroska *obj)
{
    ebml_element *block;
    if(obj->cluster == NULL)
    {
        return;
    }
    else
    {
        block = EBML_MasterFindChild(obj->cluster, &MATROSKA_ContextSimpleBlock);
    }
    if(block == NULL)
    {
        ebml_element *voidElt = EBML_ElementCreate(obj->p, &EBML_ContextEbmlVoid, FALSE, NULL);
        EBML_MasterAppend(obj->segment, voidElt);
        EBML_VoidSetFullSize(voidElt, EBML_ElementFullSize((ebml_element *)obj->cluster, WRITE_DEFAULT_ELEMENT));
        Stream_Seek(obj->output, EBML_ElementPosition((ebml_element *)obj->cluster), SEEK_SET);
        EBML_ElementRender(voidElt, obj->output, FALSE, FALSE, FALSE, NULL);
        EBML_MasterRemove(obj->segment, (ebml_element *)obj->cluster);
        NodeDelete((node *)obj->cluster);
        obj->cluster = NULL;
    }
    else
    {
        updateElementHeader((ebml_element *)obj->cluster, obj->output);
    }
}

static inline int matroska_clusters_count(const Matroska *obj)
{
    return obj->nbClusters;
}

static inline timecode_t matroska_current_cluster_timecode(const Matroska *obj)
{
    return EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(obj->cluster, &MATROSKA_ContextTimecode));
}

static ebml_element *matroska_find_track(Matroska *obj, int trackNum)
{
    ebml_element *elt;
    for(elt = EBML_MasterChildren(obj->tracks);
        elt != NULL && EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(elt, &MATROSKA_ContextTrackNumber)) != trackNum;
        elt = EBML_MasterNext(elt));
    return elt;
}

static int matroska_add_track(Matroska *obj, int trackNum, int trackType, const char codecID[])
{
    ebml_element *track = matroska_find_track(obj, trackNum);
    if(track != NULL)
    {
        return -1;
    }
    else
    {
        track = EBML_MasterAddElt(obj->tracks, &MATROSKA_ContextTrackEntry, FALSE);
        if(track == NULL)
        {
            return -2;
        }
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
        EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(track, &MATROSKA_ContextCodecDecodeAll), 0);
        return 0;
    }
}

static int matroska_del_track(Matroska *obj, int trackNum)
{
    ebml_element *track = matroska_find_track(obj, trackNum);
    if(track == NULL)
    {
        return -1;
    }
    else
    {
        if(EBML_MasterRemove(obj->tracks, track) != ERR_NONE)
        {
            return -2;
        }
        else
        {
            NodeDelete((node *)track);
            return 0;
        }
    }
}

static int matroska_track_set_codec_private(Matroska *obj, int trackNum, const uint8_t *data, size_t dataSize)
{
    ebml_element *track = matroska_find_track(obj, trackNum);
    if(track == NULL)
    {
        return -1;
    }
    else
    {
        ebml_binary *codecPrivate = EBML_MasterGetChild(track, &MATROSKA_ContextCodecPrivate);
        if(EBML_BinarySetData(codecPrivate, data, dataSize) != ERR_NONE)
        {
            return -2;
        }
        else
        {
            return 0;
        }
    }
}

static int matroska_track_set_video_info(Matroska *obj, int trackNum, const VideoMetaData *vMeta)
{
    ebml_element *track = matroska_find_track(obj, trackNum);
    if(track == NULL)
    {
        return -1;
    }
    else
    {
        int trackType = EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(track, &MATROSKA_ContextTrackType));
        if(trackType != TRACK_TYPE_VIDEO)
        {
            return -2;
        }
        else
        {
            ebml_element *videoInfo = EBML_MasterGetChild(track, &MATROSKA_ContextVideo);
            EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(videoInfo, &MATROSKA_ContextFlagInterlaced), 0);
            EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(videoInfo, &MATROSKA_ContextPixelWidth), vMeta->definition.width);
            EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(videoInfo, &MATROSKA_ContextPixelHeight), vMeta->definition.height);
            return 0;
        }
    }
}

static int matroska_track_set_audio_info(Matroska *obj, int trackNum, const AudioMetaData *aMeta)
{
    ebml_element *track = matroska_find_track(obj, trackNum);
    if(track == NULL)
    {
        return -1;
    }
    else
    {
        int trackType = EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(track, &MATROSKA_ContextTrackType));
        if(trackType != TRACK_TYPE_AUDIO)
        {
            return -2;
        }
        else
        {
            ebml_element *audioInfo = EBML_MasterGetChild(track, &MATROSKA_ContextAudio);
            EBML_FloatSetValue((ebml_float *)EBML_MasterGetChild(audioInfo, &MATROSKA_ContextSamplingFrequency), aMeta->sampleRate);
            EBML_IntegerSetValue((ebml_integer *)EBML_MasterGetChild(audioInfo, &MATROSKA_ContextChannels), aMeta->nbChannels);
            return 0;
        }
    }
}

static int matroska_add_cue(Matroska *obj, matroska_block *block)
{
    matroska_cuepoint *cue = (matroska_cuepoint *)EBML_MasterAddElt(obj->cues, &MATROSKA_ContextCuePoint, TRUE);
    if(cue == NULL)
    {
        return -1;
    }
    else
    {
        MATROSKA_LinkCuePointBlock(cue, block);
        MATROSKA_LinkCueSegmentInfo(cue, obj->info);
        MATROSKA_CuePointUpdate(cue, (ebml_element *)obj->segment);
        return 0;
    }
}

static inline void matroska_write_segment_info(Matroska *obj)
{
    EBML_ElementRender((ebml_element *)obj->info, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
    MATROSKA_MetaSeekUpdate(obj->infoMeta);
}

static inline void matroska_write_tracks(Matroska *obj)
{
    EBML_ElementRender((ebml_element *)obj->tracks, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
    MATROSKA_MetaSeekUpdate(obj->tracksMeta);
}

static inline void matroska_write_cues(Matroska *obj)
{
    EBML_ElementRender((ebml_element *)obj->cues, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
    MATROSKA_MetaSeekUpdate(obj->cuesMeta);
}

static inline void matroska_write_metaSeek(Matroska *obj)
{
    EBML_ElementRender((ebml_element *)obj->metaSeek, obj->output, WRITE_DEFAULT_ELEMENT, FALSE, FALSE, NULL);
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
 * Muxer                                                                                     *
 *********************************************************************************************/
typedef enum {
    VIDEO_BUFFER,
    AUDIO_BUFFER
} MuxerBufferType;

typedef struct
{
    MSQueue videoQueue;
    MSQueue audioQueue;
} Muxer;

static void muxer_init(Muxer *obj)
{
    ms_queue_init(&obj->videoQueue);
    ms_queue_init(&obj->audioQueue);
}

static void muxer_empty_internal_queues(Muxer *obj)
{
    ms_queue_flush(&obj->videoQueue);
    ms_queue_flush(&obj->audioQueue);
}

static void muxer_uninit(Muxer *obj)
{
    ms_queue_flush(&obj->videoQueue);
    ms_queue_flush(&obj->audioQueue);
}

static inline void muxer_put_video_buffer(Muxer *obj, mblk_t *buffer)
{
    ms_queue_put(&obj->videoQueue, buffer);
}

static inline void muxer_put_audio_buffer(Muxer *obj, mblk_t *buffer)
{
    ms_queue_put(&obj->audioQueue, buffer);
}

static mblk_t *muxer_get_buffer(Muxer *obj, MuxerBufferType *bufferType)
{
    if(ms_queue_empty(&obj->videoQueue) && ms_queue_empty(&obj->audioQueue))
    {
        return NULL;
    }
    else if(!ms_queue_empty(&obj->videoQueue) && ms_queue_empty(&obj->audioQueue))
    {
        *bufferType = VIDEO_BUFFER;
        return ms_queue_get(&obj->videoQueue);
    }
    else if(ms_queue_empty(&obj->videoQueue) && !ms_queue_empty(&obj->audioQueue))
    {
        *bufferType = AUDIO_BUFFER;
        return ms_queue_get(&obj->audioQueue);
    }
    else
    {
        uint32_t videoTimecode = mblk_get_timestamp_info(qfirst(&obj->videoQueue.q));
        uint32_t audioTimecode = mblk_get_timestamp_info(qfirst(&obj->audioQueue.q));
        if(videoTimecode <= audioTimecode)
        {
            *bufferType = VIDEO_BUFFER;
            return ms_queue_get(&obj->videoQueue);
        }
        else
        {
            *bufferType = AUDIO_BUFFER;
            return ms_queue_get(&obj->audioQueue);
        }
    }
}

/*********************************************************************************************
 * MKV Writer Filter                                                                         *
 *********************************************************************************************/
#define CLUSTER_MAX_DURATION 5000

typedef struct
{
    Matroska file;
    VideoMetaData vMeta;
    AudioMetaData aMeta;
    int videoInputNum, audioInputNum;
    ModuleId videoModuleId, audioModuleId;
    void *videoModule, *audioModule;
    timecode_t duration, clusterTime, timeOffset, lastFrameTimecode;
    ms_bool_t appendMode;
    MSRecorderState state;
    Muxer muxer;
    ms_bool_t needKeyFrame, firstFrame, haveVideoTrack, haveAudioTrack;
} MKVWriter;

static void writer_init(MSFilter *f)
{
    MKVWriter *obj = (MKVWriter *)ms_new(MKVWriter, 1);

    matroska_init(&obj->file);

    video_meta_data_init(&obj->vMeta);
    audio_meta_data_init(&obj->aMeta);

    obj->videoModuleId = H264_MOD_ID;
    obj->audioModuleId = MU_LAW_MOD_ID;
    obj->videoInputNum = 0;
    obj->audioInputNum = 1;
    modules[obj->videoModuleId]->init(&obj->videoModule);
    modules[obj->audioModuleId]->init(&obj->audioModule);

    obj->state = MSRecorderClosed;
    obj->needKeyFrame = TRUE;
    obj->firstFrame = TRUE;
    obj->haveVideoTrack = FALSE;
    obj->haveAudioTrack = FALSE;

    muxer_init(&obj->muxer);

    obj->timeOffset = 0;

    f->data=obj;
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

    matroska_frame m_frame;
    m_frame.Timecode = ((int64_t)mblk_get_timestamp_info(frame))*1000000LL;
    m_frame.Size = msgdsize(frame);
    m_frame.Data = frame->b_rptr;

    if(matroska_clusters_count(&obj->file) == 0)
    {
        matroska_start_cluster(&obj->file, mblk_get_timestamp_info(frame));
    }
    else
    {
        if((trackNum == 1 && isKeyFrame) || (obj->duration - matroska_current_cluster_timecode(&obj->file) >= CLUSTER_MAX_DURATION))
        {
            matroska_close_cluster(&obj->file);
            matroska_start_cluster(&obj->file, mblk_get_timestamp_info(frame));
        }
    }

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

static int writer_open(MSFilter *f, void *arg)
{
    MKVWriter *obj = (MKVWriter *)f->data;
    const char *filename = (const char *)arg;
    int err = 0;

    ms_filter_lock(f);
    if(obj->state == MSRecorderClosed)
    {
        if (access(filename, R_OK | W_OK) == 0)
            obj->appendMode = TRUE;
        else
            obj->appendMode = FALSE;

        if(!matroska_open_file(&obj->file, filename, obj->appendMode))
        {
            err = -1;
        }
        else
        {
            if(!obj->appendMode)
            {
                matroska_set_doctype_version(&obj->file, MKV_DOCTYPE_VERSION, MKV_DOCTYPE_READ_VERSION);
                matroska_write_ebml_header(&obj->file);
                matroska_start_segment(&obj->file);
                matroska_write_zeros(&obj->file, 1024);
                matroska_mark_segment_info_position(&obj->file);
                matroska_write_zeros(&obj->file, 1024);

                if(f->inputs[obj->videoInputNum] != NULL)
                {
                    matroska_add_track(&obj->file, 1, TRACK_TYPE_VIDEO, modules[obj->videoModuleId]->codecId);
                    matroska_track_set_video_info(&obj->file, 1, &obj->vMeta);
                    if(modules[obj->videoModuleId]->set != NULL)
                        modules[obj->videoModuleId]->set(obj->videoModule, &obj->vMeta);
                }
                if(f->inputs[obj->audioInputNum] != NULL)
                {
                    matroska_add_track(&obj->file, 2, TRACK_TYPE_AUDIO, modules[obj->audioModuleId]->codecId);
                    matroska_track_set_audio_info(&obj->file, 2, &obj->aMeta);
                    if(modules[obj->audioModuleId]->set != NULL)
                        modules[obj->audioModuleId]->set(obj->audioModule, &obj->aMeta);
                }
                obj->duration = -1;
            }
            else
            {
                const uint8_t *data;
                size_t length;
                matroska_get_codec_private(&obj->file, 1, &data, &length);
                modules[obj->videoModuleId]->load_private_data(obj->videoModule, data);
                if(f->inputs[obj->audioInputNum] != NULL)
                {
                    matroska_get_codec_private(&obj->file, 2, &data, &length);
                    modules[obj->audioModuleId]->load_private_data(obj->audioModule, data);
                }
                obj->duration = matroska_get_duration(&obj->file) + 1;
            }
            obj->state = MSRecorderPaused;
        }
    }
    ms_filter_unlock(f);
    return err;
}

static int writer_start(MSFilter *f, void *arg)
{
    MKVWriter *obj = (MKVWriter *)f->data;
    int err = 0;

    ms_filter_lock(f);
    if(obj->state == MSRecorderClosed)
    {
        err = -1;
    }
    else
    {
        obj->state = MSRecorderRunning;
        obj->needKeyFrame = TRUE;
        obj->firstFrame = TRUE;
    }
    ms_filter_unlock(f);
    return err;
}

static int writer_stop(MSFilter *f, void *arg)
{
    MKVWriter *obj = (MKVWriter *)f->data;
    int err;

    ms_filter_lock(f);
    if(obj->state == MSRecorderClosed)
    {
        err = -1;
    }
    else
    {
        obj->state = MSRecorderPaused;
        muxer_empty_internal_queues(&obj->muxer);
        err = 0;
    }
    ms_filter_unlock(f);
    return err;
}

static void writer_process(MSFilter *f)
{
    MKVWriter *obj = f->data;

    ms_filter_lock(f);
    if(obj->state == MSRecorderRunning)
    {
        mblk_t *buffer;
        MuxerBufferType bufferType;

        if(f->inputs[obj->videoInputNum] != NULL)
        {
            MSQueue frames, frames_ms;
            ms_queue_init(&frames);
            ms_queue_init(&frames_ms);

            if(modules[obj->videoModuleId]->preprocess != NULL)
                modules[obj->videoModuleId]->preprocess(obj->videoModule, f->inputs[obj->videoInputNum], &frames);
            else
                moveBuffers(f->inputs[obj->videoInputNum], &frames);

            while((buffer = ms_queue_get(&frames)) != NULL)
            {
                changeClockRate(buffer, 90000, 1000);
                ms_queue_put(&frames_ms, buffer);
            }

            if(obj->needKeyFrame)
            {
                while((buffer = ms_queue_get(&frames_ms)) != NULL)
                {
                    if(modules[obj->videoModuleId]->is_key_frame(buffer))
                        break;
                    else
                        freemsg(buffer);
                }
                if(buffer != NULL)
                {
                    muxer_put_video_buffer(&obj->muxer, buffer);
                    while((buffer = ms_queue_get(&frames_ms)) != NULL)
                        muxer_put_video_buffer(&obj->muxer, buffer);
                    obj->needKeyFrame = FALSE;
                }
            }
            else
            {
                while((buffer = ms_queue_get(&frames_ms)) != NULL)
                {
                    muxer_put_video_buffer(&obj->muxer, buffer);
                }
            }
        }

        if(f->inputs[obj->audioInputNum] != NULL)
        {
            MSQueue audioBuffers;
            ms_queue_init(&audioBuffers);

            if(f->inputs[obj->audioInputNum] && modules[obj->audioModuleId]->preprocess != NULL)
                modules[obj->audioModuleId]->preprocess(obj->audioModule, f->inputs[obj->audioInputNum], &audioBuffers);
            else
                moveBuffers(f->inputs[obj->audioInputNum], &audioBuffers);

            while((buffer = ms_queue_get(&audioBuffers)) != NULL)
            {
                changeClockRate(buffer, obj->aMeta.sampleRate, 1000);
                muxer_put_audio_buffer(&obj->muxer, buffer);
            }
        }

        while((buffer = muxer_get_buffer(&obj->muxer, &bufferType)) != NULL)
        {
            if(obj->firstFrame)
            {
                obj->timeOffset = obj->duration - mblk_get_timestamp_info(buffer) + 1;
                obj->firstFrame = FALSE;
            }
            mblk_set_timestamp_info(buffer, mblk_get_timestamp_info(buffer) + obj->timeOffset);
            timecode_t bufferTimecode = mblk_get_timestamp_info(buffer);

            if(bufferType == VIDEO_BUFFER)
            {
                matroska_block *block = write_frame(obj, buffer, 1, obj->videoModuleId, obj->videoModule);
                matroska_add_cue(&obj->file, block);
                obj->haveVideoTrack = TRUE;
            }
            else
            {
                matroska_block *block = write_frame(obj, buffer, 2, obj->audioModuleId, obj->audioModule);
                if(!obj->haveVideoTrack)
                {
                    matroska_add_cue(&obj->file, block);
                }
                obj->haveAudioTrack = TRUE;
            }
            if(bufferTimecode > obj->duration)
            {
                obj->duration = bufferTimecode;
            }
        }
    }
    else
    {
        if(f->inputs[obj->videoInputNum] != NULL)
            ms_queue_flush(f->inputs[obj->videoInputNum]);
        if(f->inputs[obj->audioInputNum] != NULL)
            ms_queue_flush(f->inputs[obj->audioInputNum]);
    }
    ms_filter_unlock(f);
}

static int writer_close(MSFilter *f, void *arg)
{
    MKVWriter *obj = (MKVWriter *)f->data;

    ms_filter_lock(f);
    if(obj->state != MSRecorderClosed)
    {
        if(f->inputs[obj->videoInputNum] != NULL)
            ms_queue_flush(f->inputs[obj->videoInputNum]);
        if(f->inputs[obj->audioInputNum] != NULL)
            ms_queue_flush(f->inputs[obj->audioInputNum]);
        muxer_empty_internal_queues(&obj->muxer);

        if(f->inputs[obj->videoInputNum] != NULL)
        {
            if(obj->haveVideoTrack)
            {
                uint8_t *codecPrivateData;
                size_t codecPrivateDataSize;
                modules[obj->videoModuleId]->get_private_data(obj->videoModule, &codecPrivateData, &codecPrivateDataSize);
                matroska_track_set_codec_private(&obj->file, 1, codecPrivateData, codecPrivateDataSize);
                ms_free(codecPrivateData);
            }
            else
            {
                matroska_del_track(&obj->file, 1);
            }
        }
        if(f->inputs[obj->audioInputNum] != NULL)
        {
            if(obj->haveAudioTrack)
            {
                uint8_t *codecPrivateData;
                size_t codecPrivateDataSize;
                modules[obj->audioModuleId]->get_private_data(obj->audioModule, &codecPrivateData, &codecPrivateDataSize);
                matroska_track_set_codec_private(&obj->file, 2, codecPrivateData, codecPrivateDataSize);
                ms_free(codecPrivateData);
            }
            else
            {
                matroska_del_track(&obj->file, 2);
            }

        }

        matroska_close_cluster(&obj->file);
        matroska_write_cues(&obj->file);

        if(!obj->appendMode)
        {
            matroska_go_to_segment_info_mark(&obj->file);
        }
        else
        {
            matroska_go_to_segment_info_begin(&obj->file);
        }
        matroska_set_segment_info(&obj->file, "libmediastreamer2", "libmediastreamer2", obj->duration);
        matroska_write_segment_info(&obj->file);
        matroska_write_tracks(&obj->file);
        matroska_go_to_segment_begin(&obj->file);
        matroska_write_metaSeek(&obj->file);
        matroska_go_to_file_end(&obj->file);
        matroska_close_segment(&obj->file);
        matroska_close_file(&obj->file);

        obj->state = MSRecorderClosed;
    }
    ms_filter_unlock(f);

    return 0;
}

static void writer_uninit(MSFilter *f){
    MKVWriter *obj = (MKVWriter *)f->data;
    ms_filter_lock(f);
    modules[obj->videoModuleId]->uninit(obj->videoModule);
    modules[obj->audioModuleId]->uninit(obj->audioModule);
    muxer_uninit(&obj->muxer);
    if(obj->state != MSRecorderClosed)
    {
        matroska_close_file(&obj->file);
    }
    ms_free(obj);
    ms_filter_unlock(f);
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
    {   MS_FILTER_SET_BITRATE       ,   setBitrate          },
    {   MS_FILTER_SET_FPS           ,   setFps              },
    {   MS_FILTER_SET_NCHANNELS     ,   setNbChannels       },
    {   MS_FILTER_SET_SAMPLE_RATE   ,   setSampleRate       },
    {   MS_FILTER_SET_VIDEO_SIZE    ,   setVideoSize        },
    {	MS_RECORDER_OPEN            ,	writer_open         },
    {	MS_RECORDER_CLOSE           ,	writer_close        },
    {	MS_RECORDER_START           ,	writer_start        },
    {	MS_RECORDER_PAUSE           ,	writer_stop         },
    {   0                           ,   NULL                }
};

#ifdef _MSC_VER
MSFilterDesc ms_mkv_writer_desc= {
    MS_MKV_WRITER_ID,
    "MSMKVWriter",
    "Writer of MKV files",
    MS_FILTER_OTHER,
    2,
    0,
    writer_init,
    NULL,
    writer_process,
    NULL,
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
    .preprocess=NULL,
    .process=writer_process,
    .postprocess=NULL,
    .uninit=writer_uninit,
    .methods=writer_methods
};
#endif

MS_FILTER_DESC_EXPORT(ms_mkv_writer_desc)
