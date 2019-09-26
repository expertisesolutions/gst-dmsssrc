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

#ifndef __GST_DMSS_PROTOCOL_H__
#define __GST_DMSS_PROTOCOL_H__

#include <gio/gio.h>

int gst_dmss_protocol_create_json_packet (char *buffer, int size,
    guint32 session_id, guint32 id, const char *fmt, ...);
int gst_dmss_protocol_create_new_packet (char *buffer, int size,
    const char *fmt, ...);
gssize gst_dmss_protocol_receive_packet_no_body (GSocket * socket,
    GCancellable * cancellable, GError ** err, gchar * buffer);
int gst_dmss_protocol_receive_packet (GSocket * socket,
    GCancellable * cancellable, GError ** err, gchar * ext_buffer,
    gssize * ext_size);

#endif
