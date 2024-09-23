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

#include <xutils/array.h>
#include <xutils/xtime.h>
#include <xutils/xjson.h>
#include <xutils/xstd.h>
#include <xutils/xsig.h>
#include <xutils/xlog.h>
#include <xutils/xstr.h>
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

/*
 If FF_API_OLD_CHANNEL_LAYOUT is defined, that means 'channel' and
 'channel_layout' members of AVCodecContext structure are deprecated
 and we need to use the new AVChannelLayout structure instead.
*/
#ifndef XCODEC_USE_NEW_CHANNEL
  #if defined(FF_API_OLD_CHANNEL_LAYOUT) || defined(__APPLE__)
    #define XCODEC_USE_NEW_CHANNEL 1
  #endif
#endif

#ifndef XCODEC_USE_NEW_FIFO
  #if defined(FF_API_FIFO_OLD_API) || defined(__APPLE__)
    #define XCODEC_USE_NEW_FIFO 1
  #endif
#endif

#endif /* __XMEDIA_STDINC_H__ */