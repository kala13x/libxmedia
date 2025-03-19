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
#include <ft2build.h>
#include FT_FREETYPE_H

#define XFRAME_SET_INT(dst, src) if (src >= 0) dst = src
#define XFRAME_SET_INT2(dst, src, src2) if (src >= 0) dst = src; else dst = src2

void XFrame_RGBtoYUV(xframe_yuv_t *pYUV, uint8_t r, uint8_t g, uint8_t b)
{
    pYUV->y = (uint8_t)(0.299 * r + 0.587 * g + 0.114 * b);
    pYUV->u = (uint8_t)(-0.169 * r - 0.331 * g + 0.5 * b + 128);
    pYUV->v = (uint8_t)(0.5 * r - 0.419 * g - 0.081 * b + 128);
}

void XFrame_ColorToYUV(xframe_yuv_t *pYUV, const char *pColorName)
{
    XASSERT_RET((xstrused(pColorName)), XFrame_RGBtoYUV(pYUV, 0, 0, 0));
    if (!strncmp(pColorName, "red", 3)) XFrame_RGBtoYUV(pYUV, 255, 0, 0);
    else if (!strncmp(pColorName, "green", 5)) XFrame_RGBtoYUV(pYUV, 0, 255, 0);
    else if (!strncmp(pColorName, "blue", 4)) XFrame_RGBtoYUV(pYUV, 0, 0, 255);
    else if (!strncmp(pColorName, "white", 5)) XFrame_RGBtoYUV(pYUV, 255, 255, 255);
    else if (!strncmp(pColorName, "black", 5)) XFrame_RGBtoYUV(pYUV, 0, 0, 0);
    else if (!strncmp(pColorName, "yellow", 6)) XFrame_RGBtoYUV(pYUV, 255, 255, 0);
    else if (!strncmp(pColorName, "cyan", 4)) XFrame_RGBtoYUV(pYUV, 0, 255, 255);
    else if (!strncmp(pColorName, "magenta", 7)) XFrame_RGBtoYUV(pYUV, 255, 0, 255);
    else XFrame_RGBtoYUV(pYUV, 0, 0, 0);
}

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
    pFrame->nX = XSTDERR;
    pFrame->nY = XSTDERR;

    pFrame->mediaType = AVMEDIA_TYPE_UNKNOWN;
    pFrame->nIndex = XSTDERR;
    pFrame->nPTS = XSTDERR;

    pFrame->source[0] = XSTR_NUL;
    pFrame->color[0] = XSTR_NUL;

    /* General parameters */
    if (pParent) XStat_InitFrom(&pFrame->status, &pParent->status);
    else XStat_Init(&pFrame->status, XSTDNON, NULL, NULL);
}

