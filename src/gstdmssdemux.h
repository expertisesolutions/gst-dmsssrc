/* GStreamer
 * Copyright (C) <2018> Felipe Magno de Almeida <felipe@expertisesolutions.com.br>
 *     Author: Felipe Magno de Almeida <felipe@expertisesolutions.com.br>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GST_DMSS_DEMUX_H__
#define __GST_DMSS_DEMUX_H__

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include <gio/gio.h>

#include "gstdmss.h"

G_BEGIN_DECLS

#define GST_TYPE_DMSS_DEMUX \
  (gst_dmss_demux_get_type())
#define GST_DMSS_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_DMSS_DEMUX,GstDmssDemux))
#define GST_DMSS_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_DMSS_DEMUX,GstDmssDemuxClass))
#define GST_IS_DMSS_DEMUX(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_DMSS_DEMUX))
#define GST_IS_DMSS_DEMUX_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_DMSS_DEMUX))

typedef struct _GstDmssDemux GstDmssDemux;
typedef struct _GstDmssDemuxClass GstDmssDemuxClass;

typedef enum GstDmssVideoFormat GstDmssVideoFormat;
typedef enum GstDmssAudioFormat GstDmssAudioFormat;
typedef enum GstDmssAudioRate GstDmssAudioRate;

enum GstDmssVideoFormat
{
  GST_DMSS_VIDEO_H264 = 0x08,
  GST_DMSS_VIDEO_H265 = 0x0C,
  GST_DMSS_VIDEO_FORMAT_UNKNOWN = 0x1FF
};

enum GstDmssAudioFormat
{
  GST_DMSS_AUDIO_ALAW = 0x0E,
  GST_DMSS_AUDIO_MULAW = 0x0A,
  GST_DMSS_AUDIO_G726 = 0x1B,
  GST_DMSS_AUDIO_AAC = 0x1A,
  GST_DMSS_AUDIO_FORMAT_UNKNOWN = 0x1FF
};

enum GstDmssAudioRate
{
  GST_DMSS_AUDIO_8000 = 2,
  GST_DMSS_AUDIO_16000 = 4,
  GST_DMSS_AUDIO_64000 = 13,
  GST_DMSS_AUDIO_UNKNOWN = 0x1FF
};

struct _GstDmssDemux
{
  GstElement element;

  GstPad *sinkpad;
  GstPad *videosrcpad;
  GstPad *audiosrcpad;

  GstDmssVideoFormat video_format;
  GstDmssAudioFormat audio_format;
  GstDmssAudioRate audio_rate;

  gboolean need_segment;
  guint32 segment_seqnum;
  GstSegment byte_segment;
  GstSegment time_segment;

  GstAdapter *adapter;
  gboolean waiting_dhav_end;
  guint16 last_ts;
  GstClockTime last_timestamp;

  GstClockTime latency; // should not use this anymore, but avg latency or something else

  GstClock* pipeline_clock;
  GstClockTime send_base_time;
  GstClockTime base_time;
  // gboolean need_resync;

  // int samples;
  // GstClockTime last_latency;
  // GstClockTime avg_latency;
  // GstClockTime jitter;
  // //GstClockTime min_delta;
  // int window_pos;
  // GstClockTime latency_window[DMSS_MAXIMUM_SAMPLES_AVERAGE];
  // GstClockTime delta_window[DMSS_MAXIMUM_SAMPLES_AVERAGE];
  // GstClockTime sorted_offsets_delta_window[DMSS_MAXIMUM_SAMPLES_AVERAGE];
};

struct _GstDmssDemuxClass
{
  GstElementClass parent_class;
};

GType gst_dmss_demux_get_type (void);

G_END_DECLS
#endif /* __GST_DMSS_DEMUX_H__ */
