/*!
 *  @file libxmedia/example/xmedia.c
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the video/audio transcoder
 * functionality based on the FFMPEG API and libraries.
 */

#include "stdinc.h"
#include "version.h"
#include "encoder.h"
#include "decoder.h"
#include "frame.h"
#include "meta.h"

extern char *optarg;

typedef struct {
    enum AVSampleFormat sampleFmt;
    enum AVPixelFormat pixelFmt;
    enum AVCodecID videoCodec;
    enum AVCodecID audioCodec;
    xscale_fmt_t scaleFmt;

    char inputFile[XPATH_MAX];
    char inputFmt[XPATH_MAX];
    char outFile[XPATH_MAX];
    char outFmt[XSTR_TINY];

    int nWidth;
    int nHeight;

    AVRational frameRate;
    int nSampleRate;
    int nChannels;

    xpts_ctl_t eTSType;
    size_t nIOBuffSize;
    xbool_t bCustomIO;
    xbool_t bRemux;
    xbool_t bDebug;
    xbool_t bLoop;
    size_t nTSFix;
} xmedia_args_t;

typedef struct {
    xmedia_args_t args;
    xdecoder_t decoder;
    xencoder_t encoder;
    xarray_t streams;
    xmeta_t meta;
    FILE *pFile;
} xtranscoder_t;

static int g_nInterrupted = 0;

static void signal_callback(int sig)
{
    if (sig == SIGINT) printf("\n");
    g_nInterrupted = 1;
}

static void status_cb(void *pUserCtx, xstatus_type_t nType, const char *pStatus)
{
    XASSERT_VOID(pStatus);
    if (nType == XSTATUS_ERROR) xloge("%s", pStatus);
    else if (nType == XSTATUS_DEBUG) xlogd("%s", pStatus);
    else xlogn("%s", pStatus);
}

static void clear_cb(xarray_data_t *pArrData)
{
    XASSERT_VOID_RET(pArrData);
    free(pArrData->pData);
}

#if XMEDIA_AVCODEC_VER_AT_LEAST(60, 31)
static int muxer_cb(void *pCtx, const uint8_t *pData, int nSize)
#else
static int muxer_cb(void *pCtx, uint8_t *pData, int nSize)
#endif
{
    xlogd("Muxer callback: size(%d)", nSize);
    xtranscoder_t *pTransmuxer = (xtranscoder_t*)pCtx;
    XASSERT((pTransmuxer->pFile != NULL), XSTDERR);
    return fwrite(pData, 1, nSize, pTransmuxer->pFile);
}

static int encoder_cb(void *pCtx, AVPacket *pPacket)
{
    xlogd("Encoder callback: stream(%d), size(%d), pts(%lld)",
        pPacket->stream_index, pPacket->size, pPacket->pts);
    return XSTDOK;
}

static int decoder_cb(void *pCtx, AVFrame *pFrame, int nStreamIndex)
{
    xtranscoder_t *pTransmuxer = (xtranscoder_t*)pCtx;
    xencoder_t *pDecoder = (xencoder_t*)&pTransmuxer->decoder;
    xencoder_t *pEncoder = (xencoder_t*)&pTransmuxer->encoder;
    xlogd("Decoder callback: stream(%d), pts(%lld)", nStreamIndex, pFrame->pts);

    xstream_t *pStream = XStreams_GetBySrcIndex(&pDecoder->streams, nStreamIndex);
    XASSERT(pStream, xthrow("Source stream is not found: src(%d)", nStreamIndex));
    XASSERT((pStream->nDstIndex >= 0), xthrow("Output stream is not found: src(%d)", nStreamIndex));

    return XEncoder_WriteFrame3(pEncoder, pFrame, pStream->nDstIndex);
}

