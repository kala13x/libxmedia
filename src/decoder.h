/*!
 *  @file libxmedia/src/decoder.h
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the video/audio decoding
 * functionality based on the FFMPEG API and libraries.
 */

#ifndef __XMEDIA_DECODER_H__
#define __XMEDIA_DECODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdinc.h"
#include "stream.h"
#include "status.h"

typedef int(*xdecoder_pkt_cb_t)(void *pUserCtx, AVFrame *pFrame, int nStreamIndex);
typedef void(*xdecoder_stat_cb_t)(void *pUserCtx, const char *pStatus);
typedef void(*xdecoder_err_cb_t)(void *pUserCtx, const char *pErrStr);

typedef struct xdecoder_ {
    /* Decoder/demuxer context */
    AVFormatContext*    pFmtCtx;
    AVDictionary*       pDemuxOpts;
    xarray_t            streams;

    /* User input context */
    xbool_t             bDemuxOnly;
    xdecoder_stat_cb_t  statusCallback;
    xdecoder_err_cb_t   errorCallback;
    xdecoder_pkt_cb_t   frameCallback;
    void*               pUserCtx;

    /* Status related context */
    xbool_t             bHaveInput;
    xstatus_t           status;
} xdecoder_t;

void XDecoder_Init(xdecoder_t *pDecoder);
void XDecoder_Destroy(xdecoder_t *pDecoder);

XSTATUS XDecoder_OpenInput(xdecoder_t *pDecoder, const char *pInput, const char *pInputFmt);
XSTATUS XDecoder_OpenCodec(xdecoder_t *pDecoder, xcodec_t *pCodec);

const xcodec_t* XDecoder_GetCodecInfo(xdecoder_t* pDecoder, int nStream);
XSTATUS XDecoder_CopyCodecInfo(xdecoder_t* pDecoder, xcodec_t *pCodecInfo, int nStream);
XSTATUS XDecoder_Seek(xdecoder_t *pDecoder, int nStream, int64_t nTS, int nFlags);

AVPacket* XDecoder_CreatePacket(xdecoder_t *pDecoder, uint8_t *pData, size_t nSize);
XSTATUS XDecoder_ReadPacket(xdecoder_t *pDecoder, AVPacket *pPacket);
XSTATUS XDecoder_DecodePacket(xdecoder_t *pDecoder, AVPacket *pPacket);

#ifdef __cplusplus
}
#endif

#endif /* __XMEDIA_DECODER_H__ */
