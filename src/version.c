/*!
 *  @file libxmedia/src/version.c
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Project version and build number.
 */

#include "stdinc.h"
#include "version.h"

static XATOMIC g_nHaveVerShort = 0;
static XATOMIC g_nHaveVerLong = 0;
static char g_xMediaVerShort[128];
static char g_xMediaVerLong[256];

size_t XMedia_GetVersion(char *pDstStr, size_t nSize, xbool_t bShort)
{
    if (bShort)
    {
        return xstrncpyf(pDstStr,
            nSize, "%d.%d.%d",
            XMEDIA_VERSION_MAX,
            XMEDIA_VERSION_MIN,
            XMEDIA_BUILD_NUMBER);
    }

    return xstrncpyf(pDstStr, nSize,
        "%d.%d build %d (%s)",
        XMEDIA_VERSION_MAX,
        XMEDIA_VERSION_MIN,
        XMEDIA_BUILD_NUMBER,
        __DATE__);
}

const char* XMedia_Version(void)
{
    if (!XSYNC_ATOMIC_GET(&g_nHaveVerLong))
    {
        XMedia_GetVersion(g_xMediaVerLong, sizeof(g_xMediaVerLong), XFALSE);
        XSYNC_ATOMIC_SET(&g_nHaveVerLong, XTRUE);
    }

    return g_xMediaVerLong;
}

const char* XMedia_VersionShort(void)
{
    if (!XSYNC_ATOMIC_GET(&g_nHaveVerShort))
    {
        XMedia_GetVersion(g_xMediaVerLong, sizeof(g_xMediaVerLong), XTRUE);
        XSYNC_ATOMIC_SET(&g_nHaveVerShort, XTRUE);
    }

    return g_xMediaVerShort;
}