void XTranscoder_Init(xtranscoder_t *pTransmuxer)
{
    XArray_Init(&pTransmuxer->streams, NULL, XSTDNON, XFALSE);
    pTransmuxer->streams.clearCb = clear_cb;

    pTransmuxer->args.videoCodec = AV_CODEC_ID_NONE;
    pTransmuxer->args.audioCodec = AV_CODEC_ID_NONE;
    pTransmuxer->args.sampleFmt = AV_SAMPLE_FMT_NONE;
    pTransmuxer->args.pixelFmt = AV_PIX_FMT_NONE;
    pTransmuxer->args.scaleFmt = XSCALE_FMT_ASPECT;

    pTransmuxer->args.nWidth = XSTDNON;
    pTransmuxer->args.nHeight = XSTDNON;

    pTransmuxer->args.nChannels = XSTDERR;
    pTransmuxer->args.nSampleRate = XSTDERR;
    pTransmuxer->args.frameRate.num = XSTDERR;
    pTransmuxer->args.frameRate.den = XSTDERR;

    xstrnul(pTransmuxer->args.inputFile);
    xstrnul(pTransmuxer->args.inputFmt);
    xstrnul(pTransmuxer->args.outFile);
    xstrnul(pTransmuxer->args.outFmt);

    pTransmuxer->args.nIOBuffSize = XSTDNON;
    pTransmuxer->args.bCustomIO = XFALSE;
    pTransmuxer->args.eTSType = XPTS_RESCALE;
    pTransmuxer->args.nTSFix = XSTDNON;
    pTransmuxer->args.bRemux = XFALSE;
    pTransmuxer->args.bDebug = XFALSE;
    pTransmuxer->args.bLoop = XFALSE;

    avdevice_register_all();
    avformat_network_init();

    XDecoder_Init(&pTransmuxer->decoder);
    XEncoder_Init(&pTransmuxer->encoder);
    XMeta_Init(&pTransmuxer->meta);
    pTransmuxer->pFile = NULL;
}

void XTranscoder_Destroy(xtranscoder_t *pTransmuxer)
{
    XEncoder_Destroy(&pTransmuxer->encoder);
    XDecoder_Destroy(&pTransmuxer->decoder);
    XArray_Destroy(&pTransmuxer->streams);
    XMeta_Clear(&pTransmuxer->meta);
    avformat_network_deinit();

    if (pTransmuxer->pFile)
    {
        fclose(pTransmuxer->pFile);
        pTransmuxer->pFile = NULL;
    }
}

xbool_t XTranscoder_InitDecoder(xtranscoder_t *pTransmuxer)
{
    xbool_t bDemuxOnly = pTransmuxer->args.bRemux;
    const char *pInFmt = pTransmuxer->args.inputFmt;
    const char *pInput = pTransmuxer->args.inputFile;
    const char *pFmt = xstrused(pInFmt) ? pInFmt : NULL;

    /* Setup decoder callbacks */
    pTransmuxer->decoder.frameCallback = decoder_cb;
    pTransmuxer->decoder.pUserCtx = pTransmuxer;
    pTransmuxer->decoder.bDemuxOnly = bDemuxOnly;
    pTransmuxer->decoder.status.cb = status_cb;
    pTransmuxer->decoder.status.nTypes = XSTATUS_ALL;

    /* Open input file for decoding */
    XSTATUS nStatus = XDecoder_OpenInput(&pTransmuxer->decoder, pInput, pFmt);
    return nStatus <= 0 ? XFALSE : XTRUE;
}

