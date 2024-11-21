/*!
 *  @file libxmedia/src/codec.c
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the video/audio codec
 * functionality based on the FFMPEG API and libraries.
 */

#include "codec.h"

#define XCODEC_APPLY_RATIONAL(dst, src) if (src.num >= 0 && src.den >= 0) dst = src
#define XCODEC_APPLY_INTEGER(dst, src) if (src >= 0) dst = src

#define XNAL_UNIT_START_CODE    "\x00\x00\x00\x01"
#define XOPUS_HEADER_SIZE       19

uint8_t* X264_CreateExtra(x264_extra_t *pExtra, int *pExtraSize)
{
    XASSERT((pExtra && pExtra->pSPS && pExtra->pPPS &&
             pExtra->nSPSSize && pExtra->nPPSSize), NULL);

    /* Allocate extradata */
    int nExtraDataSize = pExtra->nSPSSize + pExtra->nPPSSize + 8;
    uint8_t *pExtraData = av_mallocz(nExtraDataSize);
    XASSERT(pExtraData, NULL);

    /* Write NAL unit start code and SPS data */
    memcpy(pExtraData, XNAL_UNIT_START_CODE, 4);
    memcpy(&pExtraData[4], pExtra->pSPS, pExtra->nSPSSize);

    /* Write NAL unit start code and PPS data */
    memcpy(&pExtraData[4 + pExtra->nSPSSize], XNAL_UNIT_START_CODE, 4);
    memcpy(&pExtraData[8 + pExtra->nSPSSize], pExtra->pPPS, pExtra->nPPSSize);

    /* Reset buffer padding and set extradata size */
    if (pExtraSize) *pExtraSize = nExtraDataSize;
    return pExtraData;
}

uint8_t* XOPUS_CreateExtra(xopus_header_t *pHeader, int *pExtraSize)
{
    /* Allocate extradata */
    uint8_t *pExtraData = av_mallocz(XOPUS_HEADER_SIZE);
    XASSERT(pExtraData, NULL);

    /* Set the fixed parts of the header */
    const char sOputsHead[] = "OpusHead";
    memcpy(pExtraData, sOputsHead, 8);
    pExtraData[8] = 1; // Version (0 or 1)

    /* Set the variable parts of the header */
    pExtraData[9] = pHeader->nChannels;
    *((uint16_t *)(pExtraData + 10)) = pHeader->nPreSkip;
    *((uint32_t *)(pExtraData + 12)) = pHeader->nInputSampleRate;
    *((int16_t *)(pExtraData + 16)) = pHeader->nOutputGain;
    pExtraData[18] = pHeader->nChannelMappingFamily;

    /* The basic OpusHead structure has a size of 19 bytes */
    if (pExtraSize) *pExtraSize = XOPUS_HEADER_SIZE;
    return pExtraData;
}

void XCodec_Init(xcodec_t *pInfo)
{
    XASSERT_VOID(pInfo);
    pInfo->mediaType = AVMEDIA_TYPE_UNKNOWN;
    pInfo->codecId = AV_CODEC_ID_NONE;
    pInfo->nCompressLevel = XCODEC_NOT_SET;
    pInfo->nFrameSize = XCODEC_NOT_SET;
    pInfo->nProfile = FF_PROFILE_UNKNOWN;
    pInfo->timeBase = XRATIONAL_NOT_SET;
    pInfo->nBitRate = XCODEC_NOT_SET;

    /* Video codec properties */
    pInfo->aspectRatio = XRATIONAL_NOT_SET;
    pInfo->scaleFmt = XSCALE_FMT_STRETCH;
    pInfo->pixFmt = AV_PIX_FMT_NONE;
    pInfo->nWidth = XCODEC_NOT_SET;
    pInfo->nHeight = XCODEC_NOT_SET;

    /* Codec extradata */
    pInfo->nExtraSize = XSTDNON;
    pInfo->pExtraData = NULL;

    /* Audio codec properties */
    pInfo->nBitsPerSample = XCODEC_NOT_SET;
    pInfo->nSampleRate = XCODEC_NOT_SET;
    pInfo->sampleFmt = AV_SAMPLE_FMT_NONE;
    pInfo->nChannels = XCODEC_NOT_SET;

#ifdef XCODEC_USE_NEW_CHANNEL
    av_channel_layout_default(&pInfo->channelLayout, 0);
    pInfo->channelLayout.nb_channels = XCODEC_NOT_SET;
#else
    pInfo->nChannelLayout = XCODEC_NOT_SET;
#endif
}

