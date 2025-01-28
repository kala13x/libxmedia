/*!
 *  @file libxmedia/src/stdinc.h
 *
 *  This source is part of "libxmedia" project
 *  2022-2023  Sun Dro (s.kalatoz@gmail.com)
 *
 * @brief Standart includes.
 */

#ifndef __XMEDIA_STDINC_H__
#define __XMEDIA_STDINC_H__

#include <xutils/xstd.h>
#include <xutils/xtime.h>
#include <xutils/array.h>
#include <xutils/json.h>
#include <xutils/sig.h>
#include <xutils/log.h>
#include <xutils/str.h>
#include <xutils/xfs.h>

#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>

#define XMEDIA_AVCODEC_AT_LEAST(major,minor)  (LIBAVCODEC_VERSION_MAJOR > major || \
                                              (LIBAVCODEC_VERSION_MAJOR == major && \
                                               LIBAVCODEC_VERSION_MINOR >= minor))

#define XMEDIA_AVFORMAT_AT_LEAST(major,minor)  (LIBAVFORMAT_VERSION_MAJOR > major || \
                                               (LIBAVFORMAT_VERSION_MAJOR == major && \
                                                LIBAVFORMAT_VERSION_MINOR >= minor))

#ifndef XCODEC_USE_NEW_CHANNEL
  #if XMEDIA_AVCODEC_AT_LEAST(59, 24)
    #define XCODEC_USE_NEW_CHANNEL 1
  #endif
#endif

#ifndef XCODEC_USE_NEW_FIFO
  #if XMEDIA_AVCODEC_AT_LEAST(59, 24)
    #define XCODEC_USE_NEW_FIFO 1
  #endif
#endif

#endif /* __XMEDIA_STDINC_H__ */