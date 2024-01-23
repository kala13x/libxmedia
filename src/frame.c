/*!
 *  @file libxmedia/src/frame.c
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the video/audio frame
 * functionality based on the FFMPEG API and libraries.
 */

#include "frame.h"
#include "codec.h"

#define XFRAME_SET_INT(dst, src) if (src >= 0) dst = src
#define XFRAME_SET_INT2(dst, src, src2) if (src >= 0) dst = src; else dst = src2

void XFrame_InitFrame(AVFrame* pFrame)
{
    bzero(pFrame, sizeof(AVFrame));
    av_frame_unref(pFrame);
}

void XFrame_InitChannels(AVFrame *pFrame, int nChannels)
{
#ifdef XCODEC_USE_NEW_CHANNEL
    av_channel_layout_default(&pFrame->ch_layout, nChannels);
#else
    if (nChannels == 1) pFrame->channel_layout = AV_CH_LAYOUT_MONO;
    else pFrame->channel_layout = AV_CH_LAYOUT_STEREO;
    pFrame->channels = nChannels;
#endif
}

int XFrame_GetChannelCount(AVFrame *pFrame)
{
#ifdef XCODEC_USE_NEW_CHANNEL
    return pFrame->ch_layout.nb_channels;
#else
    return pFrame->channels;
#endif
}

xscale_fmt_t XFrame_GetScaleFmt(const char* pFmtName)
{
    XASSERT_RET(pFmtName, XSCALE_FMT_NONE);

    if (!strncmp(pFmtName, "stretch", 7)) return XSCALE_FMT_STRETCH;
    else if (!strncmp(pFmtName, "aspect", 6)) return XSCALE_FMT_ASPECT;

    return XSCALE_FMT_NONE;
}

void XFrame_InitParams(xframe_params_t *pFrame, xframe_params_t *pParent)
{
    /* Audio parameters */
    pFrame->sampleFmt = AV_SAMPLE_FMT_NONE;
    pFrame->nSampleRate = XSTDERR;
    pFrame->nChannels = XSTDERR;

    /* Video parameters */
    pFrame->pixFmt = AV_PIX_FMT_NONE;
    pFrame->scaleFmt = XSCALE_FMT_NONE;
    pFrame->nWidth = XSTDERR;
    pFrame->nHeight = XSTDERR;

    pFrame->mediaType = AVMEDIA_TYPE_UNKNOWN;
    pFrame->nIndex = XSTDERR;
    pFrame->nPTS = XSTDERR;

    /* General parameters */
    if (pParent) XStat_InitFrom(&pFrame->status, &pParent->status);
    else XStat_Init(&pFrame->status, XSTDNON, NULL, NULL);
}

XSTATUS XFrame_Resample(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, XSTDINV);
    xstatus_t *pStatus = &pParams->status;

    XASSERT((pFrameOut != NULL && pFrameIn != NULL),
        XStat_ErrCb(pStatus, "Invalid resample frames: in(%p), out(%p)",
            (void*)pFrameIn, (void*)pFrameOut));

    XASSERT((pParams->sampleFmt != AV_SAMPLE_FMT_NONE),
        XStat_ErrCb(pStatus, "Invalid sample format: fmt(%d)",
            (int)pParams->sampleFmt));

    XASSERT((pParams->nSampleRate > 0 && pParams->nChannels > 0),
        XStat_ErrCb(pStatus, "Invalid sample rate or channels: sr(%d), ch(%d)",
            pParams->nSampleRate, pParams->nChannels));

    struct SwrContext *pSwrCtx = NULL;
    enum AVSampleFormat srcFmt = (enum AVSampleFormat)pFrameIn->format;

#ifdef XCODEC_USE_NEW_CHANNEL
    av_channel_layout_default(&pFrameOut->ch_layout, pParams->nChannels);

    pStatus->nAVStatus = swr_alloc_set_opts2(&pSwrCtx,
        &pFrameOut->ch_layout, pParams->sampleFmt, pParams->nSampleRate,
        &pFrameIn->ch_layout, srcFmt, pFrameIn->sample_rate, 0, NULL);

    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Failed to get or create SWR context"));