void XCodec_Clear(xcodec_t *pInfo)
{
    XASSERT_VOID(pInfo);
    pInfo->nExtraSize = XSTDNON;

    if (pInfo->pExtraData != NULL)
    {
        av_free(pInfo->pExtraData);
        pInfo->pExtraData = NULL;
    }
}

void XCodec_InitChannels(xcodec_t *pInfo, int nChannels)
{
    pInfo->nChannels = nChannels;

#ifdef XCODEC_USE_NEW_CHANNEL
    av_channel_layout_default(&pInfo->channelLayout, nChannels);
#else
    if (nChannels == 1) pInfo->nChannelLayout = AV_CH_LAYOUT_MONO;
    else pInfo->nChannelLayout = AV_CH_LAYOUT_STEREO;
#endif
}

void XCodec_CopyChannels(xcodec_t *pDst, const xcodec_t *pSrc)
{
#ifdef XCODEC_USE_NEW_CHANNEL
    av_channel_layout_copy(&pDst->channelLayout, &pSrc->channelLayout);
    pDst->nChannels = pDst->channelLayout.nb_channels;
#else
    pDst->nChannelLayout = pSrc->nChannelLayout;
    pDst->nChannels = pSrc->nChannels;
#endif
}

XSTATUS XCodec_Copy(xcodec_t *pDst, const xcodec_t *pSrc)
{
    XASSERT((pDst && pSrc), XSTDINV);
    pDst->mediaType = pSrc->mediaType;
    pDst->codecId = pSrc->codecId;
    pDst->nProfile = pSrc->nProfile;
    pDst->timeBase = pSrc->timeBase;
    pDst->nBitRate = pSrc->nBitRate;
    pDst->frameRate = pSrc->frameRate;
    pDst->nFrameSize = pSrc->nFrameSize;
    pDst->nCompressLevel = pSrc->nCompressLevel;

    /* Video codec properties */
    pDst->pixFmt = pSrc->pixFmt;
    pDst->aspectRatio = pSrc->aspectRatio;
    pDst->scaleFmt = pSrc->scaleFmt;
    pDst->nWidth = pSrc->nWidth;
    pDst->nHeight = pSrc->nHeight;

    /* Audio codec properties */
    pDst->sampleFmt = pSrc->sampleFmt;
    pDst->nSampleRate = pSrc->nSampleRate;
    pDst->nBitsPerSample = pSrc->nBitsPerSample;
    XCodec_CopyChannels(pDst, pSrc);

    /* Codec context extradata */
    uint8_t *pExtraData = pSrc->pExtraData;
    int nExtraSize = pSrc->nExtraSize;
    return XCodec_AddExtra(pDst, pExtraData, nExtraSize);
}

XSTATUS XCodec_ApplyToAVCodec(xcodec_t *pInfo, AVCodecContext *pCodecCtx)
{
    XASSERT((pInfo && pCodecCtx), XSTDINV);

    if (pInfo->mediaType == AVMEDIA_TYPE_VIDEO)
        return XCodec_ApplyVideoCodec(pInfo, pCodecCtx);
    else if (pInfo->mediaType == AVMEDIA_TYPE_AUDIO)
        return XCodec_ApplyAudioCodec(pInfo, pCodecCtx);

    return XSTDINV;
}

XSTATUS XCodec_ApplyToAVParam(xcodec_t *pInfo, AVCodecParameters *pCodecPar)
{
    XASSERT((pInfo && pCodecPar), XSTDINV);

    if (pInfo->mediaType == AVMEDIA_TYPE_VIDEO)
        return XCodec_ApplyVideoParam(pInfo, pCodecPar);
    else if (pInfo->mediaType == AVMEDIA_TYPE_AUDIO)
        return XCodec_ApplyAudioParam(pInfo, pCodecPar);

    return XSTDINV;
}