XSTATUS XFrame_CopyParams(xframe_params_t *pDstParams, xframe_params_t *pSrcParams)
{
    XASSERT_RET(pDstParams, XSTDINV);
    memset(pDstParams, 0, sizeof(xframe_params_t));
    XASSERT_RET(pSrcParams, XSTDINV);

    pDstParams->sampleFmt = pSrcParams->sampleFmt;
    pDstParams->nSampleRate = pSrcParams->nSampleRate;
    pDstParams->nChannels = pSrcParams->nChannels;

    pDstParams->pixFmt = pSrcParams->pixFmt;
    pDstParams->scaleFmt = pSrcParams->scaleFmt;
    pDstParams->nWidth = pSrcParams->nWidth;
    pDstParams->nHeight = pSrcParams->nHeight;
    pDstParams->nX = pSrcParams->nX;
    pDstParams->nY = pSrcParams->nY;

    pDstParams->mediaType = pSrcParams->mediaType;
    pDstParams->nIndex = pSrcParams->nIndex;
    pDstParams->nPTS = pSrcParams->nPTS;

    xstrncpy(pDstParams->source, sizeof(pDstParams->source), pSrcParams->source);
    xstrncpy(pDstParams->color, sizeof(pDstParams->color), pSrcParams->color);
    XStat_InitFrom(&pDstParams->status, &pSrcParams->status);

    return XSTDOK;
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

XSTATUS XFrame_ConvertToYUV(AVFrame* pFrameOut, const AVFrame* pFrameIn, xframe_params_t *pParams)
{
    XASSERT_RET((pFrameOut && pFrameIn && pParams), XSTDINV);
    xstatus_t *pStatus = &pParams->status;

    // Allocate the destination frame
    pFrameOut->format = AV_PIX_FMT_YUV420P;
    pFrameOut->width = pFrameIn->width;
    pFrameOut->height = pFrameIn->height;

    // Allocate buffer for the destination frame
    pStatus->nAVStatus = av_frame_get_buffer(pFrameOut, 32);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Failed to allocate memory for AVFrame buffer"));

    // Create and configure the SwsContext
    struct SwsContext* swsCtx = sws_getContext(pFrameIn->width, pFrameIn->height,
            (enum AVPixelFormat)pFrameIn->format, pFrameOut->width, pFrameOut->height,
            AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

    pStatus->nAVStatus = swsCtx != NULL ? 0 : AVERROR_UNKNOWN;
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Failed to get or create SWS context"));

    // Perform the scaling (conversion)
    pStatus->nAVStatus = sws_scale(swsCtx, (const uint8_t* const*)pFrameIn->data,
        pFrameIn->linesize, 0, pFrameIn->height, (uint8_t* const*)pFrameOut->data, pFrameOut->linesize);

    XASSERT_CALL((pStatus->nAVStatus >= 0), sws_freeContext, swsCtx,
        XStat_ErrCb(pStatus, "Error while scaling the frame"));

    // Free the SwsContext
    sws_freeContext(swsCtx);
    return XSTDOK;
}

AVFrame* XFrame_FromFile(xframe_params_t *pParams, const char *pPath)
{
    XASSERT_RET((pParams && pPath), NULL);
    xstatus_t *pStatus = &pParams->status;

    AVFormatContext* formatContext = NULL;
    pStatus->nAVStatus = avformat_open_input(&formatContext, pPath, NULL, NULL);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrPtr(pStatus, "Could not open input file"));

    pStatus->nAVStatus = avformat_find_stream_info(formatContext, NULL);
    XASSERT_CALL((pStatus->nAVStatus >= 0), avformat_close_input, &formatContext,
        XStat_ErrPtr(pStatus, "Could not find stream information"));

    AVCodecParameters* codecPar = NULL;
    const AVCodec* codec;
    int vidStreamIndex = -1;

    for (unsigned int i = 0; i < formatContext->nb_streams; i++)
    {
        AVCodecParameters* localCodecPar = formatContext->streams[i]->codecpar;
        const AVCodec* localCodec = avcodec_find_decoder(localCodecPar->codec_id);
        if (localCodec == NULL) continue;

        if (localCodecPar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            vidStreamIndex = i;
            codec = localCodec;
            codecPar = localCodecPar;
            break;
        }
    }

    XASSERT_CALL((vidStreamIndex >= 0), avformat_close_input, &formatContext,
        XStat_ErrPtr(pStatus, "Could not find a video stream in the input file"));

    pStatus->nAVStatus = AVERROR_UNKNOWN;
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    XASSERT_CALL(codecContext, avformat_close_input, &formatContext,
        XStat_ErrPtr(pStatus, "Could not allocate codec context"));

    pStatus->nAVStatus = avcodec_parameters_to_context(codecContext, codecPar);
    if (pStatus->nAVStatus < 0)
    {
        XStat_ErrCb(pStatus, "Could not initialize codec context");
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        return NULL;
    }

    pStatus->nAVStatus = avcodec_open2(codecContext, codec, NULL);
    if (pStatus->nAVStatus < 0)
    {
        XStat_ErrCb(pStatus, "Could not open codec");
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        return NULL;
    }

    AVPacket* packet = av_packet_alloc();
    if (!packet)
    {
        pStatus->nAVStatus = AVERROR_UNKNOWN;
        XStat_ErrCb(pStatus, "Could not allocate AVPacket");
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        return NULL;
    }

    int response;
    char errBuf[AV_ERROR_MAX_STRING_SIZE];

    AVFrame* pFrame = av_frame_alloc();
    if (!packet)
    {
        pStatus->nAVStatus = AVERROR_UNKNOWN;
        XStat_ErrCb(pStatus, "Could not allocate AVPacket");
        avcodec_free_context(&codecContext);
        avformat_close_input(&formatContext);
        av_packet_free(&packet);
        return NULL;
    }

    while (av_read_frame(formatContext, packet) >= 0)
    {
        if (packet->stream_index == vidStreamIndex)
        {
            response = avcodec_send_packet(codecContext, packet);
            if (response < 0)
            {
                av_strerror(response, errBuf, AV_ERROR_MAX_STRING_SIZE);
                XStat_ErrCb(pStatus, "Error while sending a packet to the decoder: %s", errBuf);
                continue;
            }

            response = avcodec_receive_frame(codecContext, pFrame);
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) continue;

            if (response < 0)
            {
                av_strerror(response, errBuf, AV_ERROR_MAX_STRING_SIZE);
                XStat_ErrCb(pStatus, "Error while receiving a frame from the decoder: %s",errBuf);
                avcodec_free_context(&codecContext);
                avformat_close_input(&formatContext);
                av_packet_free(&packet);
                av_frame_free(&pFrame);
                return NULL;
            }

            if (pFrame->format != AV_PIX_FMT_YUV420P)
            {
                AVFrame yuvFrame;
                XFrame_InitFrame(&yuvFrame);

                if (XFrame_ConvertToYUV(&yuvFrame, pFrame, pParams) < 0)
                {
                    XStat_ErrCb(pStatus, "Failed to convert frame to YUV");
                    avcodec_free_context(&codecContext);
                    avformat_close_input(&formatContext);
                    av_frame_unref(&yuvFrame);
                    av_packet_free(&packet);
                    av_frame_free(&pFrame);
                    return NULL;
                }

                av_frame_unref(pFrame);
                av_frame_move_ref(pFrame, &yuvFrame);
                av_frame_unref(&yuvFrame);
            }

            break;
        }

        av_packet_unref(packet);
    }

    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);
    av_packet_free(&packet);

    return pFrame;
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

