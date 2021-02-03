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

/*
TransactionID:53
Method:GetParameterNames
ParameterName:Dahua.Device.Network.PlayBack.General
channel:1
ConnectionID:13547935
StartTime:2018&9&22&11&29&0
EndTime:2018&9&22&12&0&0
DriveNo:9
ClusterNo:589005
Hint:1
Type:0
IFrames:0
IsTime:1
OffLength:0
Direction:0

Ukey:

{ "id" : 5399, "method" : "mediaFileFind.findNextFile", "object" : 1077428704, "params" : { "count" : 32, "object" : 1077428704, "this" : 1077428704 }, "session" : 1142850372 }


{"id":5399,"params"
:{"found":32,"infos":
[ {"Channel":0,"Cluster":589005,"Disk":9,"EndTime":"2018-09-22 12:00:00","FilePath":"\/mnt\/dvr\/2018-09-22\/000\/dav\/11\/1\/1\/589005\/11.00.00-12.00.00[R][0@0][0].dav","Flags":["Timing"],"Length":1908146176,"Partition":1,"StartTime":"2018-09-22 11:00:00","Type":"dav","VideoStream":"Main"},
  {"Channel":0,"Cluster":611164,"Disk":9,"EndTime":"2018-09-22 13:00:00","FilePath":"\/mnt\/dvr\/2018-09-22\/000\/dav\/12\/1\/1\/611164\/12.00.00-13.00.00[R][0@0][0].dav","Flags":["Timing"],"Length":1908146176,"Partition":1,"StartTime":"2018-09-22 12:00:00","Type":"dav","VideoStream":"Main"},
  {"Channel":0,"Cluster":632824,"Disk":9,"EndTime":"2018-09-22 14:00:00","FilePath":"\/mnt\/dvr\/2018-09-22\/000\/dav\/13\/1\/1\/632824\/13.00.00-14.00.00[R][0@0][0].dav","Flags":["Timing"],"Length":1908146176,"Partition":1,"StartTime":"2018-09-22 13:00:00","Type":"dav","VideoStream":"Main"},
  {"Channel":0,"Cluster":654189,"Disk":9,"EndTime":"2018-09-22 15:00:00","FilePath":"\/mnt\/dvr\/2018-09-22\/000\/dav\/14\/1\/1\/654189\/14.00.00-15.00.00[R][0@0][0].dav","Flags":["Timing"],"Length":1907752960,"Partition":1,"StartTime":"2018-09-22 14:00:00","Type":"dav","VideoStream":"Main"},
  {"Channel":0,"Cluster":675316,"Disk":9,"EndTime":"2018-09-22 15:40:18","FilePath":"\/mnt\/dvr\/2018-09-22\/000\/dav\/15\/1\/1\/675316\/15.00.00-15.40.18[R][0@0][0].dav","Flags":["Timing"],"Length":1281753088,"Pa"


{ "id" : 4887, "method" : "mediaFileFind.findFile", "object" : 1077428704, "params" : { "condition" : { "Channel" : 0, "EndTime" : "2018-09-24 12:29:00", "Flags" : [ "*" ], "StartTime" : "2018-09-22 11:29:00", "Types" : [ "dav" ], "VideoStream" : "Main" } }, "session" : 1142850372 }

{ "id" : 4395, "method" : "mediaFileFind.factory.create", "params" : null, "session" : 1142850372 }
{"id":4395,"result":1077428704,"session":1142850372}
 */

