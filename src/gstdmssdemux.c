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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gstversion.h>
#if GST_VERSION_MINOR <= 15
#include <gst/gst-i18n-plugin.h>
#endif
#include <gst/gst.h>
#include <gio/gio.h>
#include "gstdmssdemux.h"
#include "gstdmss.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

GST_DEBUG_CATEGORY_STATIC (dmssdemux_debug);
#define GST_CAT_DEFAULT dmssdemux_debug

static const char DHAV_prefix[4] = {'D', 'H', 'A', 'V'};
static const char DHAV_suffix[4] = {'d', 'h', 'a', 'v'};

#define VIDEO_CAPS \
  GST_STATIC_CAPS (\
    "video/x-h264, stream-format=(string)byte-stream;" \
    "video/x-h265, stream-format=(string)byte-stream;" \
    "video/mpeg, " \
      "mpegversion = (int) 4; " \
)

#define AUDIO_CAPS \
  GST_STATIC_CAPS (\
    "audio/x-alaw, " \
      "rate = (int) [ 8000, 16000, 32000 ], channels = (int) [ 1, 2 ];" \
    "audio/x-mulaw, " \
      "rate = (int) [ 8000, 16000, 32000 ], channels = (int) [ 1, 2 ];" \
    "audio/mpeg, " \
      "rate = (int) [ 8000, 16000, 32000 ], channels = (int) [ 1, 2 ]," \
      "framed = (boolean) false," \
      "level = (string) 1," \
      "mpegversion = (int) 4, " \
      "stream-format = (string) adts; " \
)

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-dmss"));

static GstStaticPadTemplate video_template = GST_STATIC_PAD_TEMPLATE ("video",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    VIDEO_CAPS);

static GstStaticPadTemplate audio_template = GST_STATIC_PAD_TEMPLATE ("audio",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    AUDIO_CAPS);

enum
{
  PROP_0,
  PROP_LATENCY
};

#define gst_dmss_demux_parent_class parent_class
G_DEFINE_TYPE (GstDmssDemux, gst_dmss_demux, GST_TYPE_ELEMENT);

static void gst_dmss_demux_finalize (GObject * object);

static GstPad *gst_dmss_demux_add_audio_pad (GstDmssDemux * demux,
    GstCaps * caps);
static GstPad *gst_dmss_demux_add_video_pad (GstDmssDemux * demux,
    GstCaps * caps);

/* query functions */
static gboolean gst_dmss_demux_src_query (GstPad * pad,
    GstObject * parent, GstQuery * query);
static gboolean gst_dmss_demux_sink_query (GstPad * pad, GstObject * parent,
    GstQuery * query);

static gboolean gst_dmss_demux_handle_src_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_dmss_demux_handle_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static gboolean gst_dmss_demux_send_event (GstElement * element,
    GstEvent * event);
static gboolean gst_dmss_demux_set_clock (GstElement * element,
    GstClock * clock);
static GstClock *gst_dmss_demux_provide_clock (GstElement * element);

static GstFlowReturn gst_dmss_demux_chain (GstPad * pad, GstObject * parent,
    GstBuffer * buffer);

static gboolean gst_dmss_demux_sink_activate (GstPad * sinkpad,
    GstObject * parent);
static GstStateChangeReturn gst_dmss_demux_change_state (GstElement * element,
    GstStateChange transition);
static GstClockTime gst_dmss_demux_calculate_pts (GstDmssDemux *demux,
    guint16 frame_epoch, guint16 frame_ts);

static void gst_dmss_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dmss_demux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_dmss_demux_resync (GstDmssDemux *demux, guint16 frame_epoch, guint16 frame_ts, GstClockTime current_time,
                                   GstClockTime send_base_time, GstClockTime timestamp);


static void
gst_dmss_demux_class_init (GstDmssDemuxClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->set_property = gst_dmss_demux_set_property;
  gobject_class->get_property = gst_dmss_demux_get_property;
  gobject_class->finalize = gst_dmss_demux_finalize;

  g_object_class_install_property (gobject_class, PROP_LATENCY,
      g_param_spec_uint ("latency", "Latency",
          "Set latency in ms", 0, G_MAXUINT, DMSS_DEFAULT_LATENCY,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  
  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_dmss_demux_change_state);
  gstelement_class->send_event = GST_DEBUG_FUNCPTR (gst_dmss_demux_send_event);
  gstelement_class->set_clock = GST_DEBUG_FUNCPTR (gst_dmss_demux_set_clock);
  gstelement_class->provide_clock =
      GST_DEBUG_FUNCPTR (gst_dmss_demux_provide_clock);

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&video_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&audio_template));

  gst_element_class_set_metadata (gstelement_class,
      "DMSS demuxer",
      "Demuxer for IP Camera",
      "Receive data from IP camera",
      "Felipe Magno de Almeida <felipe@expertisesolutions.com.br>");

  GST_DEBUG_CATEGORY_INIT (dmssdemux_debug, "dmssdemux", 0, "DMSS Demux");
}

