/*!
 *  @file libxmedia/src/encoder.c
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the video/audio encoding
 * functionality based on the FFMPEG API and libraries.
 */

#include "encoder.h"

void XEncoder_Init(xencoder_t *pEncoder)
{
    XASSERT_VOID(pEncoder);
    XStreams_Init(&pEncoder->streams);
    XStat_Init(&pEncoder->status, XSTDNON, NULL, NULL);

    pEncoder->sOutputPath[0] = XSTR_NUL;
    pEncoder->sOutFormat[0] = XSTR_NUL;
    pEncoder->nIOBuffSize = 0;
    pEncoder->pIOBuffer = NULL;
    pEncoder->pIOCtx = NULL;
    pEncoder->pFmtCtx = NULL;

    pEncoder->statusCallback = NULL;
    pEncoder->packetCallback = NULL;
    pEncoder->muxerCallback = NULL;
    pEncoder->errorCallback = NULL;
    pEncoder->pUserCtx = NULL;

    pEncoder->nStartTime = XSTDNON;
    pEncoder->eTSType = XPTS_RESCALE;
    pEncoder->nTSFix = XSTDNON;

    pEncoder->bOutputOpen = XFALSE;
    pEncoder->bMuxOnly = XFALSE;
}

void XEncoder_Destroy(xencoder_t *pEncoder)
{
    XASSERT_VOID(pEncoder);
    XStreams_Destroy(&pEncoder->streams);
    pEncoder->bOutputOpen = XFALSE;

    if (pEncoder->pFmtCtx != NULL && pEncoder->pIOCtx == NULL &&
        !(pEncoder->pFmtCtx->oformat->flags & AVFMT_NOFILE))
    {
        avio_closep(&pEncoder->pFmtCtx->pb);
        pEncoder->pFmtCtx->pb = NULL;
    }

    if (pEncoder->pIOCtx)
    {
        avio_context_free(&pEncoder->pIOCtx);
        pEncoder->pIOCtx = NULL;
    }

    if (pEncoder->pIOBuffer)
    {
        av_free(pEncoder->pIOBuffer);
        pEncoder->pIOBuffer = NULL;
    }

    if (pEncoder->pFmtCtx != NULL)
    {
        avformat_free_context(pEncoder->pFmtCtx);
        pEncoder->pFmtCtx = NULL;
    }
}

XSTATUS XEncoder_OpenFormat(xencoder_t *pEncoder, const char *pFormat, const char *pOutputUrl)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    /* Validate arguments */
    XASSERT((pFormat || pOutputUrl), XStat_ErrCb(pStatus, "Invalid format arguments"));
    if (pOutputUrl) xstrncpy(pEncoder->sOutputPath, sizeof(pEncoder->sOutputPath), pOutputUrl);
    if (pFormat) xstrncpy(pEncoder->sOutFormat, sizeof(pEncoder->sOutFormat), pFormat);

    int nStatus = avformat_alloc_output_context2(&pEncoder->pFmtCtx, NULL, pFormat, pOutputUrl);
    XASSERT((nStatus >= 0), XStat_ErrCb(pStatus, "Failed to alloc output context: fmt(%s) url(%s)",
        pFormat != NULL ? pFormat : "NULL", pOutputUrl != NULL ? pOutputUrl : "NULL"));

    return XSTDOK;
}

XSTATUS XEncoder_GuessFormat(xencoder_t *pEncoder, const char *pFormat, const char *pOutputUrl)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    /* Validate arguments */
    XASSERT((pFormat || pOutputUrl), XStat_ErrCb(pStatus, "Invalid format arguments"));
    if (pOutputUrl) xstrncpy(pEncoder->sOutputPath, sizeof(pEncoder->sOutputPath), pOutputUrl);
    if (pFormat) xstrncpy(pEncoder->sOutFormat, sizeof(pEncoder->sOutFormat), pFormat);

    /* Allocate format context */
    pEncoder->pFmtCtx = avformat_alloc_context();
    XASSERT(pEncoder->pFmtCtx, XStat_ErrCb(pStatus, "Failed to alloc format context"));

    /* Guess output format for muxing by format name */
    pEncoder->pFmtCtx->oformat = av_guess_format(pFormat, pOutputUrl, NULL);
    XASSERT(pEncoder->pFmtCtx->oformat, XStat_ErrCb(pStatus, "Format not found: fmt(%s) url(%s)",
        pFormat != NULL ? pFormat : "NULL", pOutputUrl != NULL ? pOutputUrl : "NULL"));

    return XSTDOK;
}