/**
 * SECTION:element-dmsssrc
 * @title: dmsssrc
 * @see_also: #dmssmuxer
 *
 * ## Example launch line (client):
 * |[
 * gst-launch-1.0 dmsssrc port=37777 host=192.168.1.108 username=admin password=admin ! 
 * ]|
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gstversion.h>
#if GST_VERSION_MINOR <= 15
#include <gst/gst-i18n-plugin.h>
#endif
#include "gstdmsssrc.h"
#include "gstdmssprotocol.h"
#include "gstdmss.h"

#include <stdio.h>
#include <string.h>

GST_DEBUG_CATEGORY (dmsssrc_debug);
#define GST_CAT_DEFAULT dmsssrc_debug

#define DMSS_DEFAULT_TIMEOUT            0

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-dmss"));

enum
{
  PROP_0,
  PROP_HOST,
  PROP_PORT,
  PROP_USER,
  PROP_PASSWORD,
  PROP_TIMEOUT,
  PROP_CHANNEL,
  PROP_SUBCHANNEL
};

#define gst_dmss_src_parent_class parent_class
G_DEFINE_TYPE (GstDmssSrc, gst_dmss_src, GST_TYPE_PUSH_SRC);

static void gst_dmss_src_finalize (GObject * gobject);

static GstFlowReturn gst_dmss_src_create (GstPushSrc * psrc,
    GstBuffer ** outbuf);
static gboolean gst_dmss_src_stop (GstBaseSrc * bsrc);
static gboolean gst_dmss_src_start (GstBaseSrc * bsrc);

static void gst_dmss_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_dmss_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void
gst_dmss_src_class_init (GstDmssSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasesrc_class = (GstBaseSrcClass *) klass;
  gstpushsrc_class = (GstPushSrcClass *) klass;

  gobject_class->set_property = gst_dmss_src_set_property;
  gobject_class->get_property = gst_dmss_src_get_property;
  gobject_class->finalize = gst_dmss_src_finalize;

  g_object_class_install_property (gobject_class, PROP_HOST,
      g_param_spec_string ("host", "Host",
          "The host IP address to camera or NVR", DMSS_DEFAULT_HOST,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_USER,
      g_param_spec_string ("user", "User",
          "Username to authenticate with camera", DMSS_DEFAULT_USER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PASSWORD,
      g_param_spec_string ("password", "Password",
          "Password to authenticate with camera", DMSS_DEFAULT_PASSWORD,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PORT,
      g_param_spec_int ("port", "Port", "Port number, default is 37777", 0,
          DMSS_HIGHEST_PORT, DMSS_DEFAULT_PORT, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
      g_param_spec_uint ("timeout", "timeout",
          "Value in seconds to timeout a blocking I/O. 0 = No timeout. ", 0,
          G_MAXUINT, DMSS_DEFAULT_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_CHANNEL,
      g_param_spec_uint ("channel", "Channel",
          "Channel to read", 0,
          G_MAXUINT, DMSS_DEFAULT_CHANNEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SUBCHANNEL,
      g_param_spec_uint ("subchannel", "Subchannel",
          "Sub-channel to read", 0,
          G_MAXUINT, DMSS_DEFAULT_SUBCHANNEL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (gstelement_class, &srctemplate);

  gst_element_class_set_metadata (gstelement_class,
      "DMSS client source",
      "Source for IP Camera",
      "Receive data from IP camera",
      "Felipe Magno de Almeida <felipe@expertisesolutions.com.br>");

  gstbasesrc_class->start = gst_dmss_src_start;
  gstbasesrc_class->stop = gst_dmss_src_stop;
  gstpushsrc_class->create = gst_dmss_src_create;

  GST_DEBUG_CATEGORY_INIT (dmsssrc_debug, "dmsssrc", 0, "DMSS Client Source");
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_dmss_src_init (GstDmssSrc * src)
{
  src->port = DMSS_DEFAULT_PORT;
  src->host = g_strdup (DMSS_DEFAULT_HOST);
  src->user = g_strdup (DMSS_DEFAULT_USER);
  src->password = g_strdup (DMSS_DEFAULT_PASSWORD);
  src->timeout = DMSS_DEFAULT_TIMEOUT;
  src->control_socket = NULL;
  src->stream_socket = NULL;
  src->cancellable = g_cancellable_new ();
  src->channel = 0;
  src->subchannel = 0;
#if 1
  src->bytes_downloaded = 0;
#endif

  src->system_clock = gst_system_clock_obtain ();
  src->last_ack_time = GST_CLOCK_TIME_NONE;

  GST_OBJECT_FLAG_UNSET (src, GST_DMSS_SRC_CONTROL_OPEN);
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);
}

static void
gst_dmss_src_finalize (GObject * gobject)
{
  GstDmssSrc *this = GST_DMSS_SRC (gobject);

  if (this->cancellable)
    g_object_unref (this->cancellable);
  this->cancellable = NULL;
  if (this->control_socket)
    g_object_unref (this->control_socket);
  this->control_socket = NULL;
  if (this->stream_socket)
    g_object_unref (this->stream_socket);
  this->stream_socket = NULL;
  g_free (this->host);
  this->host = NULL;
  g_free (this->user);
  this->user = NULL;
  g_free (this->password);
  this->password = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
gst_dmss_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDmssSrc *src = GST_DMSS_SRC (object);

  switch (prop_id) {
    case PROP_HOST:
      if (!g_value_get_string (value)) {
        g_warning ("host property cannot be NULL");
        break;
      }
      g_free (src->host);
      src->host = g_strdup (g_value_get_string (value));
      break;
    case PROP_USER:
      if (!g_value_get_string (value)) {
        g_warning ("user property cannot be NULL");
        break;
      }
      g_free (src->user);
      src->user = g_strdup (g_value_get_string (value));
      break;
    case PROP_PASSWORD:
      if (!g_value_get_string (value)) {
        g_warning ("password property cannot be NULL");
        break;
      }
      g_free (src->password);
      src->password = g_strdup (g_value_get_string (value));
      break;
    case PROP_PORT:
      src->port = g_value_get_int (value);
      break;
    case PROP_TIMEOUT:
      src->timeout = g_value_get_uint (value);
      break;
    case PROP_CHANNEL:
      src->channel = g_value_get_uint (value);
      break;
    case PROP_SUBCHANNEL:
      src->subchannel = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_dmss_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstDmssSrc *src = GST_DMSS_SRC (object);

  switch (prop_id) {
    case PROP_HOST:
      g_value_set_string (value, src->host);
      break;
    case PROP_USER:
      g_value_set_string (value, src->user);
      break;
    case PROP_PASSWORD:
      g_value_set_string (value, src->password);
      break;
    case PROP_PORT:
      g_value_set_int (value, src->port);
      break;
    case PROP_TIMEOUT:
      g_value_set_uint (value, src->timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_dmss_src_stop (GstBaseSrc * bsrc)
{
  GstDmssSrc *this = GST_DMSS_SRC (bsrc);
  GError *error;

  if (this->control_socket) {
    g_socket_close (this->control_socket, &error);
    g_object_unref (this->control_socket);
  }
  if (this->stream_socket) {
    g_socket_close (this->stream_socket, &error);
    g_object_unref (this->stream_socket);
  }
  this->control_socket = NULL;
  this->stream_socket = NULL;

  return TRUE;
}

static GstFlowReturn
gst_dmss_src_create (GstPushSrc * psrc, GstBuffer ** outbuf)
{
  GstDmssSrc *src;
  GstFlowReturn ret = GST_FLOW_OK;
  GError *err = NULL;
  gssize body_size, size, offset;
  gchar prologue[32];
  GstMapInfo map;
  GstClockTime current_time;
  static gchar const noop_buffer[32]
      = {
    0xa1, 0,
  };

  src = GST_DMSS_SRC (psrc);

  GST_LOG_OBJECT (src, "Going to read data from stream");
  
  current_time = gst_clock_get_time (src->system_clock);

  if (!GST_CLOCK_TIME_IS_VALID (src->last_ack_time) ||
      current_time - src->last_ack_time > GST_SECOND) {
    // send no-op packet
    if (!g_socket_send (src->control_socket, noop_buffer, sizeof (noop_buffer),
            src->cancellable, &err))
      goto control_socket_error;
    GST_LOG_OBJECT (src, "Sent nope packet for keep-alive");
    src->last_ack_time = current_time;

#if 1
    GST_LOG_OBJECT (src, "Download rate of %d Bps", src->bytes_downloaded);
    src->bytes_downloaded = 0;
#endif
  }

  GST_INFO_OBJECT (src, " ");

  if (!GST_OBJECT_FLAG_IS_SET (src, GST_DMSS_SRC_CONTROL_OPEN))
    goto wrong_state;

  GST_LOG_OBJECT (src, "Receiving data from socket with blocking");
  if ((body_size =
          gst_dmss_receive_packet_no_body (src->stream_socket, src->cancellable,
              &err, prologue)) <= 0) {
    if (!err && !body_size)
      g_set_error_literal (&err, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED,
          "Connection closed by remote peer");
    GST_LOG_OBJECT (src, "Error receiving header");
    g_assert (err != NULL);
    goto recv_error;
  }
  GST_LOG_OBJECT (src, "Received header");

  g_assert (body_size == GUINT32_FROM_LE (*(guint32 *) & prologue[4]));

  //gst_dmss_debug_print_prologue(prologue);
  GST_DEBUG_OBJECT (src,
      "Received prologue packet with command %.02x. body size %d",
      (unsigned int) (unsigned char) prologue[0], (int) body_size);

#if 1
  src->bytes_downloaded += sizeof (prologue) + body_size;
#endif
  *outbuf = gst_buffer_new_and_alloc (sizeof (prologue) + body_size);
  gst_buffer_map (*outbuf, &map, GST_MAP_READWRITE);

  memcpy (map.data, prologue, sizeof (prologue));

  offset = 0;
  do {
    GST_LOG_OBJECT (src, "Receiving data from socket with blocking");
    if ((size =
            g_socket_receive (src->stream_socket,
                (gchar *) & map.data[offset + sizeof (prologue)],
                body_size - offset, src->cancellable, &err)) <= 0) {
      GST_LOG_OBJECT (src, "Error receiving header");
      if (!err && !size)
        g_set_error_literal (&err, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
            "Read operation timed out");
      g_assert (err != NULL);
      goto recv_error;
    }
    GST_LOG_OBJECT (src, "Received partial body");
    offset += size;
  } while (offset != body_size);

  GST_DEBUG_OBJECT (src, "Received body with %d", (int) offset);

  gst_buffer_unmap (*outbuf, &map);
  gst_buffer_resize (*outbuf, 0, sizeof (prologue) + body_size);

  GST_LOG_OBJECT (src,
      "Returning buffer from _get of size %" G_GSIZE_FORMAT ", ts %"
      GST_TIME_FORMAT ", dur %" GST_TIME_FORMAT
      ", offset %" G_GINT64_FORMAT ", offset_end %" G_GINT64_FORMAT
      ", still available bytes in socket: %d",
      gst_buffer_get_size (*outbuf),
      GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (*outbuf)),
      GST_TIME_ARGS (GST_BUFFER_DURATION (*outbuf)),
      GST_BUFFER_OFFSET (*outbuf), GST_BUFFER_OFFSET_END (*outbuf),
      (int) g_socket_get_available_bytes (src->stream_socket));

  return ret;
control_socket_error:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("failed reading from control socket: %s", err->message));
    g_object_unref (src->control_socket);
    if (src->stream_socket)
      g_object_unref (src->stream_socket);
    src->control_socket = NULL;
    src->stream_socket = NULL;
    return GST_FLOW_ERROR;
  }
recv_error:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("failed reading from socket: %s", err->message));
    g_object_unref (src->control_socket);
    if (src->stream_socket)
      g_object_unref (src->stream_socket);
    src->control_socket = NULL;
    src->stream_socket = NULL;
    return GST_FLOW_ERROR;
  }
wrong_state:
  {
    GST_DEBUG_OBJECT (src, "connection closed, cannot read data");
    g_object_unref (src->control_socket);
    if (src->stream_socket)
      g_object_unref (src->stream_socket);
    src->control_socket = NULL;
    src->stream_socket = NULL;
    return GST_FLOW_FLUSHING;
  }
}

static int
gst_dmss_src_new_protocol_find_value_size (gchar * buffer, int buffer_size,
    GError ** err)
{
  int offset = 0;
  while (offset != buffer_size && buffer[offset] != '\r')
    ++offset;

  return offset;
}

static gchar *
gst_dmss_src_new_protocol_find_prefix (GstDmssSrc * src, gchar * buffer,
    int buffer_size, gchar * prefix, int prefix_size, GError ** err)
{
  int offset = 0;

  while (buffer_size - offset > prefix_size
      && memcmp (&buffer[offset], prefix, prefix_size)) {
    GST_DEBUG_OBJECT (src, "Not found at %s", &buffer[offset]);
    while (buffer[offset] != '\n' && buffer_size != offset)
      ++offset;
    if (buffer_size != offset)
      ++offset;
  }

  if (buffer_size - offset <= prefix_size)
    goto error;

  offset += prefix_size;

  return &buffer[offset];
error:
  return NULL;
}

static int
gst_dmss_src_new_protocol_link_subchannel (GstDmssSrc * src, GError ** err)
{
  gchar ack_subchannel_template[] =
      "TransactionID:2\r\n"
      "Method:GetParameterNames\r\n"
      "ParameterName:Dahua.Device.Network.ControlConnection.AckSubChannel\r\n"
      "SessionID:%d\r\n" "ConnectionID:%s\r\n" "\r\n";
  gchar *new_command_buffer = NULL;
  int buffer_size = 0;
  gchar extension_recv[255] = { 0, };
  gint offset, size;

  buffer_size = gst_dmss_protocol_create_new_packet (new_command_buffer, 0,
      ack_subchannel_template, src->session_id, src->connection_id);

  new_command_buffer = g_malloc (buffer_size + 1);

  buffer_size = gst_dmss_protocol_create_new_packet (new_command_buffer,
      buffer_size, ack_subchannel_template, src->session_id,
      src->connection_id);

  new_command_buffer[buffer_size] = 0;

  GST_DEBUG_OBJECT (src, " Sending new packet with body\n%s\n",
      (new_command_buffer + 32));

  // send stream start command
  if (!g_socket_send (src->stream_socket, new_command_buffer,
          buffer_size, src->cancellable, err))
    goto error;

  if ((buffer_size =
          gst_dmss_receive_packet_no_body (src->stream_socket, src->cancellable,
              err, new_command_buffer)) < 0)
    goto error;

  GST_DEBUG_OBJECT (src, "Ack subchannel header response");

  g_free (new_command_buffer);

  offset = 0;
  do {
    if ((size = g_socket_receive (src->stream_socket, &extension_recv[offset],
                buffer_size - offset, src->cancellable, err)) <= 0) {
      if (!*err && !size)
        g_set_error_literal (err, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
            "Read operation timed out");
      goto error;
    }

    offset += size;
  } while (offset != buffer_size);

  GST_DEBUG_OBJECT (src, "ack subchannel response %s", extension_recv);

  return 0;
error:
  return -1;
}

static int
gst_dmss_src_add_object (GstDmssSrc * src, GError ** err)
{
  gchar cmd_buffer[]
      =
      "\xf4\x00\x00\x00"
      "\x76\x00\x00\x00"
      "\x00\x00\x00\x00"
      "\x00\x00\x00\x00"
      "\x00\x00\x00\x00"
      "\x00\x00\x00\x00"
      "\x00\x00\x00\x00"
      "\x00\x00\x00\x00"
      "TransactionID:1\r\n"
      "Method:AddObject\r\n"
      "ParameterName:Dahua.Device.Network.ControlConnection.Passive\r\n"
      "ConnectProtocol:0\r\n\r\n";
  gchar extension_recv[255] = { 0, };
  int buffer_size;
  gchar error_status[] = "FaultCode:";
  gchar connection_id_prefix[] = "ConnectionID:";
  gchar ok_status[] = "OK";
  int offset;
  int size;
  gchar *status_buffer;
  gchar *connection_id_buffer;
  int connection_id_value_size;

  if (!g_socket_send (src->control_socket, cmd_buffer,
          sizeof (cmd_buffer) - 1, src->cancellable, err))
    goto error;

  GST_DEBUG_OBJECT (src, "Sent add object");

  if ((buffer_size =
          gst_dmss_receive_packet_no_body (src->control_socket,
              src->cancellable, err, cmd_buffer)) < 0)
    goto error;

  GST_DEBUG_OBJECT (src, "Received header response");

  if ((unsigned char) cmd_buffer[0] == (unsigned char) 0xf4)
    GST_DEBUG_OBJECT (src,
        "Received header response with correct cmd with body size: %d",
        (int) buffer_size);

  if (buffer_size > sizeof (extension_recv))
    goto error;

  offset = 0;
  do {
    if ((size = g_socket_receive (src->control_socket, &extension_recv[offset],
                buffer_size - offset, src->cancellable, err)) <= 0) {
      if (!*err && !size)
        g_set_error_literal (err, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
            "Read operation timed out");
      goto error;
    }
    offset += size;
  } while (offset != buffer_size);

  GST_DEBUG_OBJECT (src, "Received body of response %s", extension_recv);

  offset = 0;
  // search for line with error status
  status_buffer =
      gst_dmss_src_new_protocol_find_prefix (src, extension_recv, buffer_size,
      error_status, sizeof (error_status) - 1, err);

  GST_DEBUG_OBJECT (src, "Found prefix at %s", status_buffer);

  if (!status_buffer
      || buffer_size - (status_buffer - extension_recv) <
      sizeof (ok_status) - 1)
    goto error_status;

  if (memcmp (status_buffer, ok_status, sizeof (ok_status) - 1))
    goto error_status;

  GST_DEBUG_OBJECT (src, "Value is OK");

  // search for line with connectionId
  connection_id_buffer =
      gst_dmss_src_new_protocol_find_prefix (src, extension_recv, buffer_size,
      connection_id_prefix, sizeof (connection_id_prefix) - 1, err);

  GST_DEBUG_OBJECT (src, "Found prefix at %s", connection_id_buffer);

  if (!connection_id_buffer)
    goto error_status;

  connection_id_value_size =
      gst_dmss_src_new_protocol_find_value_size (connection_id_buffer,
      buffer_size - (connection_id_buffer - extension_recv), err);

  memcpy (src->connection_id, connection_id_buffer,
      connection_id_value_size <
      sizeof (connection_id_prefix) ? connection_id_value_size :
      sizeof (connection_id_prefix) - 1);

  return sizeof (cmd_buffer);
error:
  return -1;
error_status:
  return -1;
}

static gboolean
gst_dmss_src_start (GstBaseSrc * bsrc)
{
  GstDmssSrc *src = GST_DMSS_SRC (bsrc);
  GError *err = NULL;
  GInetAddress *addr;
  GSocketAddress *saddr;
  GResolver *resolver;
  guint32 const userpass_size = 2 + strlen (src->user) + strlen (src->password);
  gchar login_buffer[32]
      = {
    0xa0, 0x00, 0x00, 0x60,
    (userpass_size & 0x000000FF),
    (userpass_size & 0x0000FF00) >> 8,
    (userpass_size & 0x00FF0000) >> 16,
    (userpass_size & 0xFF000000) >> 24,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0x04, 0x02, 0x03, 0x00, 0x01, 0xa1, 0xaa
  };
  static gchar const noop_buffer[32]
      = {
    0xa1, 0,
  };
  gchar const stream_start_template[] =
      "TransactionID:100\r\n"
      "Method:GetParameterNames\r\n"
      "ParameterName:Dahua.Device.Network.Monitor.General\r\n"
      "channel:%d\r\n"
      "state:1\r\n" "ConnectionID:%s\r\n" "stream:%d\r\n" "\r\n";
  gchar *new_command_buffer;
  gchar login_separator[2] = { '&', '&' };
  gchar prefix_buffer[32];
  gssize receive_size;
  gchar login_symbol[4];
  int size;

  /* look up name if we need to */
  addr = g_inet_address_new_from_string (src->host);
  if (!addr) {
    GList *results;

    resolver = g_resolver_get_default ();

    results =
        g_resolver_lookup_by_name (resolver, src->host, src->cancellable, &err);
    if (!results)
      goto name_resolve;
    addr = G_INET_ADDRESS (g_object_ref (results->data));

    g_resolver_free_addresses (results);
    g_object_unref (resolver);
  }