static gboolean
gst_dmss_demux_audio_push_event (GstDmssDemux * demux, GstEvent * event)
{
  gboolean res = FALSE;

  if (demux->audiosrcpad)
  {
    GST_LOG_OBJECT (demux, "Pushing event to audiosrcpad");
    res = gst_pad_push_event (demux->audiosrcpad, event);
  }
  else {
    gst_event_unref (event);
    res = TRUE;
  }

  return res;
}

static gboolean
gst_dmss_demux_video_push_event (GstDmssDemux * demux, GstEvent * event)
{
  gboolean res = FALSE;

  if (demux->videosrcpad) {
    GST_LOG_OBJECT (demux, "Pushing event to videosrcpad");
    res = gst_pad_push_event (demux->videosrcpad, event);
  }
  else {
    gst_event_unref (event);
    res = TRUE;
  }

  return res;
}

static gboolean
gst_dmss_demux_push_event (GstDmssDemux * demux, GstEvent * event)
{
  gboolean res = FALSE;

  res = gst_dmss_demux_video_push_event (demux, gst_event_ref (event));
  if (res)
    res = gst_dmss_demux_audio_push_event (demux, event);

  return res;
}

static void
gst_dmss_demux_segment_init (GstDmssDemux * demux, GstClockTime timestamp)
{
  GstEvent *event;

  GST_DEBUG_OBJECT (demux, "segment init");

  gst_segment_init (&demux->time_segment, GST_FORMAT_TIME);

  demux->time_segment.start = demux->time_segment.position = timestamp;

  event = gst_event_new_segment (&demux->time_segment);
  gst_dmss_demux_push_event (demux, event);
  demux->need_segment = FALSE;
}

static guint64
gst_dmss_demux_find_extended_header_value (guint8 prefix,
    guint64 extended_header[32])
{
  int i;
  gint64 v = -1;

  for (i = 0; i != 32; ++i) {
    if (prefix != 0x88 || prefix != 0x82) {
      guint8 pv = (extended_header[i] & 0xFF000000) >> 24;
      if (pv == prefix) {
        v = extended_header[i] & 0xFFFFFF;
        break;
      } else if (!pv)
        break;
    } else {
      guint8 pv = (extended_header[i] & 0xFF00000000000000) >> 56;
      if (pv == prefix) {
        v = extended_header[i] & 0xFFFFFFFFFFFFFF;
        break;
      } else if (!pv)
        break;
    }
  }

  return v;
}

static void
gst_dmss_demux_audio_prepare_buffer (GstDmssDemux * demux, GstBuffer * buffer,
    guint64 extended_header[32])
{
  GstDmssAudioFormat format;
  GstDmssAudioRate rate;
  guint64 value;
  GstCaps *caps;
  int rate_num;

  value =
      gst_dmss_demux_find_extended_header_value
      (DMSS_EXTENDED_HEADER_AUDIOINFO_PREFIX, extended_header);

  format = (value & 0xFF00) >> 8;
  rate = value & 0xFF;
  if (!demux->audiosrcpad && (format != demux->audio_format
          || rate != demux->audio_rate)) {
    switch (rate) {
      case GST_DMSS_AUDIO_8000:
        rate_num = 8000;
        break;
      case GST_DMSS_AUDIO_16000:
        rate_num = 16000;
        break;
      case GST_DMSS_AUDIO_32000:
        rate_num = 32000;
        break;
      case GST_DMSS_AUDIO_48000:
        rate_num = 48000;
        break;
      case GST_DMSS_AUDIO_64000:
        rate_num = 64000;
        break;
      default:
        GST_ELEMENT_WARNING (demux, RESOURCE, READ, (NULL),
            ("Unknown audio rate: %d", (int) rate));
        return;
    }

    switch (format) {
      case GST_DMSS_AUDIO_ALAW:
        GST_DEBUG_OBJECT (demux, "Audio ALAW");
        caps =
            gst_caps_new_simple ("audio/x-alaw", "rate", G_TYPE_INT, rate_num,
            "channels", G_TYPE_INT, 1, NULL);
        break;
      case GST_DMSS_AUDIO_MULAW:
        GST_DEBUG_OBJECT (demux, "Audio MULAW");
        caps =
            gst_caps_new_simple ("audio/x-mulaw", "rate", G_TYPE_INT,
            rate_num, "channels", G_TYPE_INT, 1, NULL);
        break;
      case GST_DMSS_AUDIO_G726:
        GST_DEBUG_OBJECT (demux, "Audio G726");
        caps =
            gst_caps_new_simple ("audio/x-g726", "rate", G_TYPE_INT, rate_num,
            "channels", G_TYPE_INT, 1, NULL);
        break;
      case GST_DMSS_AUDIO_AAC:
        GST_DEBUG_OBJECT (demux, "Audio AAC");
        caps =
          gst_caps_new_simple ("audio/mpeg", "rate", G_TYPE_INT, rate_num,
            "framed", G_TYPE_BOOLEAN, 0,
            "level", G_TYPE_STRING, "1",
            "mpegversion", G_TYPE_INT, 4,
            "stream-format", G_TYPE_STRING, "adts",
            "channels", G_TYPE_INT, 1, NULL);
        break;
      default:
        GST_ELEMENT_WARNING (demux, RESOURCE, READ, (NULL),
            ("Unknown audio format: %d", (int) format));
        return;
    }

    demux->audio_format = format;
    demux->audio_rate = rate;

    gst_dmss_demux_add_audio_pad (demux, caps);
  }
}

