/*!
 *  @file libxmedia/src/nalu.c
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Implementation of the NAL units parser.
 */

#include "nalu.h"

static void XNAL_UnitClearCb(xarray_data_t *pArrData)
{
    XASSERT_VOID_RET(pArrData);
    free(pArrData->pData);
}

void XNAL_InitUnit(xnal_unit_t *pUnit)
{
    pUnit->nReference = 0;
    pUnit->nUnitType = 0;
    pUnit->nDataPos = 0;
    pUnit->nNalPos = 0;
    pUnit->nSize = 0;
}

xnal_unit_t* XNAL_AllocUnit()
{
    xnal_unit_t *pUnit = (xnal_unit_t*)malloc(sizeof(xnal_unit_t));
    XASSERT(pUnit, NULL);
    XNAL_InitUnit(pUnit);
    return pUnit;
}

size_t XNAL_CheckStartCode(uint8_t *pBuffer, size_t nPos)
{
    /* Search for NAL unit start code: 0x000001 */
    if (pBuffer[nPos] == 0x00 &&
        pBuffer[nPos + 1] == 0x00 &&
        pBuffer[nPos + 2] == 0x01)
            return 3;

    /* Search for NAL unit start code: 0x00000001  */
    if (pBuffer[nPos] == 0x00 &&
        pBuffer[nPos + 1] == 0x00 &&
        pBuffer[nPos + 2] == 0x00 &&
        pBuffer[nPos + 3] == 0x01)
            return 4;

    return 0;
}

xarray_t* XNAL_ParseUnits(uint8_t *pBuffer, size_t nSize)
{
    xarray_t *pUnits = XArray_New(XSTDNON, XFALSE);
    XASSERT(pUnits, NULL);

    pUnits->clearCb = XNAL_UnitClearCb;
    size_t i;

    for (i = 0; i < nSize - 4; i++)
    {
        size_t nStartCodeSize = XNAL_CheckStartCode(pBuffer, i);
        if (nStartCodeSize)
        {
            xnal_unit_t* pUnit = XNAL_AllocUnit();
            if (pUnit == NULL)
            {
                XArray_Destroy(pUnits);
                return NULL;
            }

            pUnit->nNalPos = (int)i;
            pUnit->nDataPos = (int)(i + nStartCodeSize);
            pUnit->nUnitType = pBuffer[pUnit->nDataPos] & 0x1f;
            pUnit->nReference = pBuffer[pUnit->nDataPos] & 0x20;

            if (XArray_AddData(pUnits, pUnit, XSTDNON) <= 0)
            {
                XArray_Destroy(pUnits);
                return NULL;
            }
        }
    }

    /* Update NAL unit sizes */
    for (i = 0; i < pUnits->nUsed; i++)
    {
        xnal_unit_t* pUnit = XArray_GetData(pUnits, i);
        if (pUnit == NULL) continue;

        if (i == (pUnits->nUsed - 1))
        {
            /* This is the last unit in the array */
            pUnit->nSize = nSize - pUnit->nDataPos;
        }
        else
        {
            xnal_unit_t* pNext = XArray_GetData(pUnits, i + 1);
            pUnit->nSize = pNext ? pNext->nNalPos - pUnit->nDataPos : 0;
        }
    }

    return pUnits;
}

XSTATUS XNAL_ParseH264(uint8_t *pBuffer, size_t nSize, x264_extra_t *pExtraData)
{
    xarray_t *pUnits = XNAL_ParseUnits(pBuffer, nSize);
    XASSERT(pUnits, XSTDERR);

    size_t nUsed = XArray_Used(pUnits);
    if (!nUsed)
    {
        XArray_Destroy(pUnits);
        return XSTDNON;
    }

    size_t i;
    xbool_t bHaveExtra = XFALSE;

    for (i = 0; i < nUsed; i++)
    {
        xnal_unit_t* pUnit = XArray_GetData(pUnits, i);
        if (pUnit == NULL) continue;

        if (pUnit->nUnitType == 7) // SPS
        {
            pExtraData->pSPS = &pBuffer[pUnit->nDataPos];
            pExtraData->nSPSSize = pUnit->nSize;
        }
        else if (pUnit->nUnitType == 8) // PPS
        {
            pExtraData->pPPS = &pBuffer[pUnit->nDataPos];
            pExtraData->nPPSSize = pUnit->nSize;
        }

        if (pExtraData->pSPS &&
            pExtraData->pPPS)
            bHaveExtra = XTRUE;
    }

    XArray_Destroy(pUnits);
    return bHaveExtra ? XSTDOK : XSTDNON;
}