#ifndef GST_DISABLE_GST_DEBUG
  {
    gchar *ip = g_inet_address_to_string (addr);

    GST_DEBUG_OBJECT (src, "IP address for host %s is %s", src->host, ip);
    g_free (ip);
  }
#endif

  saddr = g_inet_socket_address_new (addr, src->port);
  g_object_unref (addr);

  /* create receiving client socket */
  GST_DEBUG_OBJECT (src, "opening receiving control socket to %s:%d",
      src->host, src->port);

  g_assert (src->control_socket == NULL);
  g_assert (src->stream_socket == NULL);

  src->control_socket =
      g_socket_new (g_socket_address_get_family (saddr), G_SOCKET_TYPE_STREAM,
      G_SOCKET_PROTOCOL_TCP, &err);
  if (!src->control_socket)
    goto no_socket;

  g_socket_set_timeout (src->control_socket, src->timeout);

  GST_DEBUG_OBJECT (src, "opened receiving control socket");

  /* connect to server */
  if (!g_socket_connect (src->control_socket, saddr, src->cancellable, &err))
    goto connect_failed;

  g_object_unref (saddr);

  // go for authentication
  if (!g_socket_send (src->control_socket, login_buffer, sizeof (login_buffer),
          src->cancellable, &err))
    goto login_error;

  if (!g_socket_send (src->control_socket, src->user, strlen (src->user),
          src->cancellable, &err))
    goto login_error;

  if (!g_socket_send (src->control_socket, login_separator,
          sizeof (login_separator), src->cancellable, &err))
    goto login_error;

  if (!g_socket_send (src->control_socket, src->password,
          strlen (src->password), src->cancellable, &err))
    goto login_error;

  GST_DEBUG_OBJECT (src,
      "sent authentication info, waiting authentication response");

  receive_size = sizeof (prefix_buffer);
  if (gst_dmss_receive_packet (src->control_socket, src->cancellable, &err,
          prefix_buffer, &receive_size) < 0)
    goto login_error;

  g_assert (receive_size == sizeof (prefix_buffer));

  memcpy (login_symbol, &prefix_buffer[16], sizeof (login_symbol));
  src->session_id = GUINT32_FROM_LE (*(guint32 *) login_symbol);

  if (prefix_buffer[8])
    goto authentication_error;

  GST_DEBUG_OBJECT (src, "authenticated in control socket");

  // send noop command
  if (!g_socket_send (src->control_socket, noop_buffer, sizeof (noop_buffer),
          src->cancellable, &err))
    goto login_error;

  // wait for response of noop operation
  do {
    if (gst_dmss_receive_packet (src->control_socket, src->cancellable, &err,
            prefix_buffer, &receive_size) < 0)
      goto login_error;
    g_assert (receive_size == 32);

    GST_DEBUG_OBJECT (src, "package received in control socket with command %d",
        (unsigned int) (unsigned char) prefix_buffer[0]);

  } while ((unsigned char) prefix_buffer[0] != (unsigned char) 0xb1);

  // connect stream socket
  GST_DEBUG_OBJECT (src, "opening stream receiving client socket to %s:%d",
      src->host, src->port);

  src->stream_socket =
      g_socket_new (g_socket_address_get_family (saddr), G_SOCKET_TYPE_STREAM,
      G_SOCKET_PROTOCOL_TCP, &err);
  if (!src->stream_socket)
    goto no_socket;

  g_socket_set_timeout (src->stream_socket, src->timeout);

  GST_DEBUG_OBJECT (src, "opened receiving stream socket");
  GST_OBJECT_FLAG_SET (src, GST_DMSS_SRC_CONTROL_OPEN);

  if ((size = gst_dmss_src_add_object (src, &err)) < 0)
    goto login_error;

  GST_DEBUG_OBJECT (src, "Added object");

  /* connect to server */
  if (!g_socket_connect (src->stream_socket, saddr, src->cancellable, &err))
    goto stream_connect_failed;

  GST_DEBUG_OBJECT (src, "Stream socket connected");

  /* memcpy (&stream_link_buffer[8], login_symbol, sizeof (login_symbol)); */

  if (gst_dmss_src_new_protocol_link_subchannel (src, &err) < 0)
    goto stream_auth_failed;

  GST_DEBUG_OBJECT (src,
      "linked stream socket. Going to start stream for channel %d and subchannel %d",
      src->channel, src->subchannel);

  // should check if response is OK
  {
    GST_DEBUG_OBJECT (src,
        "Starting stream for channel %d and subchannel %d using new protocol",
        src->channel, src->subchannel);

    size = gst_dmss_protocol_create_new_packet (NULL, 0,
        stream_start_template, src->channel, src->connection_id,
        (int) src->subchannel);

    new_command_buffer = g_malloc (size);

    size = gst_dmss_protocol_create_new_packet (new_command_buffer, size,
        stream_start_template, src->channel, src->connection_id,
        (int) src->subchannel);

    GST_DEBUG_OBJECT (src, "Sending body %s", new_command_buffer + 32);

    // send stream start command
    if (!g_socket_send (src->control_socket, new_command_buffer,
            size, src->cancellable, &err))
      goto login_error;

    //g_free (new_command_buffer);

    GST_DEBUG_OBJECT (src, "Sent start in new protocol");

    {
      int offset;
      int buffer_size;
      gchar extension_recv[255];

      if ((buffer_size =
              gst_dmss_receive_packet_no_body (src->control_socket,
                  src->cancellable, &err, new_command_buffer)) < 0)
        goto login_error;

      GST_DEBUG_OBJECT (src, "Received header %d", (int) buffer_size);

      if ((unsigned char) new_command_buffer[0] == (unsigned char) 0xf4)
        GST_DEBUG_OBJECT (src,
            "Received header response with correct cmd with body size: %d",
            (int) buffer_size);
      else
        GST_DEBUG_OBJECT (src,
            "Received header response with wrong (?) cmd %d with body size: %d",
            (unsigned int) (unsigned char) new_command_buffer[0],
            (int) buffer_size);

      if (buffer_size > sizeof (extension_recv))
        goto login_error;

      offset = 0;
      do {
        if ((size =
                g_socket_receive (src->control_socket, &extension_recv[offset],
                    buffer_size - offset, src->cancellable, &err)) <= 0) {
          if (!err && !size)
            g_set_error_literal (&err, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
                "Read operation timed out");
          goto login_error;
        }

        offset += size;
      } while (offset != buffer_size);

      GST_DEBUG_OBJECT (src, "Received body of response %s", extension_recv);
    }
  }

  g_assert (receive_size == 32);

  // should check if response is OK
  GST_DEBUG_OBJECT (src, "started stream download");

  return TRUE;