XSTATUS XCodec_GetFromAVCodec(xcodec_t *pInfo, AVCodecContext *pCodecCtx)
{
    XASSERT((pCodecCtx && pInfo), XSTDINV);
    pInfo->mediaType = pCodecCtx->codec_type;
    pInfo->nBitRate = pCodecCtx->bit_rate;
    pInfo->nProfile = pCodecCtx->profile;
    pInfo->codecId = pCodecCtx->codec_id;
    pInfo->timeBase = pCodecCtx->time_base;
    pInfo->nFrameSize = pCodecCtx->frame_size;
    pInfo->nCompressLevel = pCodecCtx->compression_level;

    if (pInfo->mediaType == AVMEDIA_TYPE_VIDEO)
    {
        /* Video codec properties */
        pInfo->aspectRatio = pCodecCtx->sample_aspect_ratio;
        pInfo->frameRate = pCodecCtx->framerate;
        pInfo->pixFmt = pCodecCtx->pix_fmt;
        pInfo->nWidth = pCodecCtx->width;
        pInfo->nHeight = pCodecCtx->height;
    }
    else if (pInfo->mediaType == AVMEDIA_TYPE_AUDIO)
    {
        /* Audio codec properties */
        pInfo->nSampleRate = pCodecCtx->sample_rate;
        pInfo->sampleFmt = pCodecCtx->sample_fmt;
        pInfo->nBitsPerSample = pCodecCtx->bits_per_coded_sample;

#ifdef XCODEC_USE_NEW_CHANNEL
        av_channel_layout_copy(&pInfo->channelLayout, &pCodecCtx->ch_layout);
        pInfo->nChannels = pInfo->channelLayout.nb_channels;
#else
        pInfo->nChannelLayout = pCodecCtx->channel_layout;
        pInfo->nChannels = pCodecCtx->channels;
#endif
    }

    return XSTDOK;
}

XSTATUS XCodec_GetFromAVStream(xcodec_t *pInfo, AVStream *pStream)
{
    XASSERT((pStream && pStream->codecpar && pInfo), XSTDINV);
    AVCodecParameters *pCodecPar = pStream->codecpar;

    pInfo->mediaType = pCodecPar->codec_type;
    pInfo->nBitRate = pCodecPar->bit_rate;
    pInfo->nProfile = pCodecPar->profile;
    pInfo->codecId = pCodecPar->codec_id;
    pInfo->nFrameSize = pCodecPar->frame_size;

    pInfo->timeBase = pStream->time_base;
    pInfo->nCompressLevel = XSTDINV;

    if (pInfo->mediaType == AVMEDIA_TYPE_VIDEO)
    {
        /* Video codec properties */
        pInfo->aspectRatio = pCodecPar->sample_aspect_ratio;
        pInfo->frameRate = pStream->avg_frame_rate;
        pInfo->pixFmt = pCodecPar->format;
        pInfo->nWidth = pCodecPar->width;
        pInfo->nHeight = pCodecPar->height;
    }
    else if (pInfo->mediaType == AVMEDIA_TYPE_AUDIO)
    {
        /* Audio codec properties */
        pInfo->nSampleRate = pCodecPar->sample_rate;
        pInfo->sampleFmt = pCodecPar->format;
        pInfo->nBitsPerSample = pCodecPar->bits_per_coded_sample;

#ifdef XCODEC_USE_NEW_CHANNEL
        av_channel_layout_copy(&pInfo->channelLayout, &pCodecPar->ch_layout);
        pInfo->nChannels = pInfo->channelLayout.nb_channels;
#else
        pInfo->nChannelLayout = pCodecPar->channel_layout;
        pInfo->nChannels = pCodecPar->channels;
#endif 
    }

    if (pCodecPar->extradata != NULL &&
        pCodecPar->extradata_size > 0)
            XCodec_GetStreamExtra(pInfo, pStream);

    return XSTDOK;
}

XSTATUS XCodec_ApplyVideoCodec(xcodec_t *pInfo, AVCodecContext *pCodecCtx)
{
    XASSERT((pCodecCtx && pInfo), XSTDINV);
    pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;

    XCODEC_APPLY_RATIONAL(pCodecCtx->sample_aspect_ratio, pInfo->aspectRatio);
    XCODEC_APPLY_RATIONAL(pCodecCtx->framerate, pInfo->frameRate);
    XCODEC_APPLY_RATIONAL(pCodecCtx->time_base, pInfo->timeBase);

    XCODEC_APPLY_INTEGER(pCodecCtx->compression_level, pInfo->nCompressLevel);
    XCODEC_APPLY_INTEGER(pCodecCtx->frame_size, pInfo->nFrameSize);
    XCODEC_APPLY_INTEGER(pCodecCtx->profile, pInfo->nProfile);

    XCODEC_APPLY_INTEGER(pCodecCtx->width, pInfo->nWidth);
    XCODEC_APPLY_INTEGER(pCodecCtx->height, pInfo->nHeight);

    if (pInfo->codecId != AV_CODEC_ID_NONE)
        pCodecCtx->codec_id = pInfo->codecId;

    if (pInfo->pixFmt != AV_PIX_FMT_NONE)
        pCodecCtx->pix_fmt = pInfo->pixFmt;

    return XSTDOK;
}