XSTATUS XFrame_GenerateYUV(AVFrame *pFrameOut, xframe_params_t *pParams)
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

    xframe_yuv_t yuv = {0};
    XFrame_ColorToYUV(&yuv, pParams->color);

    /* Fill Y plane with black (0x00) and U/V planes with neutral chroma value (0x80) */
    memset(pFrameOut->data[0], yuv.y, pFrameOut->linesize[0] * pParams->nHeight);
    memset(pFrameOut->data[1], yuv.u, pFrameOut->linesize[1] * (pParams->nHeight / 2));
    memset(pFrameOut->data[2], yuv.v, pFrameOut->linesize[2] * (pParams->nHeight / 2));

    return XSTDOK;
}

XSTATUS XFrame_OverlayYUV(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, XSTDINV);
    xstatus_t *pStatus = &pParams->status;
    pStatus->nAVStatus = AVERROR_UNKNOWN;

    XASSERT((pFrameOut && pFrameIn), XStat_ErrCb(pStatus, "Invalid overlay src/dst frame arguments"));
    XASSERT((pFrameIn->width && pFrameIn->height), XStat_ErrCb(pStatus, "Invalid src frame resolution"));
    XASSERT((pFrameOut->width && pFrameOut->height), XStat_ErrCb(pStatus, "Invalid dst frame resolution"));

    XASSERT((pFrameOut->height >= pFrameIn->height && pFrameOut->width >= pFrameIn->width),
            XStat_ErrCb(pStatus, "Overlay src is bigger than dst: src(%dx%d), dst(%dx%d)",
            pFrameIn->width, pFrameIn->height, pFrameOut->width, pFrameOut->height));

    pParams->pixFmt = AV_PIX_FMT_YUV420P;
    pFrameOut->format = pParams->pixFmt;

    int i, nSrcWidth = pFrameIn->width;
    int nSrcHeight = pFrameIn->height;

    int nOffsetX = (pFrameOut->width - nSrcWidth) / 2;
    int nOffsetY = (pFrameOut->height - nSrcHeight) / 2;

    /* Copy Y (luma) plane */
    for (i = 0; i < nSrcHeight; i++)
    {
        int y = i + nOffsetY;
        memcpy(pFrameOut->data[0] + y * pFrameOut->linesize[0] + nOffsetX,
               pFrameIn->data[0] + i * pFrameIn->linesize[0], nSrcWidth);
    }

    /* Copy U and V (chroma) planes */
    nSrcWidth /= 2; nSrcHeight /= 2;
    nOffsetX /= 2; nOffsetY /= 2;

    for (i = 0; i < nSrcHeight; i++)
    {
        int y = i + nOffsetY;
        memcpy(pFrameOut->data[1] + y * pFrameOut->linesize[1] + nOffsetX,
               pFrameIn->data[1] + i * pFrameIn->linesize[1], nSrcWidth);

        memcpy(pFrameOut->data[2] + y * pFrameOut->linesize[2] + nOffsetX,
               pFrameIn->data[2] + i * pFrameIn->linesize[2], nSrcWidth);
    }

    return XSTDOK;
}

