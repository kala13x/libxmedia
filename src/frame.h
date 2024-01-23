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

    /* General parameters */
    enum AVMediaType mediaType;
    xstatus_t status;
    int64_t nPTS;
    int nIndex;
} xframe_params_t;

xscale_fmt_t XFrame_GetScaleFmt(const char* pFmtName);
int XFrame_GetChannelCount(AVFrame *pFrame);

void XFrame_InitParams(xframe_params_t *pFrame, xframe_params_t *pParent);
void XFrame_InitChannels(AVFrame *pFrame, int nChannels);
void XFrame_InitFrame(AVFrame* pFrame);

AVFrame* XFrame_FromOpus(uint8_t *pOpusBuff, size_t nSize, xframe_params_t *pParams);
AVFrame* XFrame_FromYUV(uint8_t *pYUVBuff, size_t nSize, xframe_params_t *pParams);

XSTATUS XFrame_OverlayYUV(AVFrame *pDstFrame, AVFrame *pSrcFrame, xframe_params_t *pParams);
XSTATUS XFrame_BlackYUV(AVFrame *pFrameOut, xframe_params_t *pParams);

XSTATUS XFrame_Resample(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams);
XSTATUS XFrame_Stretch(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams);
XSTATUS XFrame_Aspect(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams);
XSTATUS XFrame_Scale(AVFrame *pFrameOut, AVFrame *pFrameIn, xframe_params_t *pParams);

AVFrame* XFrame_NewResample(AVFrame *pFrameIn, xframe_params_t *pParams);
AVFrame* XFrame_NewStretch(AVFrame *pFrameIn, xframe_params_t *pParams);
AVFrame* XFrame_NewAspect(AVFrame *pFrameIn, xframe_params_t *pParams);
AVFrame* XFrame_NewScale(AVFrame *pFrameIn, xframe_params_t *pParams);
AVFrame* XFrame_NewBlackYUV(xframe_params_t *pParams);

#ifdef __cplusplus
}
#endif

#endif /* __XMEDIA_FRAME_H__ */