static void
gst_dmss_demux_video_prepare_buffer (GstDmssDemux * demux, GstBuffer * buffer,
    guint64 extended_header[32])
{
  GstDmssVideoFormat format;
  guint32 value;
  GstCaps *caps;

  value =
      gst_dmss_demux_find_extended_header_value
      (DMSS_EXTENDED_HEADER_VIDEOINFO_PREFIX, extended_header);

  format = (value & 0xFF00) >> 8;
  if (format != demux->video_format) {
    switch (format) {
      default:
        GST_ELEMENT_WARNING (demux, RESOURCE, READ, (NULL),
            ("Unknown Video format: %d", (int) format));
      case GST_DMSS_VIDEO_H264:
        GST_DEBUG_OBJECT (demux, "Video H264");
        caps =
            gst_caps_new_simple ("video/x-h264", "stream-format",
            G_TYPE_STRING, "byte-stream", "alignment",
            G_TYPE_STRING, "nal", NULL);
        break;
      case GST_DMSS_VIDEO_H265:
        GST_DEBUG_OBJECT (demux, "Video H265");
        caps =
            gst_caps_new_simple ("video/x-h265", "stream-format",
            G_TYPE_STRING, "byte-stream", "alignment",
            G_TYPE_STRING, "nal", NULL);
        break;
        return;
    }

    demux->video_format = format;

    gst_dmss_demux_add_video_pad (demux, caps);
  }
}

static void
gst_dmss_demux_parse_extended_header (GstDmssDemux * demux, gchar * header,
    int size, guint64 extended_header[32])
{
  gchar *p = header, *end = header + size;
  int i = 0;
  guint32 copy32;               // because of alignment

  while (p != end && i != 32) {
    GST_DEBUG ("reading extended header of %.02x", (unsigned int) (guint8) * p);

    if (end - p >= sizeof (guint32)) {
      memcpy (&copy32, p, sizeof (copy32));
      extended_header[i] = GUINT32_FROM_BE (copy32);
      p += sizeof (guint32);
    } else {
      GST_ELEMENT_WARNING (demux, RESOURCE, READ, (NULL),
          ("Couldn't parse extended header correctly"));
      break;
    }
    GST_DEBUG ("Value read for header %" G_GUINT64_FORMAT, extended_header[i]);
    ++i;
  }
}

