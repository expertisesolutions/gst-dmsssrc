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

#ifndef __GST_DMSS_H__
#define __GST_DMSS_H__

#include <gst/gst.h>

#define DMSS_HIGHEST_PORT        65535
#define DMSS_DEFAULT_HOST        "192.168.1.108"
#define DMSS_DEFAULT_PORT        37777
#define DMSS_DEFAULT_USER        "admin"
#define DMSS_DEFAULT_PASSWORD    "admin"
#define DMSS_DEFAULT_CHANNEL     0
#define DMSS_DEFAULT_SUBCHANNEL     0
#define DMSS_DEFAULT_LATENCY     200
#define DMSS_EXTENDED_HEADER_AUDIOINFO_PREFIX ((guint8)0x83)
#define DMSS_EXTENDED_HEADER_VIDEOINFO_PREFIX ((guint8)0x81)
#define DMSS_MAXIMUM_SAMPLES_AVERAGE 100

#endif /* __GST_DMSS_H__ */