XSTATUS XCodec_ApplyAudioCodec(xcodec_t *pInfo, AVCodecContext *pCodecCtx)
{
    XASSERT((pCodecCtx && pInfo), XSTDINV);
    pCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;

    XCODEC_APPLY_INTEGER(pCodecCtx->bits_per_coded_sample, pInfo->nBitsPerSample);
    XCODEC_APPLY_INTEGER(pCodecCtx->compression_level, pInfo->nCompressLevel);
    XCODEC_APPLY_INTEGER(pCodecCtx->sample_rate, pInfo->nSampleRate);
    XCODEC_APPLY_INTEGER(pCodecCtx->frame_size, pInfo->nFrameSize);
    XCODEC_APPLY_INTEGER(pCodecCtx->bit_rate, pInfo->nBitRate);
    XCODEC_APPLY_INTEGER(pCodecCtx->profile, pInfo->nProfile);
    XCODEC_APPLY_RATIONAL(pCodecCtx->time_base, pInfo->timeBase);

    if (pInfo->sampleFmt != AV_SAMPLE_FMT_NONE)
        pCodecCtx->sample_fmt = pInfo->sampleFmt;

    if (pInfo->codecId != AV_CODEC_ID_NONE)
        pCodecCtx->codec_id = pInfo->codecId;

#ifdef XCODEC_USE_NEW_CHANNEL
    if (pInfo->channelLayout.nb_channels > 0)
        av_channel_layout_copy(&pCodecCtx->ch_layout, &pInfo->channelLayout);
#else
    XCODEC_APPLY_INTEGER(pCodecCtx->channel_layout, pInfo->nChannelLayout);
    XCODEC_APPLY_INTEGER(pCodecCtx->channels, pInfo->nChannels);
#endif 

    return XSTDOK;
}

XSTATUS XCodec_ApplyVideoParam(xcodec_t *pInfo, AVCodecParameters *pCodecPar)
{
    XASSERT((pCodecPar && pInfo), XSTDINV);
    pCodecPar->codec_type = AVMEDIA_TYPE_VIDEO;

    XCODEC_APPLY_RATIONAL(pCodecPar->sample_aspect_ratio, pInfo->aspectRatio);
    XCODEC_APPLY_INTEGER(pCodecPar->frame_size, pInfo->nFrameSize);
    XCODEC_APPLY_INTEGER(pCodecPar->profile, pInfo->nProfile);
    XCODEC_APPLY_INTEGER(pCodecPar->width, pInfo->nWidth);
    XCODEC_APPLY_INTEGER(pCodecPar->height, pInfo->nHeight);

    if (pInfo->codecId != AV_CODEC_ID_NONE)
        pCodecPar->codec_id = pInfo->codecId;

    if (pInfo->pixFmt != AV_PIX_FMT_NONE)
        pCodecPar->format = pInfo->pixFmt;

    return XSTDOK;
}

XSTATUS XCodec_ApplyAudioParam(xcodec_t *pInfo, AVCodecParameters *pCodecPar)
{
    XASSERT((pCodecPar && pInfo), XSTDINV);
    pCodecPar->codec_type = AVMEDIA_TYPE_AUDIO;

    XCODEC_APPLY_INTEGER(pCodecPar->bits_per_coded_sample, pInfo->nBitsPerSample);
    XCODEC_APPLY_INTEGER(pCodecPar->sample_rate, pInfo->nSampleRate);
    XCODEC_APPLY_INTEGER(pCodecPar->frame_size, pInfo->nFrameSize);
    XCODEC_APPLY_INTEGER(pCodecPar->bit_rate, pInfo->nBitRate);
    XCODEC_APPLY_INTEGER(pCodecPar->profile, pInfo->nProfile);

    if (pInfo->sampleFmt != AV_SAMPLE_FMT_NONE)
        pCodecPar->format = pInfo->sampleFmt;

    if (pInfo->codecId != AV_CODEC_ID_NONE)
        pCodecPar->codec_id = pInfo->codecId;

#ifdef XCODEC_USE_NEW_CHANNEL
    if (pInfo->channelLayout.nb_channels > 0)
        av_channel_layout_copy(&pCodecPar->ch_layout, &pInfo->channelLayout);
#else
    XCODEC_APPLY_INTEGER(pCodecPar->channel_layout, pInfo->nChannelLayout);
    XCODEC_APPLY_INTEGER(pCodecPar->channels, pInfo->nChannels);
#endif

    return XSTDOK;
}