XSTATUS XEncoder_RestartCodec(xencoder_t *pEncoder, int nStreamIndex)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    xstream_t *pStream = XStreams_GetByDstIndex(&pEncoder->streams, nStreamIndex);
    XASSERT(pStream, XStat_ErrCb(pStatus, "Stream is not found: %s", nStreamIndex));

    if (pStream->pCodecCtx != NULL)
    {
        avcodec_free_context(&pStream->pCodecCtx);
        pStream->pCodecCtx = NULL;
        pStream->bCodecOpen = XFALSE;
    }

    xcodec_t *pCodecInfo = &pStream->codecInfo;
    const AVCodec *pAvCodec = avcodec_find_encoder(pCodecInfo->codecId);
    XASSERT(pAvCodec, XStat_ErrCb(pStatus, "Failed to find encoder: %d", (int)pCodecInfo->codecId));

    pStream->pCodecCtx = avcodec_alloc_context3(pAvCodec);
    XASSERT(pStream->pCodecCtx, XStat_ErrCb(pStatus, "Failed to allocate encoder context"));

    XSTATUS nStatus = XCodec_ApplyToAVCodec(pCodecInfo, pStream->pCodecCtx);
    XASSERT((nStatus == XSTDOK), XStat_ErrCb(pStatus, "Failed to apply codec to context: %d", pStream->nDstIndex));

    if (pEncoder->pFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        pStream->pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    pStatus->nAVStatus = avcodec_open2(pStream->pCodecCtx, pAvCodec, NULL);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Cannot open encoder: %d", pStream->nDstIndex));

    pStatus->nAVStatus = avcodec_parameters_from_context(pStream->pAvStream->codecpar, pStream->pCodecCtx);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Failed to copy codec parameters: %d", pStream->nDstIndex));

    XStat_InfoCb(pStatus, "Restarted codec: id(%d), type(%d), tb(%d.%d), ind(%d)",
        (int)pCodecInfo->codecId, (int)pCodecInfo->mediaType,
        pStream->pCodecCtx->time_base.num,
        pStream->pCodecCtx->time_base.den,
        pStream->nDstIndex);

    XCodec_GetFromAVCodec(&pStream->codecInfo, pStream->pCodecCtx);
    pStream->bCodecOpen = XTRUE;

    return pStream->nDstIndex;
}

XSTATUS XEncoder_OpenStream(xencoder_t *pEncoder, xcodec_t *pCodecInfo)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    XASSERT((pCodecInfo != NULL), XStat_ErrCb(pStatus, "Invalid stream information argument"));
    XASSERT(pEncoder->pFmtCtx, XStat_ErrCb(pStatus, "Output format context is not initialized"));

    xstream_t *pStream = XStreams_NewStream(&pEncoder->streams);
    XASSERT(pStream, XStat_ErrCb(pStatus, "Failed to create stream: %s", strerror(errno)));

    const AVCodec *pAvCodec = avcodec_find_encoder(pCodecInfo->codecId);
    XASSERT(pAvCodec, XStat_ErrCb(pStatus, "Failed to find encoder: %d", (int)pCodecInfo->codecId));

    pStream->pAvStream = avformat_new_stream(pEncoder->pFmtCtx, pAvCodec);
    XASSERT(pStream->pAvStream, XStat_ErrCb(pStatus, "Failed to create avstream: %s", strerror(errno)));

    XCodec_Copy(&pStream->codecInfo, pCodecInfo);
    int nDstIndex = pStream->pAvStream->index;

    XSTATUS nStatus = XCodec_ApplyToAVStream(pCodecInfo, pStream->pAvStream);
    XASSERT((nStatus == XSTDOK), XStat_ErrCb(pStatus, "Failed to apply codec to stream: dst(%d)", nDstIndex));

    char sCodecStr[XSTR_MID];
    sCodecStr[0] = '\0';

    if (pEncoder->bMuxOnly)
    {
        XCodec_DumpStr(pCodecInfo, sCodecStr, sizeof(sCodecStr));
        XStat_InfoCb(pStatus, "Muxing stream: %s, dst(%d)", sCodecStr, nDstIndex);

        pStream->pAvStream->codecpar->codec_tag = 0;
        pStream->nDstIndex = nDstIndex;
        return pStream->nDstIndex;
    }

    pStream->pCodecCtx = avcodec_alloc_context3(pAvCodec);
    XASSERT(pStream->pCodecCtx, XStat_ErrCb(pStatus, "Failed to allocate encoder context"));

    nStatus = XCodec_ApplyToAVCodec(pCodecInfo, pStream->pCodecCtx);
    XASSERT((nStatus == XSTDOK), XStat_ErrCb(pStatus, "Failed to apply codec to context: dst(%d)", nDstIndex));

    if (pEncoder->pFmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        pStream->pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    pStatus->nAVStatus = avcodec_open2(pStream->pCodecCtx, pAvCodec, NULL);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Cannot open encoder: dst(%d)", nDstIndex));

    pStatus->nAVStatus = avcodec_parameters_from_context(pStream->pAvStream->codecpar, pStream->pCodecCtx);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Failed to copy codec parameters: dst(%d)", nDstIndex));

    XCodec_GetFromAVCodec(&pStream->codecInfo, pStream->pCodecCtx);
    XCodec_DumpStr(pCodecInfo, sCodecStr, sizeof(sCodecStr));
    XStat_InfoCb(pStatus, "Encoding stream: %s, dst(%d)", sCodecStr, nDstIndex);

    pStream->nDstIndex = nDstIndex;
    pStream->bCodecOpen = XTRUE;

    return pStream->nDstIndex;
}

