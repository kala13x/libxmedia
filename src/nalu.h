/*!
 *  @file libxmedia/src/nalu.h
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the NAL units parser.
 */

#ifndef __XMEDIA_NALU_H__
#define __XMEDIA_NALU_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stdinc.h"
#include "codec.h"

typedef struct xnal_unit_ {
    int nReference;
    int nUnitType;
    int nDataPos;
    int nNalPos;
    int nSize;
} xnal_unit_t;

void XNAL_InitUnit(xnal_unit_t *pUnit);
xnal_unit_t* XNAL_AllocUnit();

size_t XNAL_CheckStartCode(uint8_t *pBuffer, size_t nPos);
xarray_t* XNAL_ParseUnits(uint8_t *pBuffer, size_t nSize);

XSTATUS XNAL_ParseH264(uint8_t *pBuffer, size_t nSize, x264_extra_t *pExtraData);

#ifdef __cplusplus
}
#endif

#endif /* __XMEDIA_NALU_H__ */
