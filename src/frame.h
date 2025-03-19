/*!
 *  @file libxmedia/src/frame.h
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the video/audio frame
 * functionality based on the FFMPEG API and libraries.
 */

#ifndef __XMEDIA_FRAME_H__
#define __XMEDIA_FRAME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdinc.h"
#include "status.h"

typedef enum {
    XSCALE_FMT_NONE,
    XSCALE_FMT_ASPECT,
    XSCALE_FMT_STRETCH,
} xscale_fmt_t;

typedef struct xframe_yuv_ {
    uint8_t y;
    uint8_t u;
    uint8_t v;
} xframe_yuv_t;

typedef struct xframe_params_ {
    /* Audio parameters */
    enum AVSampleFormat sampleFmt;
    int nSampleRate;
    int nChannels;

    /* Video parameters */
    enum AVPixelFormat pixFmt;
    xscale_fmt_t scaleFmt;
    int nWidth;
    int nHeight;
    int nX;
    int nY;

    /* General parameters */
    char source[XLINE_MAX];
    char color[XSTR_MICRO];
    enum AVMediaType mediaType;
    xstatus_t status;
    int64_t nPTS;
    int nIndex;
} xframe_params_t;

void XFrame_RGBtoYUV(xframe_yuv_t *pYUV, uint8_t r, uint8_t g, uint8_t b);
void XFrame_ColorToYUV(xframe_yuv_t *pYUV, const char *pColorName);

xscale_fmt_t XFrame_GetScaleFmt(const char* pFmtName);
int XFrame_GetChannelCount(AVFrame *pFrame);

XSTATUS XFrame_CopyParams(xframe_params_t *pDstParams, xframe_params_t *pSrcParams);
void XFrame_InitParams(xframe_params_t *pFrame, xframe_params_t *pParent);
void XFrame_InitChannels(AVFrame *pFrame, int nChannels);
void XFrame_InitFrame(AVFrame* pFrame);

AVFrame* XFrame_FromFile(xframe_params_t *pParams, const char *pPath);
AVFrame* XFrame_FromOpus(uint8_t *pOpusBuff, size_t nSize, xframe_params_t *pParams);
AVFrame* XFrame_FromYUV(uint8_t *pYUVBuff, size_t nSize, xframe_params_t *pParams);

XSTATUS XFrame_SaveToJPEG(AVFrame *pFrameIn, const char *pDstPath, xframe_params_t *pParams);
XSTATUS XFrame_GenerateYUV(AVFrame *pFrameOut, xframe_params_t *pParams);
XSTATUS XFrame_ConvertToYUV(AVFrame* pFrameOut, const AVFrame* pFrameIn, xframe_params_t *pParams);
XSTATUS XFrame_OverlayYUV(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams);
XSTATUS XFrame_BorderYUV(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams);
XSTATUS XFrame_Resample(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams);
XSTATUS XFrame_Stretch(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams);
XSTATUS XFrame_Aspect(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams);
XSTATUS XFrame_Scale(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams);
XSTATUS XFrame_Crop(AVFrame* pFrameOut, AVFrame* pFrameIn, xframe_params_t *pParams);
XSTATUS XFrame_Border(AVFrame *pFrameOut, xframe_params_t *pParams);

AVFrame* XFrame_NewResample(AVFrame *pFrameIn, xframe_params_t *pParams);
AVFrame* XFrame_NewStretch(AVFrame *pFrameIn, xframe_params_t *pParams);
AVFrame* XFrame_NewAspect(AVFrame *pFrameIn, xframe_params_t *pParams);
AVFrame* XFrame_NewScale(AVFrame *pFrameIn, xframe_params_t *pParams);
AVFrame* XFrame_NewCrop(AVFrame *pFrameIn, xframe_params_t *pParams);
AVFrame* XFrame_NewYUV(xframe_params_t *pParams);

#ifdef __cplusplus
}
#endif

#endif /* __XMEDIA_FRAME_H__ */
