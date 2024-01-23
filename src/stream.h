/*!
 *  @file libxmedia/src/stream.h
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the video/audio stream
 * functionality based on the FFMPEG API and libraries.
 */

#ifndef __XMEDIA_STREAM_H__
#define __XMEDIA_STREAM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdinc.h"
#include "codec.h"

typedef struct xstream_ {
    xcodec_t            codecInfo;
    xbool_t             bCodecOpen;

    AVCodecContext*     pCodecCtx;
    AVStream*           pAvStream;
    AVPacket*           pPacket;
    AVFrame*            pFrame;

    uint64_t            nPacketCount;
    size_t              nPacketSize;
    int64_t             nLastPTS;
    int64_t             nLastDTS;

    int                 nSrcIndex;
    int                 nDstIndex;
} xstream_t;

xstream_t* XStream_New();
void XStream_Destroy(xstream_t *pStream);
void XStream_Init(xstream_t *pStream);

AVPacket* XStream_GetOrCreatePacket(xstream_t *pStream);
AVFrame* XStream_GetOrCreateFrame(xstream_t *pStream);

const xcodec_t* XStream_GetCodecInfo(xstream_t *pStream);
XSTATUS XStream_CopyCodecInfo(xstream_t *pStream, xcodec_t *pInfo);
XSTATUS XStream_FlushBuffers(xstream_t *pStream);

void XStreams_Init(xarray_t *pStreams);
void XStreams_Destroy(xarray_t *pStreams);

xstream_t* XStreams_NewStream(xarray_t *pStreams);
size_t XStreams_GetCount(xarray_t *pStreams);

xstream_t* XStreams_GetByIndex(xarray_t *pStreams, int nIndex);
xstream_t* XStreams_GetBySrcIndex(xarray_t *pStreams, int nSrcIndex);
xstream_t* XStreams_GetByDstIndex(xarray_t *pStreams, int nDstIndex);

#ifdef __cplusplus
}
#endif

#endif /* __XMEDIA_STREAM_H__ */
