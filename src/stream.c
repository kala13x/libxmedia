/*!
 *  @file libxmedia/src/stream.c
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the video/audio stream
 * functionality based on the FFMPEG API and libraries.
 */

#include "stream.h"

void XStream_Init(xstream_t *pStream)
{
    XASSERT_VOID_RET(pStream);
    XCodec_Init(&pStream->codecInfo);

    pStream->pCodecCtx = NULL;
    pStream->pAvStream = NULL;
    pStream->pPacket = NULL;
    pStream->pFrame = NULL;

    pStream->nSrcIndex = XSTDERR;
    pStream->nDstIndex = XSTDERR;

    pStream->nPacketCount = 0;
    pStream->nPacketSize = 0;
    pStream->nLastPTS = 0;
    pStream->nLastDTS = 0;
}

xstream_t* XStream_New()
{
    xstream_t *pStream = (xstream_t*)malloc(sizeof(xstream_t));
    XASSERT(pStream, NULL);

    XStream_Init(pStream);
    return pStream;
}

void XStream_Destroy(xstream_t *pStream)
{
    XASSERT_VOID_RET(pStream);
    XCodec_Clear(&pStream->codecInfo);

    if (pStream->pCodecCtx != NULL)
    {
        avcodec_free_context(&pStream->pCodecCtx);
        pStream->pCodecCtx = NULL;
    }

    if (pStream->pPacket != NULL)
    {
        av_packet_free(&pStream->pPacket);
        pStream->pPacket = NULL;
    }

    if (pStream->pFrame != NULL)
    {
        av_frame_free(&pStream->pFrame);
        pStream->pFrame = NULL;
    }
}

AVPacket* XStream_GetOrCreatePacket(xstream_t *pStream)
{
    if (pStream->pPacket != NULL)
        av_packet_unref(pStream->pPacket);
    else pStream->pPacket = av_packet_alloc();

    return pStream->pPacket;
}

AVFrame* XStream_GetOrCreateFrame(xstream_t *pStream)
{
    if (pStream->pFrame != NULL)
        av_frame_unref(pStream->pFrame);
    else pStream->pFrame = av_frame_alloc();

    return pStream->pFrame;
}

void XStreams_ClearCb(xarray_data_t *pArrData)
{
    XASSERT_VOID_RET((pArrData && pArrData->pData));
    XStream_Destroy((xstream_t*)pArrData->pData);
    free(pArrData->pData);
}

xstream_t* XStreams_NewStream(xarray_t *pStreams)
{
    xstream_t *pStream = XStream_New();
    XASSERT(pStream, NULL);

    int nStatus = XArray_AddData(pStreams, pStream, XSTDNON);
    XASSERT_CALL((nStatus >= 0), free, pStream, NULL);

    return pStream;
}

xstream_t* XStreams_GetByIndex(xarray_t *pStreams, int nIndex)
{
    XASSERT(pStreams, NULL);
    return (xstream_t*)XArray_GetData(pStreams, nIndex);
}

xstream_t* XStreams_GetBySrcIndex(xarray_t *pStreams, int nSrcIndex)
{
    XASSERT(pStreams, NULL);
    size_t i, nCount = XArray_Used(pStreams);

    for (i = 0; i < nCount; i++)
    {
        xstream_t *pStream = (xstream_t*)XArray_GetData(pStreams, i);
        if (pStream && pStream->nSrcIndex == nSrcIndex) return pStream;
    }

    return NULL;
}

xstream_t* XStreams_GetByDstIndex(xarray_t *pStreams, int nDstIndex)
{
    XASSERT(pStreams, NULL);
    size_t i, nCount = XArray_Used(pStreams);

    for (i = 0; i < nCount; i++)
    {
        xstream_t *pStream = (xstream_t*)XArray_GetData(pStreams, i);
        if (pStream && pStream->nDstIndex == nDstIndex) return pStream;
    }

    return NULL;
}

const xcodec_t* XStream_GetCodecInfo(xstream_t *pStream)
{
    XASSERT(pStream, NULL);
    return &pStream->codecInfo;
}

XSTATUS XStream_CopyCodecInfo(xstream_t *pStream, xcodec_t *pInfo)
{
    XASSERT((pStream && pInfo), XSTDINV);
    return XCodec_Copy(pInfo, &pStream->codecInfo);
}

XSTATUS XStream_FlushBuffers(xstream_t *pStream)
{
    XASSERT((pStream && pStream->pCodecCtx), XSTDINV);
    XASSERT(pStream->bCodecOpen, XSTDNON);

    avcodec_flush_buffers(pStream->pCodecCtx);
    return XSTDOK;
}

size_t XStreams_GetCount(xarray_t *pStreams)
{
    XASSERT(pStreams, XSTDNON);
    return XArray_Used(pStreams);
}

void XStreams_Init(xarray_t *pStreams)
{
    XArray_Init(pStreams, NULL, XSTDNON, XFALSE);
    pStreams->clearCb = XStreams_ClearCb;
}

void XStreams_Destroy(xarray_t *pStreams)
{
    XASSERT_VOID_RET(pStreams);
    XArray_Destroy(pStreams);
}