xbool_t XTranscoder_InitEncoder(xtranscoder_t *pTransmuxer)
{
    xarray_t *pSrcStreams = &pTransmuxer->decoder.streams;
    xarray_t *pDstStreams = &pTransmuxer->encoder.streams;

    size_t i, nStreamCount = XStreams_GetCount(pSrcStreams);
    XASSERT(nStreamCount, xthrowr(XFALSE, "There is no input streams"));

    const char *pOutput = pTransmuxer->args.outFile;
    const char *pOutFmt = pTransmuxer->args.outFmt;
    const char *pFmt = xstrused(pOutFmt) ? pOutFmt : NULL;

    xpts_ctl_t eTSType = pTransmuxer->args.eTSType;
    xbool_t bMuxOnly = pTransmuxer->args.bRemux;
    int nTSFix = (int)pTransmuxer->args.nTSFix;
    AVDictionary *pMuxOpts = NULL;

    /* Setup encoder and muxer callbacks */
    if (pTransmuxer->args.bCustomIO)
        pTransmuxer->encoder.muxerCallback = muxer_cb;

    /* Setup error and info callbacks */
    pTransmuxer->encoder.status.cb = status_cb;
    pTransmuxer->encoder.status.nTypes = XSTATUS_ALL;
    pTransmuxer->encoder.packetCallback = encoder_cb;
    pTransmuxer->encoder.pUserCtx = pTransmuxer;
    pTransmuxer->encoder.bMuxOnly = bMuxOnly;
    pTransmuxer->encoder.eTSType = eTSType;
    pTransmuxer->encoder.nTSFix = nTSFix;

    /* Initialize streams and output format context */
    XSTATUS nStatus = XEncoder_OpenFormat(&pTransmuxer->encoder, pFmt, pOutput);
    if (nStatus <= 0) return XFALSE;

    for (i = 0; i < nStreamCount; i++)
    {
        xcodec_t codecInfo;
        XCodec_Init(&codecInfo);

        xstream_t *pSrcStream = XStreams_GetByIndex(pSrcStreams, i);
        if (pSrcStream == NULL || pSrcStream->nSrcIndex < 0) continue;

        /* Get codec information from input stream */
        nStatus = XStream_CopyCodecInfo(pSrcStream, &codecInfo);
        if (nStatus < 0) return XFALSE;

        /* Discard codec change if only remuxing */
        if (!bMuxOnly)
        {
            /* Setup any required video/audio codec parameters for transcoding */
            if (codecInfo.mediaType == AVMEDIA_TYPE_VIDEO)
            {
                if (pTransmuxer->args.scaleFmt != XSCALE_FMT_NONE)
                    codecInfo.scaleFmt = pTransmuxer->args.scaleFmt;

                if (pTransmuxer->args.videoCodec != AV_CODEC_ID_NONE)
                    codecInfo.codecId = pTransmuxer->args.videoCodec;

                if (pTransmuxer->args.pixelFmt != AV_PIX_FMT_NONE)
                    codecInfo.pixFmt = pTransmuxer->args.pixelFmt;

                if (pTransmuxer->args.nWidth > 0 &&
                    pTransmuxer->args.nHeight > 0)
                {
                    codecInfo.nWidth = pTransmuxer->args.nWidth;
                    codecInfo.nHeight = pTransmuxer->args.nHeight;
                }

                if (pTransmuxer->args.frameRate.num > 0 &&
                    pTransmuxer->args.frameRate.den > 0)
                {
                    codecInfo.frameRate = pTransmuxer->args.frameRate;
                    codecInfo.timeBase = av_inv_q(codecInfo.frameRate);
                }
            }
            else if (codecInfo.mediaType == AVMEDIA_TYPE_AUDIO)
            {
                if (pTransmuxer->args.audioCodec != AV_CODEC_ID_NONE)
                    codecInfo.codecId = pTransmuxer->args.audioCodec;

                if (pTransmuxer->args.sampleFmt != AV_SAMPLE_FMT_NONE)
                    codecInfo.sampleFmt = pTransmuxer->args.sampleFmt;

                if (pTransmuxer->args.nChannels > 0)
                    XCodec_InitChannels(&codecInfo, pTransmuxer->args.nChannels);

                if (pTransmuxer->args.nSampleRate > 0)
                {
                    codecInfo.nSampleRate = pTransmuxer->args.nSampleRate;
                    codecInfo.timeBase = (AVRational){1, codecInfo.nSampleRate};
                }
            }
        }

        /* Open codec for output stream */
        int nDstIndex = XEncoder_OpenStream(&pTransmuxer->encoder, &codecInfo);

        XCodec_Clear(&codecInfo);
        if (nDstIndex < 0) return XFALSE;

        xstream_t *pDstStream = XStreams_GetByDstIndex(pDstStreams, nDstIndex);
        XASSERT(pDstStream, xthrowr(XFALSE, "Failed to get dst stream: %d", nDstIndex));

        /* Stream I/O mapping */
        pDstStream->nSrcIndex = pSrcStream->nSrcIndex;
        pSrcStream->nDstIndex = pDstStream->nDstIndex;
    }

    if (!XStreams_GetCount(pDstStreams))
    {
        xloge("Oputput streams is not initialized");
        return XFALSE;
    }

    /* Open output file handle */
    if (pTransmuxer->args.bCustomIO)
    {
        pTransmuxer->pFile = fopen(pOutput, "wb");
        if (pTransmuxer->pFile == NULL)
        {
            xloge("Failed to open output: %s (%s)", pOutput, strerror(errno));
            return XFALSE;
        }

        /* Initialize muxer */
        if (xstrncmp(pOutFmt, "mp4", 3))
        {
            /* Create a new fragment for each keyframe (for smooth streaming) */
            av_dict_set(&pMuxOpts, "movflags", "frag_keyframe+empty_moov", 0);
        }
        else if (xstrncmp(pOutFmt, "mpegts", 6))
        {
            char sPeriod[XSTR_TINY]; /* Add PAT/PMT for each keyframe */
            snprintf(sPeriod, sizeof(sPeriod), "%d", (INT_MAX / 2) - 1);
            av_dict_set(&pMuxOpts, "sdt_period", sPeriod, 0);
            av_dict_set(&pMuxOpts, "pat_period", sPeriod, 0);
        }
    }

    /* Setup metadta and encoder user input variables */
    XEncoder_AddMeta(&pTransmuxer->encoder, &pTransmuxer->meta);
    pTransmuxer->encoder.nIOBuffSize = pTransmuxer->args.nIOBuffSize;

    /* Open encder IO and setup write callback */
    nStatus = XEncoder_OpenOutput(&pTransmuxer->encoder, pMuxOpts);
    XASSERT((nStatus > 0), xthrowr(XFALSE, "Failed to open output: %s", pOutput));

    return XTRUE;
}