XSTATUS XFrame_Border(AVFrame *pFrameOut, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, XSTDINV);
    xstatus_t *pStatus = &pParams->status;
    pStatus->nAVStatus = AVERROR_UNKNOWN;

    XASSERT(pFrameOut, XStat_ErrCb(pStatus, "Invalid frame argument"));
    XASSERT((pFrameOut->width && pFrameOut->height), XStat_ErrCb(pStatus, "Invalid dst frame resolution"));

    if ((pParams->nX <= 0 && pParams->nY <= 0) ||
         pParams->nX >= pFrameOut->width ||
         pParams->nY >= pFrameOut->height)
    {
        XStat_ErrCb(pStatus, "Invalid border x/y or dimensions: %d/%d, %dx%d",
                pParams->nX, pParams->nY, pFrameOut->width, pFrameOut->height);

        return XSTDERR;
    }

    int borderThicknessX = pParams->nX;
    int borderThicknessY = pParams->nY;
    int frameWidth = pFrameOut->width;
    int frameHeight = pFrameOut->height;

    // Get YUV from color name
    xframe_yuv_t yuv = {0};
    XFrame_ColorToYUV(&yuv, pParams->color);

    // Draw border on Y plane
    for (int y = 0; y < frameHeight; y++)
    {
        for (int x = 0; x < frameWidth; x++)
        {
            if (x < borderThicknessX || x >= frameWidth - borderThicknessX ||
                y < borderThicknessY || y >= frameHeight - borderThicknessY)
            {
                pFrameOut->data[0][y * pFrameOut->linesize[0] + x] = yuv.y;
            }
        }
    }

    // Draw border on U and V planes (chroma planes)
    int chromaWidth = frameWidth / 2;
    int chromaHeight = frameHeight / 2;
    int chromaThicknessX = borderThicknessX / 2;
    int chromaThicknessY = borderThicknessY / 2;

    for (int y = 0; y < chromaHeight; y++)
    {
        for (int x = 0; x < chromaWidth; x++)
        {
            if (x < chromaThicknessX || x >= chromaWidth - chromaThicknessX ||
                y < chromaThicknessY || y >= chromaHeight - chromaThicknessY)
            {
                pFrameOut->data[1][y * pFrameOut->linesize[1] + x] = yuv.u;
                pFrameOut->data[2][y * pFrameOut->linesize[2] + x] = yuv.v;
            }
        }
    }

    return XSTDOK;
}

