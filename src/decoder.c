/*!
 *  @file libxmedia/src/decoder.c
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the video/audio decoding
 * functionality based on the FFMPEG API and libraries.
 */

#include "decoder.h"

void XDecoder_Init(xdecoder_t *pDecoder)
{
    XASSERT_VOID(pDecoder);
    XStreams_Init(&pDecoder->streams);
    XStat_Init(&pDecoder->status, XSTDNON, NULL, NULL);

    pDecoder->pDemuxOpts = NULL;
    pDecoder->bDemuxOnly = XFALSE;
    pDecoder->bHaveInput = XFALSE;

    pDecoder->statusCallback = NULL;
    pDecoder->errorCallback = NULL;
    pDecoder->frameCallback = NULL;
    pDecoder->pUserCtx = NULL;
    pDecoder->pFmtCtx = NULL;
}

void XDecoder_Destroy(xdecoder_t *pDecoder)
{
    XASSERT_VOID(pDecoder);
    XStreams_Destroy(&pDecoder->streams);

    if (pDecoder->pDemuxOpts != NULL)
    {
        av_dict_free(&pDecoder->pDemuxOpts);
        pDecoder->pDemuxOpts = NULL;
    }

    if (pDecoder->pFmtCtx != NULL)
    {
        if (pDecoder->bHaveInput) avformat_close_input(&pDecoder->pFmtCtx);
        else avformat_free_context(pDecoder->pFmtCtx);

        pDecoder->bHaveInput = XFALSE;
        pDecoder->pFmtCtx = NULL;
    }
}

XSTATUS XDecoder_OpenCodec(xdecoder_t *pDecoder, xcodec_t *pCodec)
{
    XASSERT(pDecoder, XSTDINV);
    xstatus_t *pStatus = &pDecoder->status;

    XASSERT(pCodec, XStat_ErrCb(pStatus, "Invalid codec argument"));
    const AVCodec *pAvCodec = avcodec_find_decoder(pCodec->codecId);
    XASSERT(pAvCodec, XStat_ErrCb(pStatus, "Codec is not found: %d", (int)pCodec->codecId));

    /* Create unique stream index */
    int nStreamIndex = (int)XArray_Used(&pDecoder->streams);
    while (XStreams_GetBySrcIndex(&pDecoder->streams, nStreamIndex)) nStreamIndex++;

    xstream_t *pStream = XStreams_NewStream(&pDecoder->streams);
    XASSERT(pStream, XStat_ErrCb(pStatus, "Failed to create new stream: src(%d)", nStreamIndex));

    pStream->pCodecCtx = avcodec_alloc_context3(pAvCodec);
    XASSERT(pStream->pCodecCtx, XStat_ErrCb(pStatus, "Failed to alloc decoder context: src(%d)", nStreamIndex));

    XSTATUS nStatus = XCodec_ApplyToAVCodec(pCodec, pStream->pCodecCtx);
    XASSERT((nStatus == XSTDOK), XStat_ErrCb(pStatus, "Failed to apply codec to context: src(%d)", nStreamIndex));

    pStatus->nAVStatus = avcodec_open2(pStream->pCodecCtx, pAvCodec, NULL);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Failed to open decoder: src(%d)", nStreamIndex));

    XCodec_GetFromAVCodec(&pStream->codecInfo, pStream->pCodecCtx);
    pStream->bCodecOpen = XTRUE;

    char sCodecId[XSTR_TINY];
    XCodec_GetNameByID(sCodecId, sizeof(sCodecId), pStream->codecInfo.codecId);

    XStat_InfoCb(pStatus, "Decoding codec: type(%d), tb(%d.%d), name(%s), src(%d)",
        (int)pStream->codecInfo.mediaType, pStream->pCodecCtx->time_base.num,
        pStream->pCodecCtx->time_base.den, sCodecId, nStreamIndex);

    pStream->nSrcIndex = nStreamIndex;
    pStream->bCodecOpen = XTRUE;
    return nStreamIndex;
}