xbool_t XTranscoder_Transcode(xtranscoder_t *pTransmuxer)
{
    /* Allocate AVPacket for decoding */
    AVPacket *pPacket = av_packet_alloc();
    if (pPacket == NULL)
    {
        xloge("Failed to allocate packet: %s", strerror(errno));
        return XFALSE;
    }

    /* Read packet from the input */
    xbool_t bRemux = pTransmuxer->args.bRemux;
    XSTATUS nStatus = XDecoder_ReadPacket(&pTransmuxer->decoder, pPacket);

    while (!g_nInterrupted && nStatus >= 0)
    {
        if (bRemux) XEncoder_WritePacket(&pTransmuxer->encoder, pPacket);
        else XDecoder_DecodePacket(&pTransmuxer->decoder, pPacket);
        av_packet_unref(pPacket); /* Recycle packet */

        nStatus = XDecoder_ReadPacket(&pTransmuxer->decoder, pPacket);
        if (nStatus == AVERROR_EOF && pTransmuxer->args.bLoop)
        {
            int nStream = pPacket->stream_index;
            xlogd("Seeking input stream to the start position: index(%d)", nStream);

            nStatus = XDecoder_Seek(&pTransmuxer->decoder, nStream, 0, AVSEEK_FLAG_BACKWARD);
            if (nStatus < 0) xloge("Failed to seek stream: %d (%d)", nStream, nStatus);
        }
    }

    /* Finish encoding and destroy context */
    XEncoder_FinishWrite(&pTransmuxer->encoder, !bRemux);
    av_packet_free(&pPacket);
    return XSTDOK;
}