const xcodec_t* XEncoder_GetCodecInfo(xencoder_t* pEncoder, int nStreamIndex)
{
    XASSERT(pEncoder, NULL);
    xstatus_t *pStatus = &pEncoder->status;

    xstream_t *pStream = XStreams_GetByDstIndex(&pEncoder->streams, nStreamIndex);
    if (pStream == NULL)
    {
        XStat_ErrCb(pStatus, "Stream is not found: dst(%d)", nStreamIndex);
        return NULL;
    }

    return XStream_GetCodecInfo(pStream);
}

XSTATUS XEncoder_CopyCodecInfo(xencoder_t* pEncoder, xcodec_t *pCodecInfo, int nStreamIndex)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;
    XASSERT(pCodecInfo, XStat_ErrCb(pStatus, "Invalid codec info argument"));

    xstream_t *pStream = XStreams_GetByDstIndex(&pEncoder->streams, nStreamIndex);
    XASSERT(pStream, XStat_ErrCb(pStatus, "Stream is not found: dst(%d)", nStreamIndex));
    return XStream_CopyCodecInfo(pStream, pCodecInfo);
}

XSTATUS XEncoder_WriteHeader(xencoder_t *pEncoder, AVDictionary *pHeaderOpts)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    XASSERT(pEncoder->pFmtCtx, XStat_ErrCb(pStatus, "Invalid format context"));
    XASSERT(pEncoder->bOutputOpen, XStat_ErrCb(pStatus, "Output context is not open"));

    pStatus->nAVStatus = avformat_write_header(pEncoder->pFmtCtx, &pHeaderOpts);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Failed to write header"));
    return XSTDOK;
}

