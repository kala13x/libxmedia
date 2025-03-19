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

#ifdef __cplusplus
extern "C" {
#endif

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
#include <libavutil/samplefmt.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixfmt.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
#include <libavutil/channel_layout.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

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

#ifdef __cplusplus
}
#endif

#endif /* __XMEDIA_STDINC_H__ */