void XTranscoder_Usage(const char *pName)
{
    xlog("================================================================");
    xlog(" Transcoder implementation example - %s", XMedia_Version());
    xlog("================================================================");
    xlog("Usage: %s [options]\n", pName);
    xlog("Options are:");
    xlog("  -i <path>            # Input file or sream path (%s*%s)", XSTR_CLR_RED, XSTR_FMT_RESET);
    xlog("  -o <path>            # Output file or srewam path (%s*%s)", XSTR_CLR_RED, XSTR_FMT_RESET);
    xlog("  -e <format>          # Input format name (example: v4l2)");
    xlog("  -f <format>          # Output format name (example: mp4)");
    xlog("  -x <format>          # Video scale format (example: aspect)");
    xlog("  -p <format>          # Video pixel format (example: yuv420p)");
    xlog("  -s <format>          # Audio sample format (example: s16p)");
    xlog("  -k <num:den>         # Video frame rate (example: 90000:3000)");
    xlog("  -q <number>          # Audio sample rate (example: 48000)");
    xlog("  -c <number>          # Audio channel count (example: 2)");
    xlog("  -v <codec>           # Output video codec (example: h264)");
    xlog("  -a <codec>           # Output audio codec (example: mp3)");
    xlog("  -w <width>           # Output video width (example: 1280)");
    xlog("  -h <height>          # Output video height (example: 720)");
    xlog("  -b <bytes>           # IO buffer size (default: 65536)");
    xlog("  -t <type>            # Timestamp calculation type");
    xlog("  -m <path>            # Metadata file path");
    xlog("  -n <number>          # Fix non motion PTS/DTS");
    xlog("  -z                   # Custom output handling");
    xlog("  -l                   # Loop transcoding/remuxing");
    xlog("  -r                   # Remux only");
    xlog("  -d                   # Debug logs");
    xlog("  -u                   # Usage information\n");

    xlog("Video scale formats:");
    xlog("- stretch   %s(Stretch video frames to the given resolution)%s", XSTR_FMT_DIM, XSTR_FMT_RESET);
    xlog("- aspect    %s(Scale video frames and protect aspect ratio)%s\n", XSTR_FMT_DIM, XSTR_FMT_RESET);

    xlog("Timestamp calculation types:");
    xlog("- calculate %s(Calculate TS based on the elapsed time and clock rate)%s", XSTR_FMT_DIM, XSTR_FMT_RESET);
    xlog("- compute   %s(Compute TS based on the sample rate and time base)%s", XSTR_FMT_DIM, XSTR_FMT_RESET);
    xlog("- rescale   %s(Rescale original TS using av_packet_rescale_ts())%s", XSTR_FMT_DIM, XSTR_FMT_RESET);
    xlog("- round     %s(Rescale original TS and round to the nearest value)%s", XSTR_FMT_DIM, XSTR_FMT_RESET);
    xlog("- source    %s(Use original PTS from the source stream)%s\n", XSTR_FMT_DIM, XSTR_FMT_RESET);

    xlog("Metadata file syntax:");
    xlog("%sstart-time|end-time|chapter-name%s", XSTR_FMT_DIM, XSTR_FMT_RESET);
    xlog("%sfield-name|field-string%s\n", XSTR_FMT_DIM, XSTR_FMT_RESET);

    xlog("If the line consists of three sections, it will be parsed");
    xlog("as a chapter, if it consists of two sections as metadata.");
    xlog("hh:mm:ss time format is used for chapter start/end time.\n");

    xlog("Metadata file example:");
    xlog("%s00:00:00|00:00:40|Opening chapter%s", XSTR_FMT_DIM, XSTR_FMT_RESET);
    xlog("%s00:00:40|00:10:32|Another chapter%s", XSTR_FMT_DIM, XSTR_FMT_RESET);
    xlog("%s00:10:32|00:15:00|Final chapter%s", XSTR_FMT_DIM, XSTR_FMT_RESET);
    xlog("%sComment|Created with xmedia%s", XSTR_FMT_DIM, XSTR_FMT_RESET);
    xlog("%sTitle|Example meta%s", XSTR_FMT_DIM, XSTR_FMT_RESET);
    xlog("%sAlbum|Examples%s\n", XSTR_FMT_DIM, XSTR_FMT_RESET);

    xlog("Examples:");
    xlog("%s%s -i file.avi -o encoded.mp4 -f mp4%s", XSTR_FMT_DIM, pName, XSTR_FMT_RESET);
    xlog("%s%s -i file.mp4 -ro remuxed.mp4 -f mp4 -m meta.txt%s\n", XSTR_FMT_DIM, pName, XSTR_FMT_RESET);
}

xpts_ctl_t XTranscoder_GetTSType(const char *pType)
{
    if (!strncmp(pType, "calculate", 9)) return XPTS_CALCULATE;
    else if (!strncmp(pType, "compute", 7)) return XPTS_COMPUTE;
    else if (!strncmp(pType, "rescale", 7)) return XPTS_RESCALE;
    else if (!strncmp(pType, "round", 5)) return XPTS_ROUND;
    else if (!strncmp(pType, "source", 6)) return XPTS_SOURCE;
    return XPTS_INVALID;
}

AVRational XTranscoder_GetFrameRate(const char *pFrameRate)
{
    AVRational frameRate;
    frameRate.num = XSTDERR;
    frameRate.den = XSTDERR;

    if (xstrsrc(pFrameRate, ":") <= 0) return frameRate;
    xarray_t *tokens = xstrsplit(pFrameRate, ":");

    if (tokens->nUsed != 2)
    {
        XArray_Destroy(tokens);
        return frameRate;
    }

    char *pNum = (char*)XArray_GetData(tokens, 0);
    char *pDen = (char*)XArray_GetData(tokens, 1);

    if (!xstrused(pNum) || !xstrused(pDen))
    {
        XArray_Destroy(tokens);
        return frameRate;
    }

    frameRate.num = atoi(pNum);
    frameRate.den = atoi(pDen);

    XArray_Destroy(tokens);
    return frameRate;
}

