/*!
 *  @file libxmedia/src/meta.h
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the video/audio metadata
 * functionality based on the FFMPEG API and libraries.
 */

#ifndef __XMEDIA_META_H__
#define __XMEDIA_META_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdinc.h"
#include "status.h"

/* Default timebase used in metadata */
#define XMETA_TIMEBASE  (AVRational){1,10000}

typedef struct xmeta_ {
    AVDictionary *pData;
    xarray_t chapters;
    xstatus_t status;
} xmeta_t;

XSTATUS XChapter_RescaleTiming(AVChapter *pChapter, size_t nStartSec, size_t nDuration);
AVChapter* XChapter_FromSeconds(uint32_t nID, size_t nStartSec, size_t nEndSec, const char *pTitle);
AVChapter* XChapter_FromTime(uint32_t nID, const char *pStartTime, const char *pEndTime, const char *pTitle);
AVChapter* XChapter_Create(uint32_t nID, AVRational timeBase, int64_t nStart, int64_t nEnd, const char *pTitle);
void XChapter_Destroy(AVChapter *pChapter);

void XMeta_Init(xmeta_t *pMeta);
void XMeta_Clear(xmeta_t *pMeta);
XSTATUS XMeta_AddField(xmeta_t *pMeta, const char *pName, const char *pData);
XSTATUS XMeta_AddChapter(xmeta_t *pMeta, AVRational timeBase, int64_t nStart, int64_t nEnd, const char *pTitle);
XSTATUS XMeta_AddChapterTime(xmeta_t *pMeta, const char *pStartTime, const char *pEndTime, const char *pTitle);
XSTATUS XMeta_AddChapterSec(xmeta_t *pMeta, size_t nStartSec, size_t nEndSec, const char *pTitle);

#ifdef __cplusplus
}
#endif

#endif /* __XMEDIA_META_H__ */