static void
gst_dmss_demux_flush (GstDmssDemux * demux)
{
  //int const prologue_size = 32;
  int const dhav_fixed_header_size = 24;
  int const dhav_epilogue_size = 8;
  int const minimum_prefix_search = 32*1024;
  guint64 extended_header[32];
  gsize size;
  GstBuffer *buffer = NULL;
  GstMapInfo map;
  guint8 dhav_packet_type;
  guint32 dhav_packet_size;
  guint32 dhav_head_size;
  guint32 dhav_body_size;
  gchar const *prologue;
  guint32 minimum_dhav_size = dhav_fixed_header_size + dhav_epilogue_size;
  guint16 frame_epoch;
  guint16 frame_ts/*, ring_diff_ts*//*, reverse_ring_diff_ts*/;
  //int diff_ts;
  //GstClockTime absolute_timestamp;
  gboolean is_audio;
  gchar const *error_msg;
  int start_offset, mapped_size;

  size = gst_adapter_available (demux->adapter);

  while (size >= /*prologue_size +*/ minimum_dhav_size) {
    GST_LOG_OBJECT (demux, "loop size %d", (int)size);
    prologue =
      gst_adapter_map (demux->adapter, /*prologue_size +*/ minimum_dhav_size);
    if (!prologue)
      goto adapter_map_error;

    start_offset = 0;

    if ((prologue[/*prologue_size +*/ 0] != DHAV_prefix[0] ||
         prologue[/*prologue_size +*/ 1] != DHAV_prefix[1] ||
         prologue[/*prologue_size +*/ 2] != DHAV_prefix[2] ||
         prologue[/*prologue_size +*/ 3] != DHAV_prefix[3])) {

      while (size - start_offset > /*prologue_size +*/ minimum_dhav_size &&
             (prologue[/*prologue_size +*/ start_offset + 0] != DHAV_prefix[0] ||
              prologue[/*prologue_size +*/ start_offset + 1] != DHAV_prefix[1] ||
              prologue[/*prologue_size +*/ start_offset + 2] != DHAV_prefix[2] ||
              prologue[/*prologue_size +*/ start_offset + 3] != DHAV_prefix[3])) {
        GST_LOG_OBJECT (demux, "searching for prologue DHAV prefix (not found immediatelly)");
        mapped_size = start_offset + minimum_prefix_search < size ? start_offset + minimum_prefix_search : size;
        assert (mapped_size <= size);
        assert (mapped_size >= /*prologue_size +*/ minimum_dhav_size);
        
        gst_adapter_unmap (demux->adapter);
        GST_LOG_OBJECT (demux, "Remapping for %d size with %d offset", mapped_size, start_offset);
        prologue = gst_adapter_map (demux->adapter, mapped_size);
        if (!prologue) {
          gst_adapter_flush (demux->adapter, start_offset);
          goto adapter_map_error;
        }

        while (mapped_size - start_offset != /*prologue_size +*/ minimum_dhav_size && 
               (prologue[/*prologue_size +*/ start_offset + 0] != DHAV_prefix[0] ||
                prologue[/*prologue_size +*/ start_offset + 1] != DHAV_prefix[1] ||
                prologue[/*prologue_size +*/ start_offset + 2] != DHAV_prefix[2] ||
                prologue[/*prologue_size +*/ start_offset + 3] != DHAV_prefix[3]))
          ++start_offset;

        GST_LOG_OBJECT (demux, "searching for prologue DHAV prefix (discarding at least %d) mapped_size %d", start_offset, mapped_size);
      }
    }
    else
      GST_LOG_OBJECT (demux, "Prologue DHAV prefix found at %d", start_offset);

    if ((prologue[/*prologue_size +*/ start_offset + 0] != DHAV_prefix[0] ||
         prologue[/*prologue_size +*/ start_offset + 1] != DHAV_prefix[1] ||
         prologue[/*prologue_size +*/ start_offset + 2] != DHAV_prefix[2] ||
         prologue[/*prologue_size +*/ start_offset + 3] != DHAV_prefix[3]))
      goto prefix_error;

    GST_LOG_OBJECT (demux, "Prologue DHAV prefix found at %d", start_offset);

    prologue += start_offset;
    dhav_packet_type = prologue[/*prologue_size +*/ 4];

    dhav_packet_size =
        GUINT32_FROM_LE (*(guint32 *) & prologue[/*prologue_size +*/ 12]);
    dhav_head_size = *(unsigned char *) &prologue[/*prologue_size +*/ 22];
    dhav_body_size =
        dhav_packet_size - (dhav_fixed_header_size + dhav_epilogue_size +
        dhav_head_size);

    gst_adapter_unmap (demux->adapter);

    if (start_offset) {
      GST_DEBUG ("Packet didn't start at right offset. Skipped %d bytes",
          start_offset);
      gst_adapter_flush (demux->adapter, start_offset);
    }

    GST_DEBUG
        ("DHAV packet (checking if downloaded) type: %.02x DHAV size: %d head size: %d body size: %d",
        (int) dhav_packet_type, (int) dhav_packet_size, (int) dhav_head_size,
        (int) dhav_body_size);

    is_audio = (dhav_packet_type == (unsigned char) 0xf0);

    if (is_audio)
      GST_INFO ("DHAV audio packet");

    if (!is_audio &&
        dhav_packet_type != (unsigned char) 0xfc &&
        dhav_packet_type != (unsigned char) 0xfd) {
      /* discard packet */
      GST_INFO ("Discarding DHAV packet that is not video frame");
      gst_adapter_flush (demux->adapter, dhav_packet_size/* + prologue_size*/);
      size = gst_adapter_available (demux->adapter);
      continue;
    }

    if (dhav_packet_size/* + prologue_size*/ <= size) {
      GST_DEBUG
          ("DHAV packet (%X) fully downloaded (size downloaded: %d, packet size + prologue: %d)", (int)(unsigned char)dhav_packet_type,
           (int) size, (int) dhav_packet_size/* + prologue_size*/);
      assert (buffer == NULL);
      buffer = gst_adapter_take_buffer_fast (demux->adapter, dhav_packet_size);

      gst_buffer_map (buffer, &map, GST_MAP_READ);

      if (map.data[/*prologue_size +*/ dhav_packet_size - 8] != DHAV_suffix[0] ||
          map.data[/*prologue_size +*/ dhav_packet_size - 7] != DHAV_suffix[1] ||
          map.data[/*prologue_size +*/ dhav_packet_size - 6] != DHAV_suffix[2] ||
          map.data[/*prologue_size +*/ dhav_packet_size - 5] != DHAV_suffix[3]) {
        error_msg = "Packet doesn't end with dhav suffix";
        goto corrupted_error;
      }

      if (GUINT32_FROM_LE (*(guint32 *) & map.data[/*prologue_size +*/
                  dhav_packet_size - 4]) != dhav_packet_size) {
        error_msg = "Packet suffixed size doesn't match header packet size";
        goto corrupted_error;
      }

      frame_epoch =
          GUINT16_FROM_LE (*(guint16 *) & prologue[/*prologue_size +*/ 16]);
      frame_ts = GUINT16_FROM_LE (*(guint16 *) & prologue[/*prologue_size +*/ 20]);

      GST_INFO ("DHAV frame timing info epoch: %d timestamp: %d",
          (int) frame_epoch, (int) frame_ts);

      gst_dmss_demux_parse_extended_header (demux,
          (gchar *) map.data +
                                            /*prologue_size +*/
          dhav_fixed_header_size, dhav_head_size, extended_header);

      if (is_audio)
        gst_dmss_demux_audio_prepare_buffer (demux, buffer, extended_header);
      else
        gst_dmss_demux_video_prepare_buffer (demux, buffer, extended_header);

      gst_buffer_unmap (buffer, &map);
      buffer = gst_buffer_make_writable (buffer);

      if (dhav_packet_type == (unsigned char) 0xfc)
      {
        GST_DEBUG ("Set delta flag for complete frame");
        GST_BUFFER_FLAG_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT);
      }

      // calculate PTS
      GST_BUFFER_TIMESTAMP (buffer) = gst_dmss_demux_calculate_pts (demux, frame_epoch, frame_ts);

      /* GST_LOG_OBJECT (demux, */
      /*     "%s buffer of size %" G_GSIZE_FORMAT ", ts %" GST_TIME_FORMAT */
      /*     ", offset %" G_GINT64_FORMAT ", offset_end %" G_GINT64_FORMAT, */
      /*     (!is_audio ? "Video" : "Audio"), gst_buffer_get_size (buffer), */
      /*     GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buffer)), */
      /*     GST_BUFFER_OFFSET (buffer), GST_BUFFER_OFFSET_END (buffer)); */

      GST_DEBUG ("Resizing buffer to offset %d and size %d",
          (int) /*prologue_size +*/ dhav_head_size, (int) dhav_body_size);
      gst_buffer_resize (buffer,
          /*prologue_size +*/ dhav_head_size +
          dhav_fixed_header_size, dhav_body_size);
      GST_DEBUG ("Resized");

      if (is_audio) {
        if (demux->audiosrcpad) {
          /* GST_DEBUG ("pushed audio buffer"); */
          GST_DEBUG_OBJECT (demux, "pushing audio buffer");
          gst_pad_push (demux->audiosrcpad, buffer);
          GST_DEBUG_OBJECT (demux, "pushed audio buffer");
          buffer = NULL;
        }
        else {
          GST_DEBUG_OBJECT (demux, "no audio pad, discarding buffer");
          gst_buffer_unref(buffer);
          buffer = NULL;
        }
      } else {
        GST_DEBUG_OBJECT (demux, "pushing video buffer");
        gst_pad_push (demux->videosrcpad, buffer);
        GST_DEBUG_OBJECT (demux, "pushed video buffer");
        buffer = NULL;
      }
      
      size = gst_adapter_available (demux->adapter);
    } else {
      /* demux->waiting_dhav_end = TRUE; */
      GST_DEBUG ("Needs to download more to complete DHAV packet");
      buffer = NULL;
      break;
    }
  }

  return;