xbool_t XTranscoder_ParseMeta(xtranscoder_t *pTransmuxer, const char *pInput)
{
    xmeta_t *pMeta = &pTransmuxer->meta;
    size_t nMetaLength = 0;

    char *pMetaRaw = (char*)XPath_Load(pInput, &nMetaLength);
    XASSERT(pMetaRaw, xthrowr(XFALSE, "Failed to parse metadata file: %s", strerror(errno)));

    char *pSavePtr = NULL;
    char *pLine = xstrtok(pMetaRaw, "\n", &pSavePtr);

    while (pLine != NULL)
    {
        xarray_t *pTokens = xstrsplit(pLine, "|");
        pLine = xstrtok(NULL, "\n", &pSavePtr);
        if (pTokens == NULL) continue;

        if (pTokens->nUsed == 3)
        {
            char *pStartTime = (char*)XArray_GetData(pTokens, 0);
            char *pEndTime = (char*)XArray_GetData(pTokens, 1);
            char *pTitle = (char*)XArray_GetData(pTokens, 2);
            XMeta_AddChapterTime(pMeta, pStartTime, pEndTime, pTitle);
        }
        else if (pTokens->nUsed)
        {
            char *pFieldName = (char*)XArray_GetData(pTokens, 0);
            char *pFieldData = (char*)XArray_GetData(pTokens, 1);
            XMeta_AddField(pMeta, pFieldName, pFieldData);
        }

        XArray_Destroy(pTokens);
    }

    free(pMetaRaw);
    return XSTDOK;
}