XSTATUS XFrame_BorderYUV(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, XSTDINV);
    xstatus_t *pStatus = &pParams->status;
    pStatus->nAVStatus = AVERROR_UNKNOWN;

    XASSERT((pFrameOut && pFrameIn), XStat_ErrCb(pStatus, "Invalid border src/dst frame arguments"));
    XASSERT((pFrameIn->width && pFrameIn->height), XStat_ErrCb(pStatus, "Invalid src frame resolution"));

    if ((pParams->nX <= 0 && pParams->nY <= 0) ||
         pParams->nX >= pFrameIn->width ||
         pParams->nY >= pFrameIn->height)
    {
        XStat_ErrCb(pStatus, "Invalid border x/y or dimensions: %d/%d, %dx%d",
                pParams->nX, pParams->nY, pFrameIn->width, pFrameIn->height);

        return XSTDERR;
    }

    if (XFrame_GenerateYUV(pFrameOut, pParams) < 0)
    {
        XStat_ErrCb(pStatus, "Error on generating a YUV frame");
        av_frame_unref(pFrameOut);
        return XSTDERR;
    }

    AVFrame croppedFrame;
    XFrame_InitFrame(&croppedFrame);


    xframe_params_t cropParams;
    XFrame_CopyParams(&cropParams, pParams);

    cropParams.nWidth = pFrameIn->width - pParams->nX;
    cropParams.nHeight = pFrameIn->height - pParams->nY;

    if (XFrame_Crop(&croppedFrame, pFrameIn, &cropParams))
    {
        XStat_ErrCb(pStatus, "Error on cropping the YUV frame");
        av_frame_unref(&croppedFrame);
        av_frame_unref(pFrameOut);
        return XSTDERR;
    }

    xframe_params_t overlayParams; // Default is to overlay at center
    XFrame_InitParams(&overlayParams, pParams);

    if (XFrame_OverlayYUV(pFrameOut, &croppedFrame, &overlayParams))
    {
        XStat_ErrCb(pStatus, "Error on overlaying the YUV frame");
        av_frame_unref(&croppedFrame);
        av_frame_unref(pFrameOut);
        return XSTDERR;
    }

    av_frame_unref(&croppedFrame);
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

    nStatus = XFrame_GenerateYUV(pFrameOut, pParams);
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

XSTATUS XFrame_Crop(AVFrame* pFrameOut, AVFrame* pFrameIn, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, XSTDINV);
    xstatus_t *pStatus = &pParams->status;
    pStatus->nAVStatus = AVERROR_UNKNOWN;

    int cropWidth = pParams->nWidth;
    int cropHeight = pParams->nHeight;

    XASSERT((pFrameOut && pFrameIn),
        XStat_ErrCb(pStatus, "Invalid crop in/out frame arguments"));
    XASSERT((cropWidth > 0 && cropHeight > 0),
        XStat_ErrCb(pStatus, "Invalid crop resolution: %dx%d", cropWidth, cropHeight));
    XASSERT((cropWidth <= pFrameIn->width && cropHeight <= pFrameIn->height),
        XStat_ErrCb(pStatus, "Invalid source resolution: %dx%d", pFrameIn->width, pFrameIn->height));

    int offsetX = (pFrameIn->width - cropWidth) / 2;
    int offsetY = (pFrameIn->height - cropHeight) / 2;

    // Allocate the destination frame buffer
    pStatus->nAVStatus = av_image_alloc(pFrameOut->data, pFrameOut->linesize,
            cropWidth, cropHeight, (enum AVPixelFormat)pFrameIn->format, 32);

    XASSERT((pStatus->nAVStatus >= 0),
        XStat_ErrCb(pStatus, "Failed to allocate memory for AVFrame buffer"));

    // Set the destination frame properties
    pFrameOut->width = cropWidth;
    pFrameOut->height = cropHeight;
    pFrameOut->format = pFrameIn->format;

    // Crop the Y (luma) plane
    for (int y = 0; y < cropHeight; y++)
    {
        uint8_t* dstPtr = pFrameOut->data[0] + y * pFrameOut->linesize[0];
        uint8_t* srcPtr = pFrameIn->data[0] + (y + offsetY) * pFrameIn->linesize[0] + offsetX;
        memcpy(dstPtr, srcPtr, cropWidth);
    }

    // Crop the U and V (chroma) planes
    int chromaWidth = cropWidth / 2;
    int chromaHeight = cropHeight / 2;
    int chromaOffsetX = offsetX / 2;
    int chromaOffsetY = offsetY / 2;

    for (int y = 0; y < chromaHeight; y++)
    {
        uint8_t* dstUPtr = pFrameOut->data[1] + y * pFrameOut->linesize[1];
        uint8_t* srcUPtr = pFrameIn->data[1] + (y + chromaOffsetY) * pFrameIn->linesize[1] + chromaOffsetX;
        memcpy(dstUPtr, srcUPtr, chromaWidth);

        uint8_t* dstVPtr = pFrameOut->data[2] + y * pFrameOut->linesize[2];
        uint8_t* srcVPtr = pFrameIn->data[2] + (y + chromaOffsetY) * pFrameIn->linesize[2] + chromaOffsetX;
        memcpy(dstVPtr, srcVPtr, chromaWidth);
    }

    return XSTDOK;
}