#else
    /* Get or create swr context with in/out frame parameters */
    pFrameOut->channel_layout = av_get_default_channel_layout(pParams->nChannels);
    pFrameOut->channels = pParams->nChannels;

    pSwrCtx = swr_alloc_set_opts(NULL,
        pFrameOut->channel_layout,
        pParams->sampleFmt, pParams->nSampleRate,
        av_get_default_channel_layout(pFrameIn->channels),
        srcFmt, pFrameIn->sample_rate, 0, NULL);

    XASSERT(pSwrCtx, XStat_ErrCb(pStatus, "Failed to get or create SWR context"));
#endif

    int64_t nSwrDelay = swr_get_delay(pSwrCtx, pFrameIn->sample_rate);
    pFrameOut->nb_samples = av_rescale_rnd(nSwrDelay + pFrameIn->nb_samples,
        pParams->nSampleRate, pFrameIn->sample_rate, AV_ROUND_UP);

    pFrameOut->format = pParams->sampleFmt;
    pFrameOut->sample_rate = pParams->nSampleRate;
    XFRAME_SET_INT2(pFrameOut->pts, pParams->nPTS, pFrameIn->pts);

    /* Initialize the resampling context */
    pStatus->nAVStatus = swr_init(pSwrCtx);
    XASSERT_CALL((pStatus->nAVStatus >= 0), swr_free, &pSwrCtx,
        XStat_ErrCb(pStatus, "Failed to initialize the SWR context"));

    /* Allocate AVFrame buffer */
    pStatus->nAVStatus = av_frame_get_buffer(pFrameOut, 0);
    XASSERT_CALL((pStatus->nAVStatus >= 0), swr_free, &pSwrCtx,
        XStat_ErrCb(pStatus, "Failed to get buffer for AVFrame"));

    XStat_DebugCb(pStatus, "Resampling frame: sample rate(%d -> %d), pts(%lld)",
                 pFrameIn->sample_rate, pParams->nSampleRate, pFrameOut->pts);

    /* Resample frames */
    pStatus->nAVStatus = swr_convert(pSwrCtx, pFrameOut->data, pFrameOut->nb_samples,
                            (const uint8_t **)pFrameIn->data, pFrameIn->nb_samples);

    XASSERT_CALL((pStatus->nAVStatus >= 0), swr_free, &pSwrCtx,
        XStat_ErrCb(pStatus, "SWR failed to resample AVFrame"));

    swr_free(&pSwrCtx);
    return XSTDOK;
}

AVFrame* XFrame_FromOpus(uint8_t *pOpusBuff, size_t nSize, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, NULL);
    xstatus_t *pStatus = &pParams->status;

    /* Validate input arguments */
    XASSERT((pOpusBuff && nSize), XStat_ErrPtr(pStatus, "Invalid frame buffer or size"));
    XASSERT((pParams->nSampleRate > 0), XStat_ErrPtr(pStatus, "Invalid frame sample rate"));
    XASSERT((pParams->nChannels > 0), XStat_ErrPtr(pStatus, "Invalid frame channel count"));

    /* Allocate AVFrame */
    AVFrame *pFrameOut = av_frame_alloc();
    XASSERT(pFrameOut, XStat_ErrPtr(pStatus, "Failed to alloc AVFrame"));

    /* Calculate the number of samples in the decoded buffer */
    enum AVSampleFormat sampleFmt = AV_SAMPLE_FMT_S16;
    int nSampleSize = av_get_bytes_per_sample(sampleFmt);
    int nNumSamples = nSize / (pParams->nChannels * nSampleSize);
    int nTotalSamples = nNumSamples * pParams->nChannels;

    XFrame_InitChannels(pFrameOut, pParams->nChannels);
    XFRAME_SET_INT(pFrameOut->pts, pParams->nPTS);
    pFrameOut->sample_rate = pParams->nSampleRate;
    pFrameOut->nb_samples = nNumSamples;
    pFrameOut->format = sampleFmt;

    pStatus->nAVStatus = av_samples_alloc(pFrameOut->data, pFrameOut->linesize,
                    pParams->nChannels, pFrameOut->nb_samples, sampleFmt, 0);

    XASSERT_CALL((pStatus->nAVStatus >= 0), av_frame_free, &pFrameOut,
        XStat_ErrPtr(pStatus, "Failed to alloc AVFrame sample buffer"));

    /* Copy Opus data from raw buffer to the AVFrame */
    uint8_t *pDstBuff = pFrameOut->data[0];
    memcpy(pDstBuff, pOpusBuff, nTotalSamples * nSampleSize);

    return pFrameOut;
}

