/*!
 *  @file libxmedia/src/meta.c
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the video/audio metadata
 * functionality based on the FFMPEG API and libraries.
 */

#include "meta.h"

XSTATUS XChapter_RescaleTiming(AVChapter *pChapter, size_t nStartSec, size_t nEndSec)
{
    XASSERT(pChapter, XSTDINV);
    pChapter->start = av_rescale_q(nStartSec * AV_TIME_BASE, AV_TIME_BASE_Q, pChapter->time_base);
    pChapter->end = av_rescale_q(nEndSec * AV_TIME_BASE, AV_TIME_BASE_Q, pChapter->time_base) - 1;
    return XSTDOK;
}

void XChapter_Destroy(AVChapter *pChapter)
{
    XASSERT_VOID_RET(pChapter);
    av_dict_free(&pChapter->metadata);
    pChapter->metadata = NULL;
    av_freep(&pChapter);
}

AVChapter* XChapter_Create(uint32_t nID, AVRational timeBase, int64_t nStart, int64_t nEnd, const char *pTitle)
{
    AVChapter *pChapter = av_mallocz(sizeof(AVChapter));
    XASSERT(pChapter, NULL);

    pChapter->time_base = timeBase;
    pChapter->start = nStart;
    pChapter->end = nEnd;
    pChapter->id = nID;
    pChapter->metadata = NULL;

    if (pTitle != NULL)
        av_dict_set(&pChapter->metadata, "title", pTitle, 0);

    return pChapter;
}

AVChapter* XChapter_FromTime(uint32_t nID, const char *pStartTime, const char *pEndTime, const char *pTitle)
{
    int nStartHours = 0, nStartMinutes = 0, nStartSeconds = 0;
    int nEndHours = 0, nEndMinutes = 0, nEndSeconds = 0;

    int nStartCount = sscanf(pStartTime, "%02d:%02d:%02d", &nStartHours, &nStartMinutes, &nStartSeconds);
    int nEndCount = sscanf(pStartTime, "%02d:%02d:%02d", &nEndHours, &nEndMinutes, &nEndSeconds);
    XASSERT((nStartCount == 3 && nEndCount == 3), NULL);

    AVChapter* pChapter = XChapter_Create(nID, XMETA_TIMEBASE, 0, 0, pTitle);
    XASSERT(pChapter, NULL);

    int nStartSec = nStartHours * 3600 + nStartMinutes * 60 + nStartSeconds;
    int nEndSec = nEndHours * 3600 + nEndMinutes * 60 + nEndSeconds;
    XChapter_RescaleTiming(pChapter, nStartSec, nEndSec);

    return pChapter;
}

AVChapter* XChapter_FromSeconds(uint32_t nID, size_t nStartSec, size_t nEndSec, const char *pTitle)
{
    AVChapter* pChapter = XChapter_Create(nID, XMETA_TIMEBASE, 0, 0, pTitle);
    XASSERT((XChapter_RescaleTiming(pChapter, nStartSec, nEndSec) > 0), NULL);
    return pChapter;
}

void XMeta_ClearCb(xarray_data_t *pArrData)
{
    XASSERT_VOID_RET((pArrData && pArrData->pData));
    XChapter_Destroy((AVChapter*)pArrData->pData);
}

void XMeta_Init(xmeta_t *pMeta)
{
    XASSERT_VOID(pMeta);

    XStat_Init(&pMeta->status, XSTDNON, NULL, NULL);
    XArray_Init(&pMeta->chapters, 0, XFALSE);

    pMeta->chapters.clearCb = XMeta_ClearCb;
    pMeta->pData = NULL;
}

void XMeta_Clear(xmeta_t *pMeta)
{
    XASSERT_VOID(pMeta);
    XArray_Destroy(&pMeta->chapters);

    if (pMeta->pData != NULL)
    {
        av_dict_free(&pMeta->pData);
        pMeta->pData = NULL;
    }
}

XSTATUS XMeta_AddField(xmeta_t *pMeta, const char *pName, const char *pData)
{
    XASSERT(pMeta, XSTDINV);
    xstatus_t *pStatus = &pMeta->status;

    XASSERT(pName, XStat_ErrCb(pStatus, "Invalid name argument"));
    XASSERT(pData, XStat_ErrCb(pStatus, "Invalid data argument"));

    pStatus->nAVStatus = av_dict_set(&pMeta->pData, pName, pData, 0);
    XASSERT((pStatus->nAVStatus > 0), XStat_ErrCb(pStatus, "Failed to add meta field"));

    return XSTDOK;
}

XSTATUS XMeta_AddChapter(xmeta_t *pMeta, AVRational timeBase, int64_t nStart, int64_t nEnd, const char *pTitle)
{
    XASSERT(pMeta, XSTDINV);
    xstatus_t *pStatus = &pMeta->status;
    const char *pName = pTitle ? pTitle : "NULL";

    uint32_t nID = (uint32_t)XArray_Used(&pMeta->chapters) + (uint32_t)1;
    AVChapter *pChapter = XChapter_Create(nID, timeBase, nStart, nEnd, pTitle);
    XASSERT(pChapter, XStat_ErrCb(pStatus, "Failed to create chapter: title(%s)", pName));

    XSTATUS nStatus = XArray_AddData(&pMeta->chapters, pChapter, 0);
    XASSERT_CALL((nStatus > 0), XChapter_Destroy, pChapter,
        XStat_ErrCb(pStatus, "Failed to store chapter: title(%s)", pName));

    return XSTDOK;
}


XSTATUS XMeta_AddChapterTime(xmeta_t *pMeta, const char *pStartTime, const char *pEndTime, const char *pTitle)
{
    XASSERT(pMeta, XSTDINV);
    xstatus_t *pStatus = &pMeta->status;
    const char *pName = pTitle ? pTitle : "NULL";

    uint32_t nID = (uint32_t)XArray_Used(&pMeta->chapters) + (uint32_t)1;
    AVChapter *pChapter = XChapter_FromTime(nID, pStartTime, pEndTime, pTitle);
    XASSERT(pChapter, XStat_ErrCb(pStatus, "Failed to create chapter: title(%s)", pName));

    XSTATUS nStatus = XArray_AddData(&pMeta->chapters, pChapter, 0);
    XASSERT_CALL((nStatus > 0), XChapter_Destroy, pChapter,
        XStat_ErrCb(pStatus, "Failed to store chapter: title(%s)", pName));

    return XSTDOK;
}

XSTATUS XMeta_AddChapterSec(xmeta_t *pMeta, size_t nStartSec, size_t nEndSec, const char *pTitle)
{
    XASSERT(pMeta, XSTDINV);
    xstatus_t *pStatus = &pMeta->status;
    const char *pName = pTitle ? pTitle : "NULL";

    uint32_t nID = (uint32_t)XArray_Used(&pMeta->chapters) + (uint32_t)1;
    AVChapter *pChapter = XChapter_FromSeconds(nID, nStartSec, nEndSec, pTitle);
    XASSERT(pChapter, XStat_ErrCb(pStatus, "Failed to create chapter: title(%s)", pName));

    XSTATUS nStatus = XArray_AddData(&pMeta->chapters, pChapter, 0);
    XASSERT_CALL((nStatus > 0), XChapter_Destroy, pChapter,
        XStat_ErrCb(pStatus, "Failed to store chapter: title(%s)", pName));

    return XSTDOK;
}