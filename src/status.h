/*!
 *  @file libxmedia/src/status.h
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the status callbacks.
 */

#ifndef __XMEDIA_STATUS_H__
#define __XMEDIA_STATUS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdinc.h"

typedef enum {
    XSTATUS_INFO = (1 << 0),
    XSTATUS_ERROR = (1 << 1),
    XSTATUS_DEBUG = (1 << 2),
    XSTATUS_ALL = 7
} xstatus_type_t;

typedef void(*xstatus_cb_t)(void *pUserCtx, xstatus_type_t nType, const char *pStatus);

typedef struct xstatus_ {
    xstatus_cb_t cb;
    uint16_t nTypes;
    void *pUserCtx;
    int nAVStatus;
} xstatus_t;

void XStat_Init(xstatus_t *pStatus, int nTypes, xstatus_cb_t cb, void *pUserCtx);
void XStat_InitFrom(xstatus_t *pStatus, xstatus_t *pParent);

XSTATUS XStat_ErrCb(xstatus_t *pStatus, const char *pFmt, ...);
XSTATUS XStat_InfoCb(xstatus_t *pStatus, const char *pFmt, ...);
XSTATUS XStat_DebugCb(xstatus_t *pStatus, const char *pFmt, ...);
void* XStat_ErrPtr(xstatus_t *pStatus, const char *pFmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __XMEDIA_STATUS_H__ */