AVFrame* XFrame_FromYUV(uint8_t *pYUVBuff, size_t nSize, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, NULL);
    xstatus_t *pStatus = &pParams->status;

    /* Validate input arguments */
    XASSERT((pYUVBuff && nSize), XStat_ErrPtr(pStatus, "Invalid YUV buffer or size"));
    XASSERT((pParams->nWidth > 0 && pParams->nHeight > 0),
        XStat_ErrPtr(pStatus, "Invalid frame resolution"));

    size_t nExpectedSize = pParams->nWidth * pParams->nHeight;
    nExpectedSize += (pParams->nWidth * pParams->nHeight / 4);
    nExpectedSize += (pParams->nWidth * pParams->nHeight / 4);

    XASSERT((nSize == nExpectedSize), XStat_ErrPtr(pStatus,
        "Invalid frame size: expected(%zu), have(%zu)", nExpectedSize, nSize));

    AVFrame *pFrameOut = av_frame_alloc();
    XASSERT(pFrameOut, XStat_ErrPtr(pStatus, "Failed to alloc AVFrame"));

    /* Setup frame parameters */
    XFRAME_SET_INT(pFrameOut->pts, pParams->nPTS);
    pParams->pixFmt = AV_PIX_FMT_YUV420P;
    pFrameOut->format = pParams->pixFmt;
    pFrameOut->width = pParams->nWidth;
    pFrameOut->height = pParams->nHeight;

    /* Allocate YUV frame buffer */    
    pStatus->nAVStatus = av_image_alloc(pFrameOut->data, pFrameOut->linesize,
                    pParams->nWidth, pParams->nHeight, pParams->pixFmt, 32);

    XASSERT_CALL((pStatus->nAVStatus >= 0), av_frame_free, &pFrameOut,
        XStat_ErrPtr(pStatus, "Failed to allocate AV image frame buffer"));

    const uint8_t *pSlices[4] = {0};
    int nLineSizes[4] = {0};

    /* Set pointers to the Y, U, and V planes of the source YUV buffer */
    pSlices[0] = pYUVBuff;
    pSlices[1] = pSlices[0] + pParams->nWidth * pParams->nHeight;
    pSlices[2] = pSlices[1] + (pParams->nWidth * pParams->nHeight) / 4;

    /* Set the linesizes for each plane in the source YUV buffer */
    nLineSizes[0] = pParams->nWidth;
    nLineSizes[1] = pParams->nWidth / 2;
    nLineSizes[2] = pParams->nWidth / 2;

    /* Copy the source YUV data into the allocated AVFrame buffer */
    av_image_copy(pFrameOut->data, pFrameOut->linesize,
                  pSlices, nLineSizes, pParams->pixFmt,
                  pParams->nWidth, pParams->nHeight);

    return pFrameOut;
}

XSTATUS XFrame_BlackYUV(AVFrame *pFrameOut, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, XSTDINV);
    xstatus_t *pStatus = &pParams->status;

    XASSERT((pFrameOut != NULL), XStat_ErrCb(pStatus, "Invalid YUV output frame argument"));
    XASSERT((pParams->nWidth && pParams->nHeight), XStat_ErrCb(pStatus, "Invalid YUV resolution"));

    XFRAME_SET_INT(pFrameOut->pts, pParams->nPTS);
    pParams->pixFmt = AV_PIX_FMT_YUV420P;
    pFrameOut->format = pParams->pixFmt;
    pFrameOut->width = pParams->nWidth;
    pFrameOut->height = pParams->nHeight;

    pStatus->nAVStatus = av_frame_get_buffer(pFrameOut, 0);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus,
        "Failed to allocate memory for AVFrame buffer"));

    pStatus->nAVStatus = av_frame_make_writable(pFrameOut);
    XASSERT_CALL((pStatus->nAVStatus >= 0), av_frame_unref, pFrameOut,
        XStat_ErrCb(pStatus, "Failed to make AVFrame writeable"));

    /* Fill Y plane with black (0x00) and U/V planes with neutral chroma value (0x80) */
    memset(pFrameOut->data[0], 0x00, pFrameOut->linesize[0] * pParams->nHeight);
    memset(pFrameOut->data[1], 0x80, pFrameOut->linesize[1] * (pParams->nHeight / 2));
    memset(pFrameOut->data[2], 0x80, pFrameOut->linesize[2] * (pParams->nHeight / 2));

    return XSTDOK;
}