XSTATUS XEncoder_OpenOutput(xencoder_t *pEncoder, AVDictionary *pOpts)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    XASSERT(pEncoder->pFmtCtx, XStat_ErrCb(pStatus, "Invalid format context"));
    xbool_t bAvFormatNoFile = (pEncoder->pFmtCtx->oformat->flags & AVFMT_NOFILE);

    XASSERT((pEncoder->muxerCallback != NULL || xstrused(pEncoder->sOutputPath)),
        XStat_ErrCb(pStatus, "Required muxer callback or output file to open the muxer"));

    if (pEncoder->muxerCallback)
    {
        if (!pEncoder->nIOBuffSize) pEncoder->nIOBuffSize = XENCODER_IO_SIZE;
        size_t nPacketSize = pEncoder->nIOBuffSize;

        unsigned char *pBuffer = (unsigned char *)av_malloc(nPacketSize);
        XASSERT(pBuffer, XStat_ErrCb(pStatus, "Failed to alloc output buffer: %s", strerror(errno)));

        pEncoder->pIOCtx = avio_alloc_context(pBuffer, nPacketSize, 1,
            pEncoder->pUserCtx, NULL, pEncoder->muxerCallback, NULL);

        XASSERT_CALL(pEncoder->pIOCtx, av_free, pBuffer,
            XStat_ErrCb(pStatus, "Failed to alloc output context"));

        pEncoder->pFmtCtx->packet_size = nPacketSize;
        pEncoder->pFmtCtx->pb = pEncoder->pIOCtx;
        pEncoder->pIOBuffer = pBuffer;

        XStat_InfoCb(pStatus, "Created output context: buffer(%zu)", nPacketSize);
    }
    else if (xstrused(pEncoder->sOutputPath))
    {
        if (!bAvFormatNoFile)
        {
            pStatus->nAVStatus = avio_open(&pEncoder->pFmtCtx->pb, pEncoder->sOutputPath, AVIO_FLAG_WRITE);
            XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Failed to open output context"));
        }

        XStat_InfoCb(pStatus, "Output context: url(%s), AVFMT_NOFILE(%s)",
            pEncoder->sOutputPath, bAvFormatNoFile ? "true" : "false");
    }

    pEncoder->bOutputOpen = XTRUE;
    return XEncoder_WriteHeader(pEncoder, pOpts);
}