stream_connect_failed:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("Connection with stream socket failed: %s", err->message));
    gst_dmss_src_stop (GST_BASE_SRC (src));
    return FALSE;
  }
stream_auth_failed:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("Authentication in stream socket failed"));
    gst_dmss_src_stop (GST_BASE_SRC (src));
    return FALSE;
  }
authentication_error:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("Authentication failed, verify your username and password"));
    gst_dmss_src_stop (GST_BASE_SRC (src));
    return FALSE;
  }
login_error:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("Failed to send data on control socket: %s", err->message));
    gst_dmss_src_stop (GST_BASE_SRC (src));
    return FALSE;
  }
no_socket:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
        ("Failed to create socket: %s", err->message));
    g_object_unref (saddr);
    gst_dmss_src_stop (GST_BASE_SRC (src));
    return FALSE;
  }
name_resolve:
  {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG_OBJECT (src, "Cancelled name resolval");
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
          ("Failed to resolve host '%s': %s", src->host, err->message));
    }
    g_object_unref (resolver);
    gst_dmss_src_stop (GST_BASE_SRC (src));
    return FALSE;
  }
connect_failed:
  {
    if (g_error_matches (err, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      GST_DEBUG_OBJECT (src, "Cancelled connecting");
    } else {
      GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
          ("Failed to connect to host '%s:%d': %s", src->host, src->port,
              err->message));
    }
    g_object_unref (saddr);
    gst_dmss_src_stop (GST_BASE_SRC (src));
    return FALSE;
  }
}