XSTATUS XFrame_OverlayYUV(AVFrame *pDstFrame, AVFrame *pSrcFrame, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, XSTDINV);
    xstatus_t *pStatus = &pParams->status;

    XASSERT((pDstFrame && pSrcFrame), XStat_ErrCb(pStatus, "Invalid overlay src/dst frame arguments"));
    XASSERT((pSrcFrame->width && pSrcFrame->height), XStat_ErrCb(pStatus, "Invalid src frame resolution"));
    XASSERT((pDstFrame->width && pDstFrame->height), XStat_ErrCb(pStatus, "Invalid dst frame resolution"));

    XASSERT((pDstFrame->height >= pSrcFrame->height && pDstFrame->width >= pSrcFrame->width),
            XStat_ErrCb(pStatus, "Overlay src is bigger than dst: src(%dx%d), dst(%dx%d)",
            pSrcFrame->width, pSrcFrame->height, pDstFrame->width, pDstFrame->height));

    pParams->pixFmt = AV_PIX_FMT_YUV420P;
    pDstFrame->format = pParams->pixFmt;

    int i, nSrcWidth = pSrcFrame->width;
    int nSrcHeight = pSrcFrame->height;

    int nOffsetX = (pDstFrame->width - nSrcWidth) / 2;
    int nOffsetY = (pDstFrame->height - nSrcHeight) / 2;

    /* Copy Y (luma) plane */
    for (i = 0; i < nSrcHeight; i++)
    {
        int y = i + nOffsetY;
        memcpy(pDstFrame->data[0] + y * pDstFrame->linesize[0] + nOffsetX,
               pSrcFrame->data[0] + i * pSrcFrame->linesize[0], nSrcWidth);
    }

    /* Copy U and V (chroma) planes */
    nSrcWidth /= 2; nSrcHeight /= 2;
    nOffsetX /= 2; nOffsetY /= 2;

    for (i = 0; i < nSrcHeight; i++)
    {
        int y = i + nOffsetY;
        memcpy(pDstFrame->data[1] + y * pDstFrame->linesize[1] + nOffsetX,
               pSrcFrame->data[1] + i * pSrcFrame->linesize[1], nSrcWidth);

        memcpy(pDstFrame->data[2] + y * pDstFrame->linesize[2] + nOffsetX,
               pSrcFrame->data[2] + i * pSrcFrame->linesize[2], nSrcWidth);
    }

    return XSTDOK;
}

