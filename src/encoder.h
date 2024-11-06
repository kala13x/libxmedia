/*!
 *  @file libxmedia/src/encoder.h
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the video/audio encoding
 * functionality based on the FFMPEG API and libraries.
 */

#ifndef __XMEDIA_ENCODER_H__
#define __XMEDIA_ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdinc.h"
#include "status.h"
#include "stream.h"
#include "frame.h"
#include "meta.h"

typedef void(*xencoder_stat_cb_t)(void *pUserCtx, const char *pStatus);
typedef void(*xencoder_err_cb_t)(void *pUserCtx, const char *pErrStr);
typedef int(*xencoder_pkt_cb_t)(void *pUserCtx, AVPacket *pPacket);

#if XMEDIA_AVCODEC_VER_AT_LEAST(5, 7)
typedef int(*xmuxer_cb_t)(void *pUserCtx, const uint8_t *pData, int nSize);
#else
typedef int(*xmuxer_cb_t)(void *pUserCtx, uint8_t *pData, int nSize);
#endif

#define XENCODER_IO_SIZE  (1024 * 64)

typedef enum {
    XPTS_CALCULATE,     // Calculate timestamps based on the elapsed time and clock rate
    XPTS_COMPUTE,       // Compute timestamps based on the sample rate and time base
    XPTS_RESCALE,       // Rescale original timestamps using av_packet_rescale_ts()
    XPTS_ROUND,         // Rescale original timestamps and round to the nearest value
    XPTS_SOURCE,        // Use original timestamps from the source stream
    XPTS_INVALID        // Invalid PTS/DTS calculation type
} xpts_ctl_t;

typedef struct xencoder_ {
    /* Encoder/muxer context */
    AVFormatContext*    pFmtCtx;
    AVIOContext*        pIOCtx;
    xarray_t            streams;

    /* IO buffer context */
    char                sOutputPath[XPATH_MAX];
    char                sOutFormat[XSTR_TINY];
    size_t              nIOBuffSize;
    uint8_t*            pIOBuffer;

    /* User input flags and params */
    xencoder_stat_cb_t  statusCallback;
    xencoder_err_cb_t   errorCallback;
    xencoder_pkt_cb_t   packetCallback;
    xmuxer_cb_t         muxerCallback;
    xbool_t             bMuxOnly;
    FILE*               pOutFile;
    void*               pUserCtx;

    /* Timestamp calculation stuff */
    xpts_ctl_t          eTSType;
    uint64_t            nStartTime;
    int                 nTSFix;

    /* FFMPEG status flag */
    xbool_t             bOutputOpen;
    xstatus_t           status;
} xencoder_t;

void XEncoder_Init(xencoder_t *pEncoder);
void XEncoder_Destroy(xencoder_t *pEncoder);

XSTATUS XEncoder_OpenFormat(xencoder_t *pEncoder, const char *pFormat, const char *pOutputUrl);
XSTATUS XEncoder_GuessFormat(xencoder_t *pEncoder, const char *pFormat, const char *pOutputUrl);
XSTATUS XEncoder_OpenStream(xencoder_t *pEncoder, xcodec_t *pCodecInfo);
XSTATUS XEncoder_OpenOutput(xencoder_t *pEncoder, AVDictionary *pOpts);

XSTATUS XEncoder_RestartCodec(xencoder_t *pEncoder, int nStreamIndex);
XSTATUS XEncoder_FlushStream(xencoder_t *pEncoder, int nStreamIndex);
XSTATUS XEncoder_FlushBuffer(xencoder_t *pEncoder, int nStreamIndex);
XSTATUS XEncoder_FlushStreams(xencoder_t *pEncoder);
XSTATUS XEncoder_FlushBuffers(xencoder_t *pEncoder);

const xcodec_t* XEncoder_GetCodecInfo(xencoder_t* pEncoder, int nStreamIndex);
XSTATUS XEncoder_CopyCodecInfo(xencoder_t* pEncoder, xcodec_t *pCodecInfo, int nStreamIndex);
XSTATUS XEncoder_RescaleTS(xencoder_t *pEncoder, AVPacket *pPacket, xstream_t *pStream);
XSTATUS XEncoder_FixTS(xencoder_t *pEncoder, AVPacket *pPacket, xstream_t *pStream);

XSTATUS XEncoder_WriteFrame3(xencoder_t *pEncoder, AVFrame *pFrame, int nStreamIndex);
XSTATUS XEncoder_WriteFrame2(xencoder_t *pEncoder, AVFrame *pFrame, xframe_params_t *pParams);
XSTATUS XEncoder_WriteFrame(xencoder_t *pEncoder, AVFrame *pFrame, int nStreamIndex);
XSTATUS XEncoder_WriteHeader(xencoder_t *pEncoder, AVDictionary *pHeaderOpts);
XSTATUS XEncoder_WritePacket(xencoder_t *pEncoder, AVPacket *pPacket);
XSTATUS XEncoder_FinishWrite(xencoder_t *pEncoder, xbool_t bFlush);

XSTATUS XEncoder_AddMeta(xencoder_t *pEncoder, xmeta_t *pMeta);
XSTATUS XEncoder_AddChapter(xencoder_t *pEncoder, AVChapter *pChapter);
XSTATUS XEncoder_AddChapters(xencoder_t *pEncoder, xarray_t *pChapters);

#ifdef __cplusplus
}
#endif

#endif /* __XMEDIA_ENCODER_H__ */
