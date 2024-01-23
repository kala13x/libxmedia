/*!
 *  @file libxmedia/src/status.c
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the status callbacks.
 */

#include "status.h"

#define XSTATUS_TYPE_CHECK(c, f) (((c) & (f)) == (f))

void XStat_Init(xstatus_t *pStatus, int nTypes, xstatus_cb_t cb, void *pUserCtx)
{
    XASSERT_VOID(pStatus);
    pStatus->nTypes = nTypes;
    pStatus->nAVStatus = XSTDNON;
    pStatus->pUserCtx = pUserCtx;
    pStatus->cb = cb;
}

void XStat_InitFrom(xstatus_t *pStatus, xstatus_t *pParent)
{
    XASSERT_VOID(pStatus);

    if (pParent != NULL)
    {
        XStat_Init(pStatus, XSTDNON, NULL, NULL);
        return;
    }

    pStatus->nTypes = pParent->nTypes;
    pStatus->nAVStatus = XSTDNON;
    pStatus->pUserCtx = pParent->pUserCtx;
    pStatus->cb = pParent->cb;
}

void* XStat_ErrPtr(xstatus_t *pStatus, const char *pFmt, ...)
{
    XASSERT_RET((pStatus && pStatus->cb), NULL);
    XASSERT_RET(XSTATUS_TYPE_CHECK(pStatus->nTypes, XSTATUS_ERROR), NULL);

    char sAvError[XSTR_MIN] = { "Unknown AVError" };
    if (pStatus->nAVStatus < 0)
    {
        sAvError[0] = XSTR_NUL;
        av_make_error_string(sAvError, sizeof(sAvError), pStatus->nAVStatus);
    }

    size_t nLength = 0;
    char *pDest;
    va_list args;

    va_start(args, pFmt);
    pDest = xstracpyargs(pFmt, args, &nLength);
    va_end(args);

    if (pDest == NULL) return NULL;
    char sError[XSTR_MID];

    xstrncpyf(sError, sizeof(sError), "%s (%s)", pDest, sAvError);
    pStatus->cb(pStatus->pUserCtx, XSTATUS_ERROR, sError);

    free(pDest);
    return NULL;
}

XSTATUS XStat_ErrCb(xstatus_t *pStatus, const char *pFmt, ...)
{
    XASSERT_RET((pStatus && pStatus->cb), XSTDERR);
    XASSERT_RET(XSTATUS_TYPE_CHECK(pStatus->nTypes, XSTATUS_ERROR), XSTDERR);

    char sAvError[XSTR_MIN] = { "Unknown AVError" };
    if (pStatus->nAVStatus < 0)
    {
        sAvError[0] = XSTR_NUL;
        av_make_error_string(sAvError, sizeof(sAvError), pStatus->nAVStatus);
    }

    size_t nLength = 0;
    char *pDest;
    va_list args;

    va_start(args, pFmt);
    pDest = xstracpyargs(pFmt, args, &nLength);
    va_end(args);

    if (pDest == NULL) return XSTDERR;
    char sError[XSTR_MID];

    xstrncpyf(sError, sizeof(sError), "%s (%s)", pDest, sAvError);
    pStatus->cb(pStatus->pUserCtx, XSTATUS_ERROR, sError);

    free(pDest);
    return XSTDERR;
}

XSTATUS XStat_InfoCb(xstatus_t *pStatus, const char *pFmt, ...)
{
    XASSERT_RET((pStatus && pStatus->cb), XSTDERR);
    XASSERT_RET(XSTATUS_TYPE_CHECK(pStatus->nTypes, XSTATUS_INFO), XSTDERR);

    size_t nLength = 0;
    char *pStatusStr;
    va_list args;

    va_start(args, pFmt);
    pStatusStr = xstracpyargs(pFmt, args, &nLength);
    va_end(args);

    if (pStatusStr == NULL) return XSTDERR;
    pStatus->cb(pStatus->pUserCtx, XSTATUS_INFO, pStatusStr);

    free(pStatusStr);
    return XSTDOK;
}

XSTATUS XStat_DebugCb(xstatus_t *pStatus, const char *pFmt, ...)
{
    XASSERT_RET((pStatus && pStatus->cb), XSTDERR);
    XASSERT_RET(XSTATUS_TYPE_CHECK(pStatus->nTypes, XSTATUS_DEBUG), XSTDERR);

    size_t nLength = 0;
    char *pStatusStr;
    va_list args;

    va_start(args, pFmt);
    pStatusStr = xstracpyargs(pFmt, args, &nLength);
    va_end(args);

    if (pStatusStr == NULL) return XSTDERR;
    pStatus->cb(pStatus->pUserCtx, XSTATUS_DEBUG, pStatusStr);

    free(pStatusStr);
    return XSTDOK;
}