XSTATUS XFrame_Stretch(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, XSTDINV);
    xstatus_t *pStatus = &pParams->status;

    XASSERT((pFrameOut && pFrameIn), XStat_ErrCb(pStatus, "Invalid scale in/out frame arguments"));
    XASSERT((pParams->pixFmt != AV_PIX_FMT_NONE), XStat_ErrCb(pStatus, "Invalid pixel format"));
    XASSERT((pParams->nWidth && pParams->nHeight), XStat_ErrCb(pStatus, "Invalid scale resolution"));

    struct SwsContext *pSwsCtx = NULL;
    enum AVPixelFormat srcFmt = (enum AVPixelFormat)pFrameIn->format;

    /* Get or create sws context with in/out frame parameters */
    pSwsCtx = sws_getCachedContext(NULL, pFrameIn->width, pFrameIn->height,
                                   srcFmt, pParams->nWidth, pParams->nHeight,
                                   pParams->pixFmt, SWS_BICUBIC, NULL, NULL, NULL);
    XASSERT(pSwsCtx, XStat_ErrCb(pStatus, "Failed to get or create SWS context"));

    /* Setup out frame properties */
    XFRAME_SET_INT2(pFrameOut->pts, pParams->nPTS, pFrameIn->pts);
    pFrameOut->width = pParams->nWidth;
    pFrameOut->height = pParams->nHeight;
    pFrameOut->format = pParams->pixFmt;
    pFrameOut->pkt_dts = pFrameIn->pkt_dts;
    int nSrcSliceY = 0;

    /* Allocate AVFrame buffer */
    pStatus->nAVStatus = av_frame_get_buffer(pFrameOut, 0);
    XASSERT_CALL((pStatus->nAVStatus >= 0), sws_freeContext, pSwsCtx,
        XStat_ErrCb(pStatus, "Failed to get buffer for AVFrame"));

    XStat_DebugCb(pStatus, "Scaling frame: in(%dx%d), out(%dx%d), pts(%lld)",
        pFrameIn->width, pFrameIn->height, pParams->nWidth,
        pParams->nHeight, pFrameOut->pts);

    /* Scale destination frame */
    sws_scale(pSwsCtx, (const uint8_t* const*)pFrameIn->data, pFrameIn->linesize,
            nSrcSliceY, pFrameIn->height, pFrameOut->data, pFrameOut->linesize);

    sws_freeContext(pSwsCtx);
    return XSTDOK;
}

XSTATUS XFrame_Aspect(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, XSTDINV);
    xstatus_t *pStatus = &pParams->status;

    XASSERT((pFrameOut && pFrameIn), XStat_ErrCb(pStatus, "Invalid scale in/out frame arguments"));
    XASSERT((pParams->pixFmt != AV_PIX_FMT_NONE), XStat_ErrCb(pStatus, "Invalid pixel format"));
    XASSERT((pParams->nWidth && pParams->nHeight), XStat_ErrCb(pStatus, "Invalid scale resolution"));

    AVFrame scaledFrame;
    XFrame_InitFrame(&scaledFrame);

    xframe_params_t scaleParams;
    XFrame_InitParams(&scaleParams, pParams);

    /* Calculate the scaling factors for width and height */
    float widthScaleFactor = (float)pParams->nWidth / pFrameIn->width;
    float heightScaleFactor = (float)pParams->nHeight / pFrameIn->height;

    /* Choose the smaller scaling factor to fit both dimensions */
    float scaleFactor = (widthScaleFactor < heightScaleFactor) ?
                         widthScaleFactor : heightScaleFactor;

    /* Calculate the scaled width and height */
    scaleParams.nWidth = (int)(pFrameIn->width * scaleFactor);
    scaleParams.nHeight = (int)(pFrameIn->height * scaleFactor);
    scaleParams.pixFmt = pParams->pixFmt;
    scaleParams.nPTS = pParams->nPTS;

    if (scaleParams.nWidth == pParams->nWidth &&
        scaleParams.nHeight == pParams->nHeight)
        return XFrame_Stretch(pFrameOut, pFrameIn, pParams);

    XStat_DebugCb(pStatus, "Corrected aspect: in(%dx%d), ar(%dx%d), out(%dx%d), pts(%lld)",
        pFrameIn->width, pFrameIn->height, scaleParams.nWidth, scaleParams.nHeight,
        pParams->nWidth, pParams->nHeight, pParams->nPTS);

    /* Create YUV420P scaled frame for overlay */
    scaleParams.pixFmt = AV_PIX_FMT_YUV420P;
    pParams->pixFmt = AV_PIX_FMT_YUV420P;

    int nStatus = XFrame_Stretch(&scaledFrame, pFrameIn, &scaleParams);
    XASSERT((nStatus > 0), XStat_ErrCb(pStatus, "Failed to stretch frame"));

    nStatus = XFrame_BlackYUV(pFrameOut, pParams);
    XASSERT_CALL((nStatus > 0), av_frame_unref, &scaledFrame,
        XStat_ErrCb(pStatus, "Failed to create black YUV frame"));

    nStatus = XFrame_OverlayYUV(pFrameOut, &scaledFrame, pParams);
    XASSERT_CALL((nStatus > 0), av_frame_unref, &scaledFrame,
        XStat_ErrCb(pStatus, "Failed to overlay YUV frame"));

    XFRAME_SET_INT2(pFrameOut->pts, pParams->nPTS, pFrameIn->pts);
    pFrameOut->pkt_dts = pFrameIn->pkt_dts;

    av_frame_unref(&scaledFrame);
    return XSTDOK;
}