XSTATUS XFrame_OverlayText(AVFrame *pFrameOut, xframe_params_t *pParams, const char *pText)
{
    XASSERT_RET(pParams, XSTDINV);
    xstatus_t *pStatus = &pParams->status;
    pStatus->nAVStatus = AVERROR_UNKNOWN;

    XASSERT(pFrameOut, XStat_ErrCb(pStatus, "Invalid dst frame for overlay text"));
    XASSERT((xstrused(pText)), XStat_ErrCb(pStatus, "Invalid text to overlay"));
    XASSERT((xstrused(pParams->source)), XStat_ErrCb(pStatus, "Invalid font path"));
    XASSERT((pParams->nHeight > 0), XStat_ErrCb(pStatus, "Invalid font height"));

    struct stat stat_buf;
    if (stat(pParams->source, &stat_buf) != 0)
    {
        XStat_ErrCb(pStatus, "Font file does not exist");
        return -1;
    }

    // Initialize FreeType
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        XStat_ErrCb(pStatus, "Could not initialize FreeType");
        return -1;
    }

    FT_Face face;
    if (FT_New_Face(ft, pParams->source, 0, &face))
    {
        XStat_ErrCb(pStatus, "Failed to load font");
        return -1;
    }

    FT_Set_Pixel_Sizes(face, 0, pParams->nHeight);
    int maxGlyphHeight = 0;
    int totalTextWidth = 0;
    int i, textLen = strlen(pText);

    for (i = 0; i < textLen; i++)
    {
        char c = pText[i];
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            XStat_ErrCb(pStatus, "Failed to load glyph for character: %c", c);
            continue;
        }

        // Get the width of the glyph in pixels
        totalTextWidth += face->glyph->advance.x >> 6;
        maxGlyphHeight = XSTD_MAX(maxGlyphHeight, face->glyph->bitmap_top);
    }

    int penX = (pFrameOut->width - totalTextWidth) / 2;
    int penY = (pFrameOut->height + maxGlyphHeight) / 2;

    for (i = 0; i < textLen; i++)
    {
        char c = pText[i];
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            XStat_ErrCb(pStatus, "Failed to load glyph for character: %c", c);
            continue;
        }

        // Access the glyph bitmap
        FT_GlyphSlot g = face->glyph;

        // Copy the bitmap into the Y plane of the YUV frame
        for (unsigned int y = 0; y < g->bitmap.rows; ++y)
        {
            for (unsigned int x = 0; x < g->bitmap.width; ++x)
            {
                // Calculate where to draw the text (position each glyph horizontally)
                int frameX = penX + g->bitmap_left + x;
                int frameY = penY - g->bitmap_top + y;

                if (frameX >= 0 && frameX < pFrameOut->width && frameY >= 0 && frameY < pFrameOut->height)
                {
                    // Get the Y plane data and blend the glyph bitmap pixel into the Y plane (and fuck my life as well)
                    uint8_t *yPlane = pFrameOut->data[0] + frameY * pFrameOut->linesize[0] + frameX;
                    *yPlane = (uint8_t)(g->bitmap.buffer[y * g->bitmap.width + x]);
                }
            }
        }

        // Move the pen to the right for the next character (advance by glyph width + spacing)
        penX += g->advance.x >> 6;  // Advance.x is in 1/64th pixel, so shift by 6 to get pixel units
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    return XSTDOK;
}

