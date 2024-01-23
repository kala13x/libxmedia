/*!
 *  @file libxmedia/src/version.h
 *
 *  This source is part of "libxmedia" project
 *  2022-2023 (c) Sun Dro (s.kalatoz@gmail.com)
 * 
 * @brief Project version and build number.
 */

#ifndef __XMEDIA_XLIBVER_H__
#define __XMEDIA_XLIBVER_H__

#include "stdinc.h"

#define XMEDIA_VERSION_MAX     1
#define XMEDIA_VERSION_MIN     2
#define XMEDIA_BUILD_NUMBER    14

#ifdef __cplusplus
extern "C" {
#endif

size_t XMedia_GetVersion(char *pDstStr, size_t nSize, xbool_t bShort);
const char* XMedia_VersionShort(void);
const char* XMedia_Version(void);

#ifdef __cplusplus
}
#endif

#endif /* __XMEDIA_XLIBVER_H__ */