prefix_error:
  GST_ELEMENT_INFO (demux, RESOURCE, READ, (NULL),
      ("DHAV packet doesn't start with the correct bytes"));
  gst_adapter_unmap (demux->adapter);
  gst_adapter_clear (demux->adapter);
  return;
corrupted_error:
  GST_ELEMENT_INFO (demux, RESOURCE, READ, (NULL),
      ("DHAV packet is corrupted: %s", error_msg));
  gst_object_unref (buffer);
  gst_buffer_unmap (buffer, &map);
  gst_adapter_clear (demux->adapter);
  return;
adapter_map_error:
  GST_ELEMENT_ERROR (demux, RESOURCE, READ, (NULL),
      ("Error mapping buffer with gst_adapter_map"));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_dmss_demux_init (GstDmssDemux * demux)
{
  demux->sinkpad = gst_pad_new_from_static_template (&sink_template, "sink");
  demux->videosrcpad =
      gst_pad_new_from_static_template (&video_template, "video");
  demux->audiosrcpad = NULL;
  demux->adapter = gst_adapter_new ();
  demux->need_segment = TRUE;
  demux->audio_format = GST_DMSS_AUDIO_FORMAT_UNKNOWN;
  demux->video_format = GST_DMSS_VIDEO_FORMAT_UNKNOWN;
  demux->latency = DMSS_DEFAULT_LATENCY;
  demux->pipeline_clock = NULL;
  demux->base_time = 0;
  /* demux->need_resync = TRUE; */
  /* demux->samples = 0; */
  /* demux->last_latency = 0; */
  /* demux->avg_latency = 0; */
  /* demux->jitter = 0; */

  gst_pad_set_query_function (demux->videosrcpad,
      GST_DEBUG_FUNCPTR (gst_dmss_demux_src_query));
  gst_pad_set_event_function (demux->videosrcpad,
      GST_DEBUG_FUNCPTR (gst_dmss_demux_handle_src_event));
  gst_pad_use_fixed_caps (demux->videosrcpad);
  gst_pad_set_active (demux->videosrcpad, TRUE);
  gst_element_add_pad (GST_ELEMENT (demux), demux->videosrcpad);

  gst_pad_set_activate_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dmss_demux_sink_activate));

  gst_pad_set_query_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dmss_demux_sink_query));
  /* for push mode, this is the chain function */
  gst_pad_set_chain_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dmss_demux_chain));
  /* handling events (in push mode only) */
  gst_pad_set_event_function (demux->sinkpad,
      GST_DEBUG_FUNCPTR (gst_dmss_demux_handle_sink_event));

  /* now add the pad */
  gst_element_add_pad (GST_ELEMENT (demux), demux->sinkpad);
}