XSTATUS XCodec_AddExtra(xcodec_t *pInfo, uint8_t *pExtraData, int nSize)
{
    XASSERT((pInfo != NULL), XSTDINV);
    XCodec_Clear(pInfo);

    XASSERT_RET((pExtraData && nSize > 0), XSTDNON);
    int nPaddingSize = AV_INPUT_BUFFER_PADDING_SIZE;

    pInfo->pExtraData = av_mallocz(nSize + nPaddingSize);
    XASSERT((pInfo->pExtraData != NULL), XSTDERR);

    memcpy(pInfo->pExtraData, pExtraData, nSize);
    pInfo->nExtraSize = nSize;

    return XSTDOK;
}

XSTATUS XCodec_GetStreamExtra(xcodec_t *pInfo, AVStream *pStream)
{
    XASSERT((pInfo && pStream && pStream->codecpar), XSTDINV);
    uint8_t *pExtraData = pStream->codecpar->extradata;
    int nExtraSize = pStream->codecpar->extradata_size;
    return XCodec_AddExtra(pInfo, pExtraData, nExtraSize);
}

XSTATUS XCodec_ApplyStreamExtra(xcodec_t *pInfo, AVStream *pStream)
{
    XASSERT((pInfo != NULL && pStream != NULL), XSTDINV);
    XASSERT((pInfo->pExtraData && pInfo->nExtraSize > 0), XSTDNON);

    AVCodecParameters *pCodecPar = pStream->codecpar;
    int nPaddingSize = AV_INPUT_BUFFER_PADDING_SIZE;

    /* Store extra data in AVStream codec parameters */
    pCodecPar->extradata = av_mallocz(pInfo->nExtraSize + nPaddingSize);
    memcpy(pCodecPar->extradata, pInfo->pExtraData, pInfo->nExtraSize);
    pCodecPar->extradata_size = pInfo->nExtraSize;

    return XSTDOK;
}

XSTATUS XCodec_ApplyToAVStream(xcodec_t *pInfo, AVStream *pStream)
{
    XASSERT((pInfo && pStream), XSTDINV);
    AVCodecParameters *pCodecPar = pStream->codecpar;
    pStream->time_base = pInfo->timeBase;

    if (pInfo->mediaType == AVMEDIA_TYPE_VIDEO)
        XCodec_ApplyVideoParam(pInfo, pCodecPar);
    else if (pInfo->mediaType == AVMEDIA_TYPE_AUDIO)
        XCodec_ApplyAudioParam(pInfo, pCodecPar);

    if (pInfo->pExtraData &&
        pInfo->nExtraSize > 0)
        XCodec_ApplyStreamExtra(pInfo, pStream);

    return XSTDOK;
}

enum AVMediaType XCodec_GetMediaType(const char *pMediaTypeStr)
{
    if (!strncmp(pMediaTypeStr, "audio", 5))
        return AVMEDIA_TYPE_AUDIO;
    else if (!strncmp(pMediaTypeStr, "video", 5))
        return AVMEDIA_TYPE_VIDEO;
    else if (!strncmp(pMediaTypeStr, "subtitle", 8))
        return AVMEDIA_TYPE_SUBTITLE;
    else if (!strncmp(pMediaTypeStr, "data", 4))
        return AVMEDIA_TYPE_DATA;
    else if (!strncmp(pMediaTypeStr, "attachment", 10))
        return AVMEDIA_TYPE_ATTACHMENT;

    return AVMEDIA_TYPE_UNKNOWN;
}

enum AVCodecID XCodec_GetIDByName(const char *pCodecName)
{
    const AVCodecDescriptor *pDesc = avcodec_descriptor_get_by_name(pCodecName);
    XASSERT(pDesc, AV_CODEC_ID_NONE);
    return pDesc->id;
}

char* XCodec_GetNameByID(char *pName, size_t nLength, enum AVCodecID codecId)
{
    const AVCodecDescriptor *pDesc = avcodec_descriptor_get(codecId);
    if (pDesc == NULL) xstrncpy(pName, nLength, "none");
    else xstrncpy(pName, nLength, pDesc->name);
    return pName;
}

char* XCodec_GetScaleFmtStr(char *pOutput, size_t nLength, xscale_fmt_t eScaleFmt)
{
    XASSERT_RET((pOutput && nLength), NULL);
    if (eScaleFmt == XSCALE_FMT_STRETCH) xstrncpy(pOutput, nLength, "stretch");
    else if (eScaleFmt == XSCALE_FMT_ASPECT) xstrncpy(pOutput, nLength, "aspect");
    else xstrncpy(pOutput, nLength, "none");;
    return pOutput;
}