gssize
gst_dmss_receive_packet_no_body (GSocket * socket, GCancellable * cancellable,
    GError ** err, gchar * buffer)
{
  gssize size;
  gssize offset;
  gssize body_size;
  static int const buffer_size = 32;

  g_assert (err != NULL);
  offset = 0;
  do {
    if ((size = g_socket_receive (socket, &buffer[offset],
                buffer_size - offset, cancellable, err)) <= 0) {
      if (!*err && !size)
        g_set_error_literal (err, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED,
            "Connection closed by remote peer");
      goto recv_error;
    }

    offset += size;
  }
  while (offset != buffer_size);

  body_size = GUINT32_FROM_LE (*(guint32 *) & buffer[4]);
  return body_size;             // body size
recv_error:
  return -1;
}

int
gst_dmss_receive_packet (GSocket * socket, GCancellable * cancellable,
    GError ** err, gchar * ext_buffer, gssize * ext_size)
{
  gssize size, body_size;
  gssize offset;
  gchar buffer[32];

  offset = 0;
  do {
    if ((size = g_socket_receive (socket, &buffer[offset],
                sizeof (buffer) - offset, cancellable, err)) <= 0) {
      if (!*err && !size)
        g_set_error_literal (err, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
            "Read operation timed out");
      goto recv_error;
    }

    if (offset < *ext_size)
      memcpy (&ext_buffer[offset], &buffer[offset],
          *ext_size - offset < size ? *ext_size - offset : size);

    offset += size;
  }
  while (offset != 32);

  body_size = GUINT32_FROM_LE (*(guint32 *) & buffer[4]);

  offset = 0;
  while (offset != body_size) {
    if ((size = g_socket_receive (socket, &buffer[offset % sizeof (buffer)],
                (body_size - offset) >
                sizeof (buffer) -
                (offset %
                    sizeof (buffer)) ? sizeof (buffer) -
                (offset % sizeof (buffer)) : (body_size -
                    offset), cancellable, err)) <= 0) {
      if (!*err && !size)
        g_set_error_literal (err, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
            "Read operation timed out");
      goto recv_error;
    }

    if (offset + 32 < *ext_size)
      memcpy (&ext_buffer[offset + 32], &buffer[offset + 32],
          *ext_size - (offset + 32) < size ? *ext_size - (offset + 32) : size);

    offset += size;
  }

  return 32 + body_size;
recv_error:
  return -1;
}

int
gst_dmss_receive_packet_ignore (GSocket * socket, GCancellable * cancellable,
    GError ** err)
{
  gchar buffer[32];
  gssize size = 32;
  return gst_dmss_receive_packet (socket, cancellable, err, buffer, &size);
}