static void
gst_dmss_demux_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_dmss_demux_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDmssDemux *demux = GST_DMSS_DEMUX (object);

  switch (prop_id) {
  case PROP_LATENCY:
      demux->latency = g_value_get_uint (value);

      gst_element_send_event (GST_ELEMENT (demux),
            gst_event_new_latency (demux->latency * GST_MSECOND));
      break;
  default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}  

static void
gst_dmss_demux_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDmssDemux *demux = GST_DMSS_DEMUX (object);

  switch (prop_id) {
    case PROP_LATENCY:
      g_value_set_uint (value, demux->latency);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstStateChangeReturn
gst_dmss_demux_change_state (GstElement * element, GstStateChange transition)
{
  GstDmssDemux *demux = GST_DMSS_DEMUX (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      demux->need_segment = TRUE;
      gst_adapter_clear (demux->adapter);
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static gboolean
gst_dmss_demux_sink_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  /* GST_DEBUG ("%s:%d %s %d", __FILE__, __LINE__, __func__, */
  /*     (int) GST_QUERY_TYPE (query)); */

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_CONVERT:
      return FALSE;
    default:
      return gst_pad_query_default (pad, parent, query);
  }
}

static gboolean
gst_dmss_demux_src_query (GstPad * pad, GstObject * parent, GstQuery * query)
{
  gboolean ret;
  GstDmssDemux *demux = GST_DMSS_DEMUX (parent);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_LATENCY:
    {
      if ((ret = gst_pad_peer_query (demux->sinkpad, query))) {
        GstClockTime min, max;
        gboolean live;

        gst_query_parse_latency (query, &live, &min, &max);

        /* GST_DEBUG_OBJECT (demux, "Peer latency: min %" */
        /*     GST_TIME_FORMAT " max %" GST_TIME_FORMAT, */
        /*     GST_TIME_ARGS (min), GST_TIME_ARGS (max)); */

        GST_LOG_OBJECT (demux, "Our latency: min %" GST_TIME_FORMAT
            ", max %" GST_TIME_FORMAT,
                            GST_TIME_ARGS (demux->latency*GST_MSECOND), GST_TIME_ARGS (demux->latency*GST_MSECOND));

        min += demux->latency * GST_MSECOND;
        if (max != GST_CLOCK_TIME_NONE)
          max += demux->latency * GST_MSECOND;

        /* GST_DEBUG_OBJECT (demux, "Calculated total latency : min %" */
        /*     GST_TIME_FORMAT " max %" GST_TIME_FORMAT, */
        /*     GST_TIME_ARGS (min), GST_TIME_ARGS (max)); */

        gst_query_set_latency (query, live, min, max);
      }

      break;
    }
    default:
      ret = gst_pad_query_default (pad, parent, query);
      break;
  }

  return ret;
}

static gboolean
gst_dmss_demux_send_event (GstElement * element, GstEvent * event)
{
  /* GST_DEBUG ("%s:%d %s", __FILE__, __LINE__, __func__); */

  return TRUE;
}

static gboolean gst_dmss_demux_set_clock (GstElement * element,
    GstClock * clock)
{
  GstDmssDemux *demux = GST_DMSS_DEMUX (element);
  /* GST_DEBUG ("%s:%d %s %p", __FILE__, __LINE__, __func__, clock); */

  if (demux->pipeline_clock)
    gst_object_unref (demux->pipeline_clock);
  demux->pipeline_clock = clock ? gst_object_ref(clock) : NULL;
  
  return TRUE;
}

static GstClock *gst_dmss_demux_provide_clock (GstElement * element)
{
  /* GST_DEBUG ("%s:%d %s", __FILE__, __LINE__, __func__); */

  return gst_system_clock_obtain ();
}

/* decide on push or pull based scheduling */
static gboolean
gst_dmss_demux_sink_activate (GstPad * sinkpad, GstObject * parent)
{
  /* GST_DEBUG_OBJECT (sinkpad, "activating push"); */
  return gst_pad_activate_mode (sinkpad, GST_PAD_MODE_PUSH, TRUE);
}

static gboolean
gst_dmss_demux_handle_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstDmssDemux *demux = GST_DMSS_DEMUX (parent);
  gboolean res = TRUE;

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_SEGMENT:
    {
      const GstSegment *segment;

      gst_event_parse_segment (event, &segment);
      if (segment->format == GST_FORMAT_BYTES) {
        /* gst_segment_copy_into (segment, &demux->byte_segment); */
        /* demux->need_segment = TRUE; */
        /* demux->segment_seqnum = gst_event_get_seqnum (event); */

        gst_event_unref (event);
      } else {
        gst_event_unref (event);
        /* cannot accept this format */
        res = FALSE;
      }
      break;
    }
    case GST_EVENT_EOS:
      /* flush any pending data, should be nothing left. */
      gst_dmss_demux_flush (demux);
      /* forward event */
      res = gst_dmss_demux_push_event (demux, event);
      /* and clear the adapter */
      gst_adapter_clear (demux->adapter);
      break;
    case GST_EVENT_CAPS:
      gst_event_unref (event);
      break;
    default:
      res = gst_dmss_demux_push_event (demux, event);
      break;
  }

  return res;
}

