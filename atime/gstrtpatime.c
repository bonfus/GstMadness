/*
 * gstrtpatime.c
 *
 * Copyright (C) 2014 Axis Communications AB
 *  Author: Guillaume Desmottes <guillaume.desmottes@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include "gstrtpatimetimestamp.h"
#include "gstrtpatimeparse.h"

static gboolean
plugin_init (GstPlugin * plugin)
{
  if (!gst_element_register (plugin, "rtpatimetimestamp", GST_RANK_NONE,
          GST_TYPE_RTP_atime_TIMESTAMP))
    return FALSE;
  if (!gst_element_register (plugin, "rtpatimeparse", GST_RANK_NONE,
          GST_TYPE_RTP_atime_PARSE))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    rtpatime,
    "atime Streaming features",
    plugin_init, VERSION, GST_LICENSE_UNKNOWN, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