XSTATUS XEncoder_RescaleTS(xencoder_t *pEncoder, AVPacket *pPacket, xstream_t *pStream)
{
    XASSERT_RET(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;
    XASSERT(pPacket, XStat_ErrCb(pStatus, "Invalid packet argument"));

    if (pStream == NULL)
    {
        pStream = XStreams_GetByDstIndex(&pEncoder->streams, pPacket->stream_index);
        XASSERT(pStream, XStat_ErrCb(pStatus, "Stream is not found: dst(%d)", pPacket->stream_index));
    }

    if (pEncoder->eTSType == XPTS_RESCALE)
    {
        /* Rescale packet timestamps and reset position */
        AVRational srcTimeBase = pStream->codecInfo.timeBase;
        AVRational dstTimeBase = pStream->pAvStream->time_base;

        av_packet_rescale_ts(pPacket, srcTimeBase, dstTimeBase);
        pPacket->pos = XSTDERR; /* Let FFMPEG decide position */
    }
    else if (pEncoder->eTSType == XPTS_ROUND)
    {
        /* Rescale and round PTS/DTS to the nearest value */
        AVRational srcTimeBase =  pStream->codecInfo.timeBase;
        AVRational dstTimeBase = pStream->pAvStream->time_base;
        enum AVRounding avRound = (enum AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);

        pPacket->pts = av_rescale_q_rnd(pPacket->pts, srcTimeBase, dstTimeBase, avRound);
        pPacket->dts = av_rescale_q_rnd(pPacket->dts, srcTimeBase, dstTimeBase, avRound);
    }
    else if (pEncoder->eTSType == XPTS_CALCULATE)
    {
        /* Calculate PTS based on the elapsed time and clock rate */
        if (!pEncoder->nStartTime) pEncoder->nStartTime = XTime_GetStamp();
        AVRational dstTimeBase = pStream->pAvStream->time_base;

        uint64_t nCurrentTime = XTime_GetStamp();
        uint64_t nElapsedTime = nCurrentTime - pEncoder->nStartTime;
        uint64_t nPTS = (nElapsedTime * dstTimeBase.den) / 1000000;

        pPacket->pts = nPTS;
        pPacket->dts = nPTS;
    }
    else if (pEncoder->eTSType == XPTS_COMPUTE)
    {
        if (pStream->codecInfo.mediaType == AVMEDIA_TYPE_VIDEO)
        {
            /* Calculate the PTS and DTS based on the frame count and time base */
            AVRational srcTimeBase = av_inv_q(pStream->codecInfo.frameRate);
            AVRational dstTimeBase = pStream->pAvStream->time_base;

            pPacket->duration = av_rescale_q(1, srcTimeBase, dstTimeBase);
            pPacket->pts = av_rescale_q(pStream->nPacketCount, srcTimeBase, dstTimeBase);
            pPacket->dts = pPacket->pts;
        }
        else if (pStream->codecInfo.mediaType == AVMEDIA_TYPE_AUDIO)
        {
            /* Calculate the PTS based on the sample rate and time base */
            AVRational srcTimeBase = (AVRational){1, pStream->codecInfo.nSampleRate};
            AVRational dstTimeBase = pStream->pAvStream->time_base;
            int nSamplesPerFrame = pStream->codecInfo.nFrameSize;

            pPacket->duration = av_rescale_q(nSamplesPerFrame, srcTimeBase, dstTimeBase);
            pPacket->pts = av_rescale_q(pStream->nPacketCount * nSamplesPerFrame, srcTimeBase, dstTimeBase);
            pPacket->dts = pPacket->pts;
        }
    }

    return XSTDOK;
}

XSTATUS XEncoder_FixTS(xencoder_t *pEncoder, AVPacket *pPacket, xstream_t *pStream)
{
    XASSERT_RET(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;
    XASSERT(pPacket, XStat_ErrCb(pStatus, "Invalid packet argument"));

    enum AVMediaType eType = pStream->codecInfo.mediaType;
    const char* pType = (eType == AVMEDIA_TYPE_AUDIO) ? "audio" : "video";

    if (pStream == NULL)
    {
        pStream = XStreams_GetByDstIndex(&pEncoder->streams, pPacket->stream_index);
        XASSERT(pStream, XStat_ErrCb(pStatus, "Stream is not found: %d", pPacket->stream_index));
    }

    if (pEncoder->nTSFix &&
       (pStream->nLastPTS >= pPacket->pts ||
        pStream->nLastDTS >= pPacket->dts))
    {
        pPacket->pts = pStream->nLastPTS + pEncoder->nTSFix;
        pPacket->dts = pStream->nLastDTS + pEncoder->nTSFix;

        XStat_InfoCb(pStatus, "Fixed %s TS: PTS(%lld), DTS(%lld)",
            pType, pPacket->pts, pPacket->dts);

        return XSTDOK;
    }

    return XSTDNON;
}

XSTATUS XEncoder_WritePacket(xencoder_t *pEncoder, AVPacket *pPacket)
{
    XASSERT_RET(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    XASSERT(pPacket, XStat_ErrCb(pStatus, "Invalid packet argument"));
    XASSERT(pEncoder->pFmtCtx, XStat_ErrCb(pStatus, "Encoder format context is not init"));
    XASSERT(pEncoder->bOutputOpen, XStat_ErrCb(pStatus, "Output context is not open"));

    xstream_t *pStream = XStreams_GetByDstIndex(&pEncoder->streams, pPacket->stream_index);
    XASSERT(pStream, XStat_ErrCb(pStatus, "Stream is not found: %d", pPacket->stream_index));
    XASSERT(pStream->pAvStream, XStat_ErrCb(pStatus, "Stream is not open: %d", pStream->nDstIndex));

    /* Rescale timestamps and fix non motion PTS/DTS if detected */
    XEncoder_RescaleTS(pEncoder, pPacket, pStream);
    XEncoder_FixTS(pEncoder, pPacket, pStream);

    pStream->nLastPTS = pPacket->pts;
    pStream->nLastDTS = pPacket->dts;

    pStatus->nAVStatus = av_interleaved_write_frame(pEncoder->pFmtCtx, pPacket);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Failed to write packet"));

    pStream->nPacketCount++;
    return XSTDOK;
}

XSTATUS XEncoder_WriteFrame(xencoder_t *pEncoder, AVFrame *pFrame, int nStreamIndex)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    xstream_t *pStream = XStreams_GetByDstIndex(&pEncoder->streams, nStreamIndex);
    XASSERT(pStream, XStat_ErrCb(pStatus, "Stream is not found: dst(%d)", nStreamIndex));
    XASSERT(pStream->bCodecOpen, XStat_ErrCb(pStatus, "Codec is not open: dst(%d)", nStreamIndex));

    AVPacket* pPacket = XStream_GetOrCreatePacket(pStream);
    XASSERT(pPacket, XStat_ErrCb(pStatus, "Failed to allocate packet: %s", strerror(errno)));

    pStatus->nAVStatus = avcodec_send_frame(pStream->pCodecCtx, pFrame);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Failed to send frame to encoder: dst(%d)", nStreamIndex));

    while (pStatus->nAVStatus >= 0)
    {
        pStatus->nAVStatus = avcodec_receive_packet(pStream->pCodecCtx, pPacket);
        if (pStatus->nAVStatus == AVERROR(EAGAIN) || pStatus->nAVStatus == AVERROR_EOF) return XSTDOK;
        XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Failed to encode packet: dst(%d)", nStreamIndex));

        pPacket->stream_index = nStreamIndex;
        int nRetVal = XSTDOK;

        if (pEncoder->packetCallback != NULL)
        {
            nRetVal = pEncoder->packetCallback(pEncoder->pUserCtx, pPacket);
            XASSERT((nRetVal >= 0), XStat_ErrCb(pStatus, "User terminated packet encoding"));
        }

        if (nRetVal > 0)
        {
            XEncoder_WritePacket(pEncoder, pPacket);
            XASSERT_RET((pStatus->nAVStatus >= 0), XSTDERR);
        }

        /* Recycle packet */
        av_packet_unref(pPacket);
    }

    return XSTDOK;
}

XSTATUS XEncoder_WriteFrame2(xencoder_t *pEncoder, AVFrame *pFrame, xframe_params_t *pParams)
{
    XASSERT((pEncoder && pFrame && pParams), XSTDINV);
    XASSERT((pParams->nIndex >= 0), XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    if (pParams->mediaType == AVMEDIA_TYPE_VIDEO)
    {
        enum AVPixelFormat pixFmt = (enum AVPixelFormat)pFrame->format;
        int nHeight = pFrame->height;
        int nWidth = pFrame->width;
        xbool_t bRescale = XFALSE;

        if ((pParams->nWidth > 0 && pParams->nHeight > 0) &&
            (pFrame->width != pParams->nWidth ||
            pFrame->height != pParams->nHeight))
        {
            nWidth = pParams->nWidth;
            nHeight = pParams->nHeight;
            bRescale = XTRUE;
        }
        else if (pFrame->width > pFrame->linesize[0] ||
                pFrame->width / 2 > pFrame->linesize[1] ||
                pFrame->width / 2 > pFrame->linesize[2])
        {
            nWidth = pFrame->width;
            nHeight = pFrame->height;
            bRescale = XTRUE;
        }

        if (pParams->pixFmt != AV_PIX_FMT_NONE &&
            pParams->pixFmt != pixFmt)
        {
            pixFmt = pParams->pixFmt;
            bRescale = XTRUE;
        }

        if (bRescale)
        {
            pParams->pixFmt = pixFmt;
            pParams->nWidth = nWidth;
            pParams->nHeight = nHeight;

            AVFrame *pAvFrame = XFrame_NewScale(pFrame, pParams);
            XASSERT(pAvFrame, xthrow("Failed to scale frame"));
            int64_t nPTS = pAvFrame->pts;

            pStatus->nAVStatus = XEncoder_WriteFrame(pEncoder, pAvFrame, pParams->nIndex);
            XASSERT_CALL((pStatus->nAVStatus > 0), av_frame_free, &pAvFrame, XStat_ErrCb(pStatus,
                "Video encoding failed: pts(%lld), dst(%d)", nPTS, pParams->nIndex));

            av_frame_free(&pAvFrame);
            return XSTDOK;
        }
    }
    else if (pParams->mediaType == AVMEDIA_TYPE_AUDIO)
    {
        enum AVSampleFormat sampleFmt = (enum AVSampleFormat)pFrame->format;
        int nChannels = XFrame_GetChannelCount(pFrame);
        int nSampleRate = pFrame->sample_rate;
        xbool_t bResample = XFALSE;

        if (pParams->nSampleRate > 0 &&
            pParams->nSampleRate != nSampleRate)
        {
            nSampleRate = pParams->nSampleRate;
            bResample = XTRUE;
        }

        if (pParams->sampleFmt != AV_SAMPLE_FMT_NONE &&
            pParams->sampleFmt != sampleFmt)
        {
            sampleFmt = pParams->sampleFmt;
            bResample = XTRUE;
        }

        if (pParams->nChannels > 0 &&
            pParams->nChannels != nChannels)
        {
            nChannels = pParams->nChannels;
            bResample = XTRUE;
        }

        if (bResample)
        {
            pParams->nSampleRate = nSampleRate;
            pParams->sampleFmt = sampleFmt;
            pParams->nChannels = nChannels;

            AVFrame *pAvFrame = XFrame_NewResample(pFrame, pParams);
            XASSERT(pAvFrame, XStat_ErrCb(pStatus, "Failed to resample frame"));
            int64_t nPTS = pAvFrame->pts;

            pStatus->nAVStatus = XEncoder_WriteFrame(pEncoder, pAvFrame, pParams->nIndex);
            XASSERT_CALL((pStatus->nAVStatus > 0), av_frame_free, &pAvFrame, XStat_ErrCb(pStatus,
                "Audio encoding failed: pts(%lld), dst(%d)", nPTS, pParams->nIndex));

            av_frame_free(&pAvFrame);
            return XSTDOK;
        }
    }

    pStatus->nAVStatus = XEncoder_WriteFrame(pEncoder, pFrame, pParams->nIndex);
    XASSERT((pStatus->nAVStatus > 0), XStat_ErrCb(pStatus,
        "Encoding failed: pts(%lld), dst(%d)", pFrame->pts, pParams->nIndex));

    return XSTDOK;
}

XSTATUS XEncoder_WriteFrame3(xencoder_t *pEncoder, AVFrame *pFrame, int nStreamIndex)
{
    XASSERT((pEncoder && pFrame), XSTDINV);
    XASSERT((nStreamIndex >= 0), XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    const xcodec_t *pCodecInfo = XEncoder_GetCodecInfo(pEncoder, nStreamIndex);
    XASSERT(pCodecInfo, XStat_ErrCb(pStatus, "Failed to get output codec info: dst(%d)", nStreamIndex));

    xframe_params_t params;
    XFrame_InitParams(&params, NULL);

    params.nIndex = nStreamIndex;
    params.mediaType = pCodecInfo->mediaType;
    params.status.cb = pEncoder->status.cb;
    params.status.nTypes = pEncoder->status.nTypes;
    params.status.pUserCtx = pEncoder->status.pUserCtx;

    if (params.mediaType == AVMEDIA_TYPE_VIDEO)
    {
        params.scaleFmt = pCodecInfo->scaleFmt;
        params.pixFmt = pCodecInfo->pixFmt;
        params.nWidth = pCodecInfo->nWidth;
        params.nHeight = pCodecInfo->nHeight;
    }
    else if (params.mediaType == AVMEDIA_TYPE_AUDIO)
    {
        params.nSampleRate = pCodecInfo->nSampleRate;
        params.sampleFmt = pCodecInfo->sampleFmt;
        params.nChannels = pCodecInfo->nChannels;
    }

    return XEncoder_WriteFrame2(pEncoder, pFrame, &params);
}

XSTATUS XEncoder_AddChapter(xencoder_t *pEncoder, AVChapter *pChapter)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    XASSERT(pChapter, XStat_ErrCb(pStatus, "Invalid chapter argument"));
    XASSERT(pEncoder->pFmtCtx, XStat_ErrCb(pStatus, "Invalid format context"));

    size_t nChapterTotal = pEncoder->pFmtCtx->nb_chapters + 1;
    size_t nElementSize = sizeof(*pEncoder->pFmtCtx->chapters);

    AVChapter **pTmp = av_realloc_f(pEncoder->pFmtCtx->chapters, nChapterTotal, nElementSize);
    XASSERT(pTmp, XStat_ErrCb(pStatus, "Failed to reallocate output contetxt chapters"));
    pEncoder->pFmtCtx->chapters = pTmp;

    pEncoder->pFmtCtx->chapters[pEncoder->pFmtCtx->nb_chapters++] = pChapter;
    return XSTDOK;
}

XSTATUS XEncoder_AddChapters(xencoder_t *pEncoder, xarray_t *pChapters)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    XASSERT(pChapters, XStat_ErrCb(pStatus, "Invalid chapters argument"));
    XASSERT(pEncoder->pFmtCtx, XStat_ErrCb(pStatus, "Invalid format context"));

    size_t i, nChapterStored = 0, nChapterCount = XArray_Used(pChapters);
    XASSERT(nChapterCount, XStat_ErrCb(pStatus, "Empty chapter array"));

    size_t nChapterTotal = pEncoder->pFmtCtx->nb_chapters + nChapterCount;
    size_t nElementSize = sizeof(*pEncoder->pFmtCtx->chapters);

    AVChapter **pTmp = av_realloc_f(pEncoder->pFmtCtx->chapters, nChapterTotal, nElementSize);
    XASSERT(pTmp, XStat_ErrCb(pStatus, "Failed to reallocate output contetxt chapters"));
    pEncoder->pFmtCtx->chapters = pTmp;

    for (i = 0; i < nChapterCount; i++)
    {
        AVChapter *pChapter = (AVChapter*)XArray_GetData(pChapters, i);
        if (pChapter == NULL) continue;

        pEncoder->pFmtCtx->chapters[pEncoder->pFmtCtx->nb_chapters++] = pChapter;
        nChapterStored++;
    }

    return nChapterStored;
}

XSTATUS XEncoder_AddMeta(xencoder_t *pEncoder, xmeta_t *pMeta)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    XASSERT(pMeta, XStat_ErrCb(pStatus, "Invalid meta argument"));
    XASSERT(pEncoder->pFmtCtx, XStat_ErrCb(pStatus, "Invalid format context"));

    if (pMeta->chapters.nUsed > 0)
    {
        if (XEncoder_AddChapters(pEncoder, &pMeta->chapters) > 0)
            pMeta->chapters.clearCb = NULL; /* Encoder owns chapters now */
    }

    if (pMeta->pData != NULL)
    {
        pEncoder->pFmtCtx->metadata = pMeta->pData;
        pMeta->pData = NULL; /* Encoder owns metadata now */
    }

    return XSTDOK;
}

XSTATUS XEncoder_FlushBuffer(xencoder_t *pEncoder, int nStreamIndex)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    xstream_t *pStream = XStreams_GetByDstIndex(&pEncoder->streams, nStreamIndex);
    XASSERT(pStream, XStat_ErrCb(pStatus, "Stream is not found: dst(%d)", nStreamIndex));

    XASSERT((pStream->pCodecCtx && pStream->bCodecOpen),
        XStat_ErrCb(pStatus, "Codec is not open: dst(%d)", nStreamIndex));

    XStream_FlushBuffers(pStream);
    return XSTDOK;
}