static GstPad *
gst_dmss_demux_add_video_pad (GstDmssDemux * demux, GstCaps * caps)
{
  GstEvent *event;
  gchar *stream_id;

  GST_DEBUG_OBJECT (demux, "add video pad");
  
  stream_id =
    gst_pad_create_stream_id (demux->videosrcpad,
      GST_ELEMENT_CAST (demux), "video");
  event = gst_event_new_stream_start (stream_id);

  gst_dmss_demux_video_push_event (demux, event);
  g_free (stream_id);

  gst_pad_set_caps (demux->videosrcpad, caps);

  if (!demux->need_segment) {
    event = gst_event_new_segment (&demux->time_segment);
    gst_dmss_demux_video_push_event (demux, event);
  }
  
  return demux->videosrcpad;
}

static GstPad *
gst_dmss_demux_add_audio_pad (GstDmssDemux * demux, GstCaps * caps)
{
  GstEvent *event;
  gchar *stream_id;

  GST_DEBUG_OBJECT (demux, "add audio pad");
  
  demux->audiosrcpad =
      gst_pad_new_from_static_template (&audio_template,
      audio_template.name_template);

  gst_pad_set_query_function (demux->audiosrcpad,
      GST_DEBUG_FUNCPTR (gst_dmss_demux_src_query));

  gst_pad_set_event_function (demux->audiosrcpad,
      GST_DEBUG_FUNCPTR (gst_dmss_demux_handle_src_event));
  gst_pad_use_fixed_caps (demux->audiosrcpad);
  gst_pad_set_active (demux->audiosrcpad, TRUE);

  stream_id =
      gst_pad_create_stream_id (demux->audiosrcpad,
      GST_ELEMENT_CAST (demux), "audio");

  event = gst_event_new_stream_start (stream_id);

  gst_dmss_demux_audio_push_event (demux, event);
  g_free (stream_id);
  
  gst_pad_set_caps (demux->audiosrcpad, caps);

  gst_element_add_pad (GST_ELEMENT (demux), demux->audiosrcpad);

  if (!demux->need_segment) {
    event = gst_event_new_segment (&demux->time_segment);
    gst_dmss_demux_audio_push_event (demux, event);
  }

  return demux->audiosrcpad;
}

/* streaming operation: 
 *
 * accumulate data until we have a frame, then decode. 
 */
static GstFlowReturn
gst_dmss_demux_chain (GstPad * pad, GstObject * parent, GstBuffer * buffer)
{
  GstDmssDemux *demux = GST_DMSS_DEMUX (parent);
  GstMapInfo map;
  gssize body_size;
  int const prologue_size = 32;
  GstBuffer *outbuf;

  if (gst_buffer_get_size(buffer) < prologue_size)
    return GST_FLOW_OK;  
  
  gst_buffer_map (buffer, &map, GST_MAP_READ);

  GST_DEBUG ("buffer received with command %.02x", (unsigned char) map.data[0]);
  if (((unsigned char *) map.data)[0] == (unsigned char) 0xbc) {
    body_size = GUINT32_FROM_LE (*(guint32 *) & map.data[4]);
    GST_DEBUG ("buffer received with DHAV payload size %d", (int) body_size);

    if (!body_size)
      goto discard_mapped_buffer;

    gst_buffer_unmap (buffer, &map);

    outbuf = gst_buffer_make_writable (buffer);
    gst_buffer_resize (outbuf, prologue_size,
                       gst_buffer_get_size (outbuf) - prologue_size);
    gst_adapter_push (demux->adapter, outbuf);

    gst_dmss_demux_flush (demux);

    return GST_FLOW_OK;
  } else {
    gst_buffer_unmap (buffer, &map);
  }

  return GST_FLOW_OK;
discard_mapped_buffer:
  gst_buffer_unmap (buffer, &map);
  return GST_FLOW_OK;
}

/* handle an event on the source pad, it's most likely a seek */
static gboolean
gst_dmss_demux_handle_src_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  gboolean res = FALSE;
  GstDmssDemux *demux = GST_DMSS_DEMUX (parent);

  switch (GST_EVENT_TYPE (event)) {
    /* case GST_EVENT_QOS: */
    /* { */
    /*   GstQOSType type; */
    /*   gdouble proportion; */
    /*   GstClockTimeDiff diff; */
    /*   GstClockTime timestamp; */

    /*   gst_event_parse_qos (event, &type, &proportion, &diff, &timestamp); */

    /*   GST_WARNING ("QoS type: %d timestamp %" GST_TIME_FORMAT " diff %" */
    /*       GST_TIME_FORMAT, (int) type, GST_TIME_ARGS (timestamp), */
    /*       GST_TIME_ARGS (diff)); */

    /*   if (type == GST_QOS_TYPE_UNDERFLOW) { */
    /*     GST_WARNING ("New latency: %" GST_TIME_FORMAT, */
    /*         GST_TIME_ARGS (demux->avg_latency + demux->jitter + DMSS_DEFAULT_LATENCY)); */
    /*     res = */
    /*         gst_element_send_event (GST_ELEMENT (demux), */
    /*         gst_event_new_latency (demux->avg_latency + demux->jitter + DMSS_DEFAULT_LATENCY)); */
    /*     res = */
    /*         gst_pad_push_event (demux->sinkpad, */
    /*         gst_event_new_latency (demux->avg_latency + demux->jitter + DMSS_DEFAULT_LATENCY)); */
    /*   } else { */
    /*     res = gst_pad_push_event (demux->sinkpad, event); */
    /*   } */
    /*   break; */
    /* } */
    default:
      res = gst_pad_push_event (demux->sinkpad, event);
      break;
  }

  return res;
}