XSTATUS XFrame_SaveToJPEG(AVFrame *pFrameIn, const char *pDstPath, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, XSTDINV);
    xstatus_t *pStatus = &pParams->status;
    pStatus->nAVStatus = AVERROR_UNKNOWN;

    XASSERT(pFrameIn, XStat_ErrCb(pStatus, "Invalid input frame"));
    const AVCodec *pCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    XASSERT(pCodec, XStat_ErrCb(pStatus, "Codec not found"));

    AVCodecContext *pCodecCtx = avcodec_alloc_context3(pCodec);
    XASSERT(pCodecCtx, XStat_ErrCb(pStatus, "Could not allocate video codec context"));

    pCodecCtx->bit_rate = 400000;
    pCodecCtx->width = pFrameIn->width;
    pCodecCtx->height = pFrameIn->height;
    pCodecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    pCodecCtx->time_base = (AVRational){1, 25};

    pStatus->nAVStatus = avcodec_open2(pCodecCtx, pCodec, NULL);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Could not open MJPEG codec"));
    XStat_InfoCb(pStatus, "Saving frame to JPEG: %dx%d", pFrameIn->width, pFrameIn->height);

    AVPacket packet;
    bzero(&packet, sizeof(AVPacket));
    av_packet_unref(&packet);

    // packet data will be allocated by the encoder
    packet.data = NULL;
    packet.size = 0;

    pStatus->nAVStatus = avcodec_send_frame(pCodecCtx, pFrameIn);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Error sending a frame for encoding"));

    pStatus->nAVStatus = avcodec_receive_packet(pCodecCtx, &packet);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Error receiving packet from encoder"));

    XSTATUS nStatus = XPath_Write(pDstPath, (uint8_t*)packet.data, packet.size, "cwt");
    XASSERT((nStatus > 0), XStat_ErrCb(pStatus, "Could not write to: %s (%s)", pDstPath, XSTRERR));

    avcodec_free_context(&pCodecCtx);
    av_packet_unref(&packet);

    return XSTDOK;
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

AVFrame* XFrame_NewCrop(AVFrame *pFrameIn, xframe_params_t *pParams)
{
    XASSERT_RET(pParams, NULL);
    xstatus_t *pStatus = &pParams->status;

    /* Allocate AVFrame structure */
    AVFrame *pFrameOut = av_frame_alloc();
    XASSERT(pFrameOut, XStat_ErrPtr(pStatus, "Failed to alloc AVFrame"));

    /* Crop AVFrame */
    int nStatus = XFrame_Crop(pFrameOut, pFrameIn, pParams);
    XASSERT_CALL((nStatus > 0), av_frame_free, &pFrameOut, NULL);

    return pFrameOut;
}

AVFrame* XFrame_NewYUV(xframe_params_t *pParams)
{
    XASSERT_RET(pParams, NULL);
    xstatus_t *pStatus = &pParams->status;

    /* Allocate AVFrame structure */
    AVFrame *pFrameOut = av_frame_alloc();
    XASSERT(pFrameOut, XStat_ErrPtr(pStatus, "Failed to alloc AVFrame"));

    /* Initialize black YUV420 buffer */
    int nStatus = XFrame_GenerateYUV(pFrameOut, pParams);
    XASSERT_CALL((nStatus > 0), av_frame_free, &pFrameOut, NULL);

    return pFrameOut;
}