xscale_fmt_t XCodec_GetScaleFmt(const char *pScaleFmt)
{
    XASSERT_RET(pScaleFmt, XSCALE_FMT_NONE);
    if (!strncmp(pScaleFmt, "stretch", 7)) return XSCALE_FMT_STRETCH;
    else if (!strncmp(pScaleFmt, "aspect", 6)) return XSCALE_FMT_ASPECT;
    return XSCALE_FMT_NONE;
}

size_t XCodec_DumpStr(xcodec_t *pCodec, char *pOutput, size_t nSize)
{
    XASSERT((pCodec && pOutput && nSize), XSTDINV);
    char sMediaSpecific[XSTR_TINY];
    char sCodecIdStr[XSTR_TINY];

    sMediaSpecific[0] = '\0';
    sCodecIdStr[0] = '\0';

    XCodec_GetNameByID(sCodecIdStr, sizeof(sCodecIdStr), pCodec->codecId);
    const char *pMediaType = av_get_media_type_string(pCodec->mediaType);

    if (pCodec->mediaType == AVMEDIA_TYPE_AUDIO)
    {
        const char *pFormat = av_get_sample_fmt_name(pCodec->sampleFmt);
        const char *pSampleFormat = pFormat != NULL ? pFormat : "unknown";

        xstrncpyf(sMediaSpecific, sizeof(sMediaSpecific),
            "fmt(%s), chan(%d), sr(%d), bps(%d)",
            pSampleFormat, pCodec->nChannels,
            pCodec->nSampleRate, pCodec->nBitsPerSample);
    }
    else if (pCodec->mediaType == AVMEDIA_TYPE_VIDEO)
    {
        const char *pFormat = av_get_pix_fmt_name(pCodec->pixFmt);
        const char *pPixFmt = pFormat != NULL ? pFormat : "unknown";

        xstrncpyf(sMediaSpecific, sizeof(sMediaSpecific),
            "fmt(%s), size(%dx%d), ar(%d:%d), fr(%d.%d)",
            pPixFmt, pCodec->nWidth, pCodec->nHeight,
            pCodec->aspectRatio.num, pCodec->aspectRatio.den,
            pCodec->frameRate.num, pCodec->frameRate.den);
    }

    return xstrncpyf(pOutput, nSize,
        "type(%s), codec(%s), %s, tb(%d.%d)",
        pMediaType, sCodecIdStr, sMediaSpecific,
        pCodec->timeBase.num, pCodec->timeBase.den);
}

