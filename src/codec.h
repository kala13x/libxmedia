/*!
 *  @file libxmedia/src/codec.h
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the video/audio codec
 * functionality based on the FFMPEG API and libraries.
 */

#ifndef __XMEDIA_CODEC_H__
#define __XMEDIA_CODEC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdinc.h"
#include "frame.h"

#define XCODEC_NOT_SET      XSTDINV
#define XRATIONAL_NOT_SET   (AVRational){XCODEC_NOT_SET, XCODEC_NOT_SET}

typedef struct xcodec_ {
    enum AVMediaType    mediaType;
    enum AVCodecID      codecId;
    AVRational          timeBase;
    int64_t             nBitRate;
    int                 nFrameSize;
    int                 nProfile;
    int                 nCompressLevel;

    /* Video codec properties */
    enum AVPixelFormat  pixFmt;
    xscale_fmt_t        scaleFmt;
    AVRational          aspectRatio;
    AVRational          frameRate;
    int                 nWidth;
    int                 nHeight;

    /* Audio codec properties */
    enum AVSampleFormat sampleFmt;
    int                 nSampleRate;
    int                 nChannels;
    int                 nBitsPerSample;

#ifdef XCODEC_USE_NEW_CHANNEL
    AVChannelLayout     channelLayout;
#else
    uint64_t            nChannelLayout;
#endif

    /* Codec context extra data */
    uint8_t*            pExtraData;
    int                 nExtraSize;
} xcodec_t;

typedef struct x264_extra_ {
    /* h264 extradata */
    int         nSPSSize;
    int         nPPSSize;
    uint8_t*    pSPS;
    uint8_t*    pPPS;
} x264_extra_t;

typedef struct xopus_header_ {
    uint8_t     nChannels;
    uint16_t    nPreSkip;
    uint32_t    nInputSampleRate;
    int16_t     nOutputGain;
    uint8_t     nChannelMappingFamily;
} xopus_header_t;

xscale_fmt_t XCodec_GetScaleFmt(const char *pScaleFmt);
char* XCodec_GetScaleFmtStr(char *pOutput, size_t nLength, xscale_fmt_t eScaleFmt);

enum AVCodecID XCodec_GetIDByName(const char *pCodecName);
char* XCodec_GetNameByID(char *pName, size_t nLength, enum AVCodecID codecId);

size_t XCodec_DumpJSON(xcodec_t *pCodec, char *pOutput, size_t nSize, size_t nTabSize, xbool_t bPretty);
size_t XCodec_DumpStr(xcodec_t *pCodec, char *pOutput, size_t nSize);
XSTATUS XCodec_FromJSON(xcodec_t *pCodec, char* pJson, size_t nLength);

uint8_t* X264_CreateExtra(x264_extra_t *pExtra, int *pExtraSize);
uint8_t* XOPUS_CreateExtra(xopus_header_t *pHeader, int *pExtraSize);

void XCodec_Init(xcodec_t *pInfo);
void XCodec_Clear(xcodec_t *pInfo);
XSTATUS XCodec_Copy(xcodec_t *pDst, const xcodec_t *pSrc);

void XCodec_InitChannels(xcodec_t *pInfo, int nChannels);
void XCodec_CopyChannels(xcodec_t *pDst, const xcodec_t *pSrc);

XSTATUS XCodec_ApplyVideoParam(xcodec_t *pInfo, AVCodecParameters *pCodecPar);
XSTATUS XCodec_ApplyAudioParam(xcodec_t *pInfo, AVCodecParameters *pCodecPar);

XSTATUS XCodec_ApplyVideoCodec(xcodec_t *pInfo, AVCodecContext *pCodecCtx);
XSTATUS XCodec_ApplyAudioCodec(xcodec_t *pInfo, AVCodecContext *pCodecCtx);

XSTATUS XCodec_AddExtra(xcodec_t *pInfo, uint8_t *pExtraData, int nSize);
XSTATUS XCodec_GetStreamExtra(xcodec_t *pInfo, AVStream *pStream);
XSTATUS XCodec_ApplyStreamExtra(xcodec_t *pInfo, AVStream *pStream);

XSTATUS XCodec_GetFromAVStream(xcodec_t *pInfo, AVStream *pStream);
XSTATUS XCodec_ApplyToAVStream(xcodec_t *pInfo, AVStream *pStream);

XSTATUS XCodec_GetFromAVCodec(xcodec_t *pInfo, AVCodecContext *pCodecCtx);
XSTATUS XCodec_ApplyToAVCodec(xcodec_t *pInfo, AVCodecContext *pCodecCtx);

#ifdef __cplusplus
}
#endif

#endif /* __XMEDIA_CODEC_H__ */