XSTATUS XFrame_Scale(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, XSTDINV);
    xstatus_t *pStatus = &pParams->status;
    XSTATUS nStatus = XSTDINV;

    XASSERT((pParams->scaleFmt != XSCALE_FMT_NONE),
        XStat_ErrCb(pStatus, "Invalid scale format"));

    if (pParams->scaleFmt == XSCALE_FMT_STRETCH)
        nStatus = XFrame_Stretch(pFrameOut, pFrameIn, pParams);
    else if (pParams->scaleFmt == XSCALE_FMT_ASPECT)
        nStatus = XFrame_Aspect(pFrameOut, pFrameIn, pParams);

    return nStatus;
}

AVFrame* XFrame_NewResample(AVFrame *pFrameIn, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, NULL);
    xstatus_t *pStatus = &pParams->status;

    /* Allocate AVFrame structure */
    AVFrame *pFrameOut = av_frame_alloc();
    XASSERT(pFrameOut, XStat_ErrPtr(pStatus, "Failed to alloc AVFrame"));

    /* Resample AVFrame */
    int nStatus = XFrame_Resample(pFrameOut, pFrameIn, pParams);
    XASSERT_CALL((nStatus > 0), av_frame_free, &pFrameOut, NULL);

    return pFrameOut;
}

AVFrame* XFrame_NewStretch(AVFrame *pFrameIn, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, NULL);
    xstatus_t *pStatus = &pParams->status;

    /* Allocate AVFrame structure */
    AVFrame *pFrameOut = av_frame_alloc();
    XASSERT(pFrameOut, XStat_ErrPtr(pStatus, "Failed to alloc AVFrame"));

    /* Stretch AVFrame */
    int nStatus = XFrame_Stretch(pFrameOut, pFrameIn, pParams);
    XASSERT_CALL((nStatus > 0), av_frame_free, &pFrameOut, NULL);

    return pFrameOut;
}

AVFrame* XFrame_NewAspect(AVFrame *pFrameIn, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, NULL);
    xstatus_t *pStatus = &pParams->status;

    /* Allocate AVFrame structure */
    AVFrame *pFrameOut = av_frame_alloc();
    XASSERT(pFrameOut, XStat_ErrPtr(pStatus, "Failed to alloc AVFrame"));

    /* Scale AVFrame */
    int nStatus = XFrame_Aspect(pFrameOut, pFrameIn, pParams);
    XASSERT_CALL((nStatus > 0), av_frame_free, &pFrameOut, NULL);

    return pFrameOut;
}

AVFrame* XFrame_NewScale(AVFrame *pFrameIn, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, NULL);
    xstatus_t *pStatus = &pParams->status;

    /* Allocate AVFrame structure */
    AVFrame *pFrameOut = av_frame_alloc();
    XASSERT(pFrameOut, XStat_ErrPtr(pStatus, "Failed to alloc AVFrame"));

    /* Scale AVFrame */
    int nStatus = XFrame_Scale(pFrameOut, pFrameIn, pParams);
    XASSERT_CALL((nStatus > 0), av_frame_free, &pFrameOut, NULL);

    return pFrameOut;
}

AVFrame* XFrame_NewBlackYUV(xframe_params_t *pParams)
{
    XASSERT_RET(pParams, NULL);
    xstatus_t *pStatus = &pParams->status;

    /* Allocate AVFrame structure */
    AVFrame *pFrameOut = av_frame_alloc();
    XASSERT(pFrameOut, XStat_ErrPtr(pStatus, "Failed to alloc AVFrame"));

    /* Initialize black YUV420 buffer */
    int nStatus = XFrame_BlackYUV(pFrameOut, pParams);
    XASSERT_CALL((nStatus > 0), av_frame_free, &pFrameOut, NULL);

    return pFrameOut;
}