size_t XCodec_DumpJSON(xcodec_t *pCodec, char *pOutput, size_t nSize, size_t nTabSize, xbool_t bPretty)
{
    XASSERT((pCodec && pOutput && nSize), XSTDINV);
    xjson_obj_t *pCodecObj = XJSON_NewObject(NULL, NULL, XFALSE);
    XASSERT((pCodecObj != NULL), XSTDERR);

    const char *pMediaType = av_get_media_type_string(pCodec->mediaType);
    const char *pType = pMediaType != NULL ? pMediaType : "unknown";

    XJSON_AddObject(pCodecObj, XJSON_NewString(NULL, "mediaType", pType));

    char sCodecIdStr[XSTR_TINY];
    XCodec_GetNameByID(sCodecIdStr, sizeof(sCodecIdStr), pCodec->codecId);
    XJSON_AddObject(pCodecObj, XJSON_NewString(NULL, "codecId", sCodecIdStr));

    xjson_obj_t *pRationalObj = XJSON_NewArray(NULL, "timeBase", XFALSE);
    if (pRationalObj != NULL)
    {
        XJSON_AddObject(pRationalObj, XJSON_NewInt(NULL, NULL, pCodec->timeBase.num));
        XJSON_AddObject(pRationalObj, XJSON_NewInt(NULL, NULL, pCodec->timeBase.den));

        pRationalObj->nAllowLinter = XFALSE;
        XJSON_AddObject(pCodecObj, pRationalObj);
    }

    XJSON_AddObject(pCodecObj, XJSON_NewInt(NULL, "compressLevel", pCodec->nCompressLevel));
    XJSON_AddObject(pCodecObj, XJSON_NewInt(NULL, "frameSize", pCodec->nFrameSize));
    XJSON_AddObject(pCodecObj, XJSON_NewInt(NULL, "bitRate", pCodec->nBitRate));
    XJSON_AddObject(pCodecObj, XJSON_NewInt(NULL, "profile", pCodec->nProfile));

    if (pCodec->mediaType == AVMEDIA_TYPE_AUDIO)
    {
        const char *pSampleFormat = av_get_sample_fmt_name(pCodec->sampleFmt);
        const char *pFormat = pSampleFormat != NULL ? pSampleFormat : "unknown";

        XJSON_AddObject(pCodecObj, XJSON_NewString(NULL, "sampleFmt", pFormat));
        XJSON_AddObject(pCodecObj, XJSON_NewInt(NULL, "bitsPerSample", pCodec->nBitsPerSample));
        XJSON_AddObject(pCodecObj, XJSON_NewInt(NULL, "sampleRate", pCodec->nSampleRate));
        XJSON_AddObject(pCodecObj, XJSON_NewInt(NULL, "channels", pCodec->nChannels));
    }
    else if (pCodec->mediaType == AVMEDIA_TYPE_VIDEO)
    {
        char sScaleFmt[XSTR_TINY];
        XCodec_GetScaleFmtStr(sScaleFmt, sizeof(sScaleFmt), pCodec->scaleFmt);

        const char *pPixelFormat = av_get_pix_fmt_name(pCodec->pixFmt);
        const char *pFormat = pPixelFormat != NULL ? pPixelFormat : "unknown";

        XJSON_AddObject(pCodecObj, XJSON_NewString(NULL, "scaleFmt", sScaleFmt));
        XJSON_AddObject(pCodecObj, XJSON_NewString(NULL, "pixFmt", pFormat));

        pRationalObj = XJSON_NewArray(NULL, "aspectRatio", XFALSE);
        if (pRationalObj != NULL)
        {
            XJSON_AddObject(pRationalObj, XJSON_NewInt(NULL, NULL, pCodec->aspectRatio.num));
            XJSON_AddObject(pRationalObj, XJSON_NewInt(NULL, NULL, pCodec->aspectRatio.den));

            pRationalObj->nAllowLinter = XFALSE;
            XJSON_AddObject(pCodecObj, pRationalObj);
        }

        pRationalObj = XJSON_NewArray(NULL, "frameRate", XFALSE);
        if (pRationalObj != NULL)
        {
            XJSON_AddObject(pRationalObj, XJSON_NewInt(NULL, NULL, pCodec->frameRate.num));
            XJSON_AddObject(pRationalObj, XJSON_NewInt(NULL, NULL, pCodec->frameRate.den));

            pRationalObj->nAllowLinter = XFALSE;
            XJSON_AddObject(pCodecObj, pRationalObj);
        }

        pRationalObj = XJSON_NewArray(NULL, "size", XFALSE);
        if (pRationalObj != NULL)
        {
            XJSON_AddObject(pRationalObj, XJSON_NewInt(NULL, NULL, pCodec->nWidth));
            XJSON_AddObject(pRationalObj, XJSON_NewInt(NULL, NULL, pCodec->nHeight));

            pRationalObj->nAllowLinter = XFALSE;
            XJSON_AddObject(pCodecObj, pRationalObj);
        }
    }

    xjson_writer_t writer;
    XJSON_InitWriter(&writer, NULL, pOutput, nSize);

    writer.nTabSize = nTabSize;
    writer.nPretty = bPretty;

    XJSON_WriteObject(pCodecObj, &writer);
    XJSON_DestroyWriter(&writer);
    XJSON_FreeObject(pCodecObj);

    return writer.nLength;
}