XSTATUS XTranscoder_ParseArgs(xtranscoder_t *pTransmuxer, int argc, char *argv[])
{
    xmedia_args_t *pArgs = (xmedia_args_t*)&pTransmuxer->args;
    char sFrameRate[XSTR_TINY];
    char sMetaData[XPATH_MAX];
    char sTSType[XSTR_TINY];

    xstrnul(sFrameRate);
    xstrnul(sMetaData);
    xstrnul(sTSType);
    int nChar = 0;

    xbool_t bAudioCodecParsed = XFALSE;
    xbool_t bVideoCodecParsed = XFALSE;
    xbool_t bScaleFormatParsed = XFALSE;
    xbool_t bPixelFormatParsed = XFALSE;
    xbool_t bSampleFormatParsed = XFALSE;

    while ((nChar = getopt(argc, argv, "a:b:c:f:i:e:m:n:o:p:k:q:s:t:w:h:v:x:z1:l1:d1:r1:u1")) != -1)
    {
        switch (nChar)
        {
            case 'i':
                xstrncpy(pArgs->inputFile, sizeof(pArgs->inputFile), optarg);
                break;
            case 'e':
                xstrncpy(pArgs->inputFmt, sizeof(pArgs->inputFmt), optarg);
                break;
            case 'o':
                xstrncpy(pArgs->outFile, sizeof(pArgs->outFile), optarg);
                break;
            case 'f':
                xstrncpy(pArgs->outFmt, sizeof(pArgs->outFmt), optarg);
                break;
            case 'k':
                xstrncpy(sFrameRate, sizeof(sFrameRate), optarg);
                break;
            case 'm':
                xstrncpy(sMetaData, sizeof(sMetaData), optarg);
                break;
            case 't':
                xstrncpy(sTSType, sizeof(sTSType), optarg);
                break;
            case 'a':
                pArgs->audioCodec = XCodec_GetIDByName(optarg);
                bAudioCodecParsed = XTRUE;
                break;
            case 'v':
                pArgs->videoCodec = XCodec_GetIDByName(optarg);
                bVideoCodecParsed = XTRUE;
                break;
            case 'x':
                pArgs->scaleFmt = XFrame_GetScaleFmt(optarg);
                bScaleFormatParsed = XTRUE;
                break;
            case 'p':
                pArgs->pixelFmt = av_get_pix_fmt(optarg);
                bPixelFormatParsed = XTRUE;
                break;
            case 's':
                pArgs->sampleFmt = av_get_sample_fmt(optarg);
                bSampleFormatParsed = XTRUE;
                break;
            case 'q':
                pArgs->nSampleRate = atoi(optarg);
                break;
            case 'c':
                pArgs->nChannels = atoi(optarg);
                break;
            case 'b':
                pArgs->nIOBuffSize = atoi(optarg);
                break;
            case 'w':
                pArgs->nWidth = atoi(optarg);
                break;
            case 'h':
                pArgs->nHeight = atoi(optarg);
                break;
            case 'n':
                pArgs->nTSFix = atoi(optarg);
                break;
            case 'z':
                pArgs->bCustomIO = XTRUE;
                break;
            case 'r':
                pArgs->bRemux = XTRUE;
                break;
            case 'd':
                pArgs->bDebug = XTRUE;
                break;
            case 'l':
                pArgs->bLoop = XTRUE;
                break;
            case 'u':
                return XSTDNON;
            default:
                return XSTDERR;
        }
    }

    if (!xstrused(pArgs->inputFile))
    {
        xloge("Required input file argument");
        return XSTDERR;
    }

    if (!xstrused(pArgs->outFile))
    {
        xloge("Required output file argument");
        return XSTDERR;
    }

    if (bAudioCodecParsed && pArgs->audioCodec == AV_CODEC_ID_NONE)
    {
        xloge("Audio codec is not found");
        return XSTDERR;
    }

    if (bVideoCodecParsed && pArgs->videoCodec == AV_CODEC_ID_NONE)
    {
        xloge("Video codec is not found");
        return XSTDERR;
    }

    if (bPixelFormatParsed && pArgs->pixelFmt == AV_PIX_FMT_NONE)
    {
        xloge("Video pixel format is not found");
        return XSTDERR;
    }

    if (bSampleFormatParsed && pArgs->sampleFmt == AV_SAMPLE_FMT_NONE)
    {
        xloge("Audio sample format is not found");
        return XSTDERR;
    }

    if (bScaleFormatParsed && pArgs->scaleFmt == XSCALE_FMT_NONE)
    {
        xloge("Video scale format is not found");
        return XSTDERR;
    }

    if (xstrused(sTSType))
    {
        pTransmuxer->args.eTSType = XTranscoder_GetTSType(sTSType);
        if (pTransmuxer->args.eTSType == XPTS_INVALID)
        {
            xloge("Invalid PTS rescaling type: %s", sTSType);
            return XSTDERR;
        }
    }

    if (xstrused(sFrameRate))
    {
        pTransmuxer->args.frameRate = XTranscoder_GetFrameRate(sFrameRate);
        if (pTransmuxer->args.frameRate.num <= 0 ||
            pTransmuxer->args.frameRate.den <= 0)
        {
            xloge("Invalid video frame rate: %s", sFrameRate);
            return XSTDERR;
        }
    }

    if (xstrused(sMetaData) &&
        !XTranscoder_ParseMeta(pTransmuxer, sMetaData))
            return XSTDERR;

    if (pArgs->bDebug)
    {
        av_log_set_level(AV_LOG_VERBOSE);
        xlog_timing(XLOG_TIME);
        xlog_enable(XLOG_ALL);
        xlog_indent(XTRUE);
    }

    return XSTDOK;
}

int main(int argc, char *argv[])
{
    /* Initialize logger */
    xlog_defaults();
    xlog_enable(XLOG_INFO);

    /* Initialize FFMPEG logger */
    av_log_set_level(AV_LOG_WARNING);
    XSTATUS nStatus = XSTDOK;

    int nSignals[2];
    nSignals[0] = SIGTERM;
    nSignals[1] = SIGINT;
    XSig_Register(nSignals, 2, signal_callback);

    /* Initialize transcoder */
    xtranscoder_t transcoder;
    XTranscoder_Init(&transcoder);

    /* Parse arguments */
    nStatus = XTranscoder_ParseArgs(&transcoder, argc, argv);
    if (!nStatus)
    {
        XTranscoder_Usage(argv[0]);
        return XSTDOK;
    }
    else if (nStatus < 0)
    {
        xlogi("Run %s with -u for usage info", argv[0]);
        return XSTDERR;
    }

    /* Init encoder/decoder and start transcoding */
    if (!XTranscoder_InitDecoder(&transcoder) ||
        !XTranscoder_InitEncoder(&transcoder) ||
        !XTranscoder_Transcode(&transcoder)) nStatus = XSTDERR;

    /* Cleanup everything */
    XTranscoder_Destroy(&transcoder);
    return nStatus;
}