XSTATUS XDecoder_OpenInput(xdecoder_t *pDecoder, const char *pInput, const char *pInputFmt)
{
    XASSERT(pDecoder, XSTDINV);
    xstatus_t *pStatus = &pDecoder->status;
    XASSERT(pInput, XStat_ErrCb(pStatus, "Invalid input argument"));

#ifdef XCODEC_USE_NEW_FIFO
    const AVInputFormat *pAvInFmt = pInputFmt ? av_find_input_format(pInputFmt) : NULL;
#else
    AVInputFormat *pAvInFmt = pInputFmt ? av_find_input_format(pInputFmt) : NULL;
#endif

    pStatus->nAVStatus = avformat_open_input(&pDecoder->pFmtCtx, pInput, pAvInFmt, &pDecoder->pDemuxOpts);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Cannot open input: %s", pInput));

    pStatus->nAVStatus = avformat_find_stream_info(pDecoder->pFmtCtx, NULL);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Cannot find stream info: %s", pInput));

    pDecoder->bHaveInput = XTRUE;
    unsigned int i;

    for (i = 0; i < pDecoder->pFmtCtx->nb_streams; i++)
    {
        xstream_t *pStream = XStreams_GetBySrcIndex(&pDecoder->streams, i);
        XASSERT((pStream == NULL), XStat_ErrCb(pStatus, "Stream already exists: src(%u)", i));

        char sCodecIdStr[XSTR_TINY];
        AVStream *pAvStream = pDecoder->pFmtCtx->streams[i];
        enum AVCodecID codecId = pAvStream->codecpar->codec_id;
        XCodec_GetNameByID(sCodecIdStr, sizeof(sCodecIdStr), codecId);

        if (pAvStream->codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            pAvStream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
        {
            XStat_InfoCb(pStatus, "Skipping stream: type(%s), codec(%s), src(%d)",
                av_get_media_type_string(pAvStream->codecpar->codec_type), sCodecIdStr, i);

            pAvStream->discard = AVDISCARD_ALL;
            continue;
        }

        pStream = XStreams_NewStream(&pDecoder->streams);
        XASSERT(pStream, XStat_ErrCb(pStatus, "Failed to create new stream: src(%u)", i));

        char sCodecStr[XSTR_MID];
        XCodec_GetFromAVStream(&pStream->codecInfo, pAvStream);
        XCodec_DumpStr(&pStream->codecInfo, sCodecStr, sizeof(sCodecStr));

        pStream->nSrcIndex = pAvStream->index;
        pStream->pAvStream = pAvStream;

        if (pDecoder->bDemuxOnly)
        {
            XStat_InfoCb(pStatus, "Demuxing stream: %s, src(%d)", sCodecStr, i);
            return XSTDOK;
        }

        const AVCodec *pDecCodec = avcodec_find_decoder(pAvStream->codecpar->codec_id);
        XASSERT(pDecCodec, XStat_ErrCb(pStatus, "Failed to find decoder: id(%d), src(%u)",
            (int)pAvStream->codecpar->codec_id, i));

        pStream->pCodecCtx = avcodec_alloc_context3(pDecCodec);
        XASSERT(pStream->pCodecCtx, XStat_ErrCb(pStatus, "Failed to alloc decoder context: src(%u)", i));

        pStatus->nAVStatus = avcodec_parameters_to_context(pStream->pCodecCtx, pAvStream->codecpar);
        XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Failed to copy codec parameters: src(%u)", i));

        if (pStream->pCodecCtx->codec_type == AVMEDIA_TYPE_VIDEO)
            pStream->pCodecCtx->framerate = av_guess_frame_rate(pDecoder->pFmtCtx, pAvStream, NULL);

        if (pStream->pCodecCtx->codec_type == AVMEDIA_TYPE_VIDEO ||
            pStream->pCodecCtx->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            pStatus->nAVStatus = avcodec_open2(pStream->pCodecCtx, pDecCodec, NULL);
            XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus, "Failed to open decoder: src(%u)", i));
            pStream->bCodecOpen = XTRUE;
        }

        XStat_InfoCb(pStatus, "Decoding stream: %s, src(%d)", sCodecStr, i);
    }
  
    return XSTDOK;
}

XSTATUS XDecoder_Seek(xdecoder_t *pDecoder, int nStream, int64_t nTS, int nFlags)
{
    XASSERT(pDecoder, XSTDINV);
    xstatus_t *pStatus = &pDecoder->status;

    XASSERT(pDecoder->bHaveInput, XStat_ErrCb(pStatus, "Input format is not open"));
    pStatus->nAVStatus = av_seek_frame(pDecoder->pFmtCtx, nStream, nTS, nFlags);
    return pStatus->nAVStatus;
}