XSTATUS XCodec_FromJSON(xcodec_t *pCodec, char* pJson, size_t nLength)
{
    XASSERT((pCodec && pJson && nLength), XSTDINV);
    XCodec_Init(pCodec);

    xjson_t json;
    if (!XJSON_Parse(&json, NULL, pJson, nLength))
    {
        XJSON_Destroy(&json);
        return XSTDERR;
    }

    xjson_obj_t *pRootObj = json.pRootObj;
    xjson_obj_t *pRationalObj = NULL;
    xjson_obj_t *pChildObj = NULL;

    const char *pMediaType = XJSON_GetString(XJSON_GetObject(pRootObj, "mediaType"));
    if (xstrused(pMediaType)) pCodec->mediaType = XCodec_GetMediaType(pMediaType);

    const char *pCodecId = XJSON_GetString(XJSON_GetObject(pRootObj, "codecId"));
    if (xstrused(pCodecId)) pCodec->codecId = XCodec_GetIDByName(pCodecId);

    pRationalObj = XJSON_GetObject(pRootObj, "timeBase");
    if (pRationalObj != NULL && XJSON_GetArrayLength(pRationalObj) >= 2)
    {
        pChildObj = XJSON_GetArrayItem(pRationalObj, 0);
        if (pChildObj != NULL) pCodec->timeBase.num = XJSON_GetInt(pChildObj);

        pChildObj = XJSON_GetArrayItem(pRationalObj, 1);
        if (pChildObj != NULL) pCodec->timeBase.den = XJSON_GetInt(pChildObj);
    }

    pChildObj = XJSON_GetObject(pRootObj, "bitRate");
    if (pChildObj != NULL) pCodec->nBitRate = XJSON_GetInt(pChildObj);

    pChildObj = XJSON_GetObject(pRootObj, "compressLevel");
    if (pChildObj != NULL) pCodec->nCompressLevel = XJSON_GetInt(pChildObj);

    pChildObj = XJSON_GetObject(pRootObj, "frameSize");
    if (pChildObj != NULL) pCodec->nFrameSize = XJSON_GetInt(pChildObj);

    pChildObj = XJSON_GetObject(pRootObj, "profile");
    if (pChildObj != NULL) pCodec->nProfile = XJSON_GetInt(pChildObj);

    if (pCodec->mediaType == AVMEDIA_TYPE_AUDIO)
    {
        const char *pSampleFmt = XJSON_GetString(XJSON_GetObject(pRootObj, "sampleFmt"));
        if (xstrused(pSampleFmt)) pCodec->sampleFmt = av_get_sample_fmt(pSampleFmt);

        pChildObj = XJSON_GetObject(pRootObj, "bitsPerSample");
        if (pChildObj != NULL) pCodec->nBitsPerSample = XJSON_GetInt(pChildObj);

        pChildObj = XJSON_GetObject(pRootObj, "sampleRate");
        if (pChildObj != NULL) pCodec->nSampleRate = XJSON_GetInt(pChildObj);

        pChildObj = XJSON_GetObject(pRootObj, "channels");
        if (pChildObj != NULL)
        {
            int nChannels = XJSON_GetInt(pChildObj);
            XCodec_InitChannels(pCodec, nChannels);
        }
    }
    else if (pCodec->mediaType == AVMEDIA_TYPE_VIDEO)
    {
        const char *pPixFmt = XJSON_GetString(XJSON_GetObject(pRootObj, "pixFmt"));
        if (xstrused(pPixFmt)) pCodec->pixFmt = av_get_pix_fmt(pPixFmt);

        const char *pScaleFmt = XJSON_GetString(XJSON_GetObject(pRootObj, "scaleFmt"));
        if (xstrused(pScaleFmt)) pCodec->scaleFmt = XCodec_GetScaleFmt(pScaleFmt);

        pRationalObj = XJSON_GetObject(pRootObj, "aspectRatio");
        if (pRationalObj != NULL && XJSON_GetArrayLength(pRationalObj) >= 2)
        {
            pChildObj = XJSON_GetArrayItem(pRationalObj, 0);
            if (pChildObj != NULL) pCodec->aspectRatio.num = XJSON_GetInt(pChildObj);

            pChildObj = XJSON_GetArrayItem(pRationalObj, 1);
            if (pChildObj != NULL) pCodec->aspectRatio.den = XJSON_GetInt(pChildObj);
        }

        pRationalObj = XJSON_GetObject(pRootObj, "frameRate");
        if (pRationalObj != NULL && XJSON_GetArrayLength(pRationalObj) >= 2)
        {
            pChildObj = XJSON_GetArrayItem(pRationalObj, 0);
            if (pChildObj != NULL) pCodec->frameRate.num = XJSON_GetInt(pChildObj);

            pChildObj = XJSON_GetArrayItem(pRationalObj, 1);
            if (pChildObj != NULL) pCodec->frameRate.den = XJSON_GetInt(pChildObj);
        }

        pRationalObj = XJSON_GetObject(pRootObj, "size");
        if (pRationalObj != NULL && XJSON_GetArrayLength(pRationalObj) >= 2)
        {
            pChildObj = XJSON_GetArrayItem(pRationalObj, 0);
            if (pChildObj != NULL) pCodec->nWidth = XJSON_GetInt(pChildObj);

            pChildObj = XJSON_GetArrayItem(pRationalObj, 1);
            if (pChildObj != NULL) pCodec->nHeight = XJSON_GetInt(pChildObj);
        }
    }

    XJSON_Destroy(&json);
    return XSTDOK;
}