void gst_dmss_demux_resync (GstDmssDemux *demux, guint16 frame_epoch, guint16 frame_ts, GstClockTime current_time,
    GstClockTime send_base_time, GstClockTime timestamp)
{
  demux->base_time = current_time;
  demux->last_ts = frame_ts;

  demux->send_base_time = send_base_time;
  demux->last_timestamp = timestamp;
}

GstClockTime gst_dmss_demux_calculate_pts (GstDmssDemux *demux, guint16 frame_epoch, guint16 frame_ts)
{
  GstClockTime current_time;
  GstClockTime timestamp/*, send_base_time = demux->send_base_time*/;
  GstClockTime timestamp_recv, timestamp_send/*, latency*//*, delta_latency*/;
  guint16 ring_diff_ts, reverse_ring_diff_ts/*, old_latency*/;
  int diff_ts;
  
  if (!demux->pipeline_clock)
    {
      GST_ERROR_OBJECT (demux, "No pipeline clock!");
      abort();
    }

/* resync: */
  current_time = gst_clock_get_time(demux->pipeline_clock);

  if (demux->need_segment)
  {
    timestamp = frame_epoch;
    timestamp *= GST_SECOND;
    // The rest of the division is used here to avoid negative timestamp
    timestamp += (((guint64) frame_ts) % 1000) * GST_MSECOND;

    gst_dmss_demux_segment_init (demux, timestamp);

    gst_dmss_demux_resync (demux, frame_epoch, frame_ts, current_time, timestamp/* - 30*GST_MSECOND*/, timestamp);
  }
  
  // ts are in milliseconds
  {
    ring_diff_ts = frame_ts - demux->last_ts;
    reverse_ring_diff_ts = demux->last_ts - frame_ts;

    GST_DEBUG_OBJECT (demux, "ring_diff_ts %d reverse_ring_diff_ts %d",
                      ring_diff_ts, reverse_ring_diff_ts);
    
    if (ring_diff_ts <= 1000)
      diff_ts = ring_diff_ts;
    else if (reverse_ring_diff_ts <= 1000)
      // going back in time (audio frames, likely)
      //diff_ts = -(int) reverse_ring_diff_ts;
      diff_ts = 0;
    else
    {
      GST_ERROR_OBJECT (demux, "Should resync last_ts %d frame_ts %d", (int)demux->last_ts, (int)frame_ts);

      timestamp = frame_epoch;
      timestamp *= GST_SECOND;
      // The rest of the division is used here to avoid negative timestamp
      timestamp += (((guint64) frame_ts) % 1000) * GST_MSECOND;

      gst_dmss_demux_resync (demux, frame_epoch, frame_ts, demux->base_time,
          timestamp - (current_time - demux->base_time), timestamp);
      diff_ts = frame_ts - demux->last_ts;
    }
    
    timestamp = demux->last_timestamp;
    timestamp += (guint64) diff_ts * GST_MSECOND;
  }

  /* if (timestamp < demux->send_base_time) */
  /*   timestamp_send = 0; */
  /* else */
  /*   timestamp_send = timestamp - demux->send_base_time; */
  timestamp_send = timestamp;
  timestamp_recv = current_time - demux->base_time;

  GST_DEBUG ("timestamp_recv: %" GST_TIME_FORMAT
             " timestamp_send: %" GST_TIME_FORMAT
             " arrived (before) %s%" GST_TIME_FORMAT,
             GST_TIME_ARGS(timestamp_recv),
             GST_TIME_ARGS(timestamp_send),
             (timestamp_recv <= timestamp_send ? "" : "-"),
             GST_TIME_ARGS(timestamp_send < timestamp_recv
                           ? timestamp_recv - timestamp_send 
                           : timestamp_send - timestamp_recv));

  GST_DEBUG
    ("Current time in pipeline %" GST_TIME_FORMAT
     " and timestamp from packet %" GST_TIME_FORMAT
     " and latency %"GST_TIME_FORMAT
     , GST_TIME_ARGS(current_time - demux->base_time)
     , GST_TIME_ARGS(timestamp_send)
     , GST_TIME_ARGS(demux->latency*GST_MSECOND)
     );

  demux->last_ts = frame_ts;
  demux->last_timestamp = timestamp_send;
  
  return timestamp_send;
}

