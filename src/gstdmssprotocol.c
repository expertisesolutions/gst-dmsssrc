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
#include <gst/gstprotection.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "gstdmssprotocol.h"

GST_DEBUG_CATEGORY_EXTERN (dmsssrc_debug);
#define GST_CAT_DEFAULT dmsssrc_debug

int
gst_dmss_protocol_create_json_packet (char *buffer, int size,
    guint32 session_id, guint32 id, const char *fmt, ...)
{
  int calculated_buffer_size;
  va_list args;
  gchar last_char = fmt[strlen (fmt) ? strlen (fmt) - 1 : 0];

  va_start (args, fmt);

  if (buffer)
    calculated_buffer_size = vsnprintf (&buffer[32], size - 32, fmt, args) + 32;
  else
    calculated_buffer_size = vsnprintf (NULL, 0, fmt, args) + 32;

  if (calculated_buffer_size <= size) {
    g_assert (buffer[calculated_buffer_size - 1] == 0);
    buffer[calculated_buffer_size - 1] = last_char;     // String gets truncated
    memset (buffer, 0, 32);
    buffer[0] = 0xf6;
    GST_WRITE_UINT32_LE (&buffer[4], (calculated_buffer_size - 32));
    GST_WRITE_UINT32_LE (&buffer[8], id);
    GST_WRITE_UINT32_LE (&buffer[16], (calculated_buffer_size - 32));
    GST_WRITE_UINT32_LE (&buffer[24], session_id);
  }

  return calculated_buffer_size;
}

int
gst_dmss_protocol_create_new_packet (char *buffer, int size, const char *fmt,
    ...)
{
  int calculated_buffer_size;
  va_list args;
  gchar last_char = fmt[strlen (fmt) ? strlen (fmt) - 1 : 0];

  va_start (args, fmt);

  if (buffer)
    calculated_buffer_size = vsnprintf (&buffer[32], size - 32, fmt, args) + 32;
  else
    calculated_buffer_size = vsnprintf (NULL, 0, fmt, args) + 32;

  if (calculated_buffer_size <= size) {
    g_assert (buffer[calculated_buffer_size - 1] == 0);
    buffer[calculated_buffer_size - 1] = last_char;     // String gets truncated
    memset (buffer, 0, 32);
    buffer[0] = 0xf4;
    GST_WRITE_UINT32_LE (&buffer[4], (calculated_buffer_size - 32));
  }

  return calculated_buffer_size;
}

gssize
gst_dmss_protocol_receive_packet_no_body (GSocket * socket,
    GCancellable * cancellable, GError ** err, gchar * buffer)
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
        g_set_error_literal (err, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
            "Read operation timed out");
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
gst_dmss_protocol_receive_packet (GSocket * socket, GCancellable * cancellable,
    GError ** err, gchar * ext_buffer, gssize * ext_size)
{
  gssize size, body_size, ring_size;
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

  GST_DEBUG ("Received packet header with body of size %" G_GSSIZE_FORMAT
      " and command %.02x", body_size,
      (unsigned int) (unsigned char) buffer[0]);

  ring_size = *ext_size - 32;
  offset = 0;
  while (offset != body_size) {
    if ((size =
            g_socket_receive (socket, &ext_buffer[(offset % ring_size) + 32],
                // if remaining bytes to be read is bigger than
                // than free space in ring, read only remaining
                // ring space, otherwise read all that remains
                (body_size - offset) > ring_size - (offset % ring_size) ?
                ring_size - (offset % ring_size) :
                (body_size - offset), cancellable, err)) <= 0) {
      if (!*err && !size)
        g_set_error_literal (err, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
            "Read operation timed out");
      goto recv_error;
    }

    offset += size;
  }

  // null-terminate if there's enough space
  if (*ext_size > 32 + body_size)
    ext_buffer[32 + body_size] = 0;

  GST_DEBUG ("Received packet body\n%s\n", &ext_buffer[32]);
  return (*ext_size = 32 + body_size);
recv_error:
  return -1;
}