XSTATUS XDecoder_ReadPacket(xdecoder_t *pDecoder, AVPacket *pPacket)
{
    XASSERT(pDecoder, XSTDINV);
    xstatus_t *pStatus = &pDecoder->status;

    XASSERT(pPacket, XStat_ErrCb(pStatus, "Invalid packet argument"));
    XASSERT(pDecoder->bHaveInput, XStat_ErrCb(pStatus, "Input format is not open"));

    pStatus->nAVStatus = av_read_frame(pDecoder->pFmtCtx, pPacket);
    return pStatus->nAVStatus;
}

XSTATUS XDecoder_DecodePacket(xdecoder_t *pDecoder, AVPacket *pPacket)
{
    XASSERT(pDecoder, XSTDINV);
    xstatus_t *pStatus = &pDecoder->status;

    XASSERT(pPacket, XStat_ErrCb(pStatus, "Invalid packet argument"));
    XASSERT(pDecoder->frameCallback, XStat_ErrCb(pStatus, "Decoder frame callback is not set"));

    xstream_t *pStream = XStreams_GetBySrcIndex(&pDecoder->streams, pPacket->stream_index);
    XASSERT(pStream, XStat_ErrCb(pStatus, "Stream is not found: src(%d)", pPacket->stream_index));
    XASSERT(pStream->bCodecOpen, XStat_ErrCb(pStatus, "Codec is not open: src(%d)", pStream->nSrcIndex));

    AVFrame *pFrame = XStream_GetOrCreateFrame(pStream);
    XASSERT(pFrame, XStat_ErrCb(pStatus, "Failed to alloc frame: src(%d)", pStream->nSrcIndex));

    pFrame->pts = pPacket->pts;
    pFrame->pkt_dts = pPacket->dts;

    pStatus->nAVStatus = avcodec_send_packet(pStream->pCodecCtx, pPacket);
    XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus,
        "Failed to decode packet: src(%d)", pStream->nSrcIndex));

    while (pStatus->nAVStatus >= 0)
    {
        pStatus->nAVStatus = avcodec_receive_frame(pStream->pCodecCtx, pFrame);
        if (pStatus->nAVStatus == AVERROR(EAGAIN) ||
            pStatus->nAVStatus == AVERROR_EOF) return XSTDOK;

        XASSERT((pStatus->nAVStatus >= 0), XStat_ErrCb(pStatus,
            "Failed to receive frame: src(%d)", pStream->nSrcIndex));

        pStatus->nAVStatus = pDecoder->frameCallback(pDecoder->pUserCtx, pFrame, pStream->nSrcIndex);
        av_frame_unref(pFrame);
    }

    return pStatus->nAVStatus;
}

AVPacket* XDecoder_CreatePacket(xdecoder_t *pDecoder, uint8_t *pData, size_t nSize)
{
    XASSERT(pDecoder, NULL);
    xstatus_t *pStatus = &pDecoder->status;

    XASSERT(pData, XStat_ErrPtr(pStatus, "Invalid data argument"));
    XASSERT(nSize, XStat_ErrPtr(pStatus, "Invalid size argument"));

    AVPacket *pPacket = av_packet_alloc();
    XASSERT(pPacket, XStat_ErrPtr(pStatus, "Failed to allocate AVPacket"));

    pPacket->data = pData;
    pPacket->size = (int)nSize;
    pPacket->stream_index = 0;

    return pPacket;
}

const xcodec_t* XDecoder_GetCodecInfo(xdecoder_t* pDecoder, int nStream)
{
    XASSERT(pDecoder, NULL);
    xstatus_t *pStatus = &pDecoder->status;

    xstream_t *pStream = XStreams_GetBySrcIndex(&pDecoder->streams, nStream);
    if (pStream == NULL)
    {
        XStat_ErrCb(pStatus, "Stream is not found: src(%d)", nStream);
        return NULL;
    }

    return XStream_GetCodecInfo(pStream);
}

XSTATUS XDecoder_CopyCodecInfo(xdecoder_t* pDecoder, xcodec_t *pCodecInfo, int nStream)
{
    XASSERT(pDecoder, XSTDINV);
    xstatus_t *pStatus = &pDecoder->status;
    XASSERT(pCodecInfo, XStat_ErrCb(pStatus, "Invalid codec info argument"));

    xstream_t *pStream = XStreams_GetBySrcIndex(&pDecoder->streams, nStream);
    XASSERT(pStream, XStat_ErrCb(pStatus, "Stream is not found: src(%d)", nStream));
    return XStream_CopyCodecInfo(pStream, pCodecInfo);
}