XSTATUS XEncoder_FlushBuffers(xencoder_t *pEncoder)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    size_t i, nCount = XArray_Used(&pEncoder->streams);
    XStat_InfoCb(pStatus, "Flushing streams: count(%d),", nCount);

    for (i = 0; i < pEncoder->streams.nUsed; i++)
    {
        xstream_t *pStream = XStreams_GetByIndex(&pEncoder->streams, i);
        if (pStream != NULL) XStream_FlushBuffers(pStream);
    }

    return XSTDOK;
}

XSTATUS XEncoder_FlushStream(xencoder_t *pEncoder, int nStreamIndex)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    xstream_t *pStream = XStreams_GetByDstIndex(&pEncoder->streams, nStreamIndex);
    XASSERT(pStream, XStat_ErrCb(pStatus, "Stream is not found: dst(%d)", nStreamIndex));

    XASSERT((pStream->pCodecCtx && pStream->bCodecOpen),
        XStat_ErrCb(pStatus, "Codec is not open: dst(%d)", nStreamIndex));

    XEncoder_WriteFrame(pEncoder, NULL, pStream->nDstIndex);
    return XSTDOK;
}

XSTATUS XEncoder_FlushStreams(xencoder_t *pEncoder)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;

    size_t i, nCount = XArray_Used(&pEncoder->streams);
    XStat_InfoCb(pStatus, "Flushing streams: count(%d),", nCount);

    for (i = 0; i < nCount; i++)
    {
        xstream_t *pStream = XStreams_GetByIndex(&pEncoder->streams, i);
        if (!pStream || !pStream->pCodecCtx || !pStream->bCodecOpen) continue;

        XEncoder_WriteFrame(pEncoder, NULL, pStream->nDstIndex);
    }

    return XSTDOK;
}

XSTATUS XEncoder_FinishWrite(xencoder_t *pEncoder, xbool_t bFlush)
{
    XASSERT(pEncoder, XSTDINV);
    xstatus_t *pStatus = &pEncoder->status;
    if (bFlush) XEncoder_FlushStreams(pEncoder);

    /* Write trailer */
    if (pEncoder->pFmtCtx != NULL)
    {
        const char *pFmt = xstrused(pEncoder->sOutFormat) ? pEncoder->sOutFormat : "N/A";
        XStat_InfoCb(pStatus, "Writing trailer: fmt(%s), url(%s)", pFmt, pEncoder->sOutputPath);
        av_write_trailer(pEncoder->pFmtCtx);
    }

    return XSTDOK;
}
