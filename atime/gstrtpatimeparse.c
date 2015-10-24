/*
 * gstrtpatimetimestamp-parse.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gst/rtp/gstrtpbuffer.h>

#include "gstrtpatimeparse.h"
#include "gstrtpatimemeta.h"

GST_DEBUG_CATEGORY_STATIC (rtpatimeparse_debug);
#define GST_CAT_DEFAULT (rtpatimeparse_debug)

static GstFlowReturn gst_rtp_atime_parse_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);

static GstStaticPadTemplate sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

static GstStaticPadTemplate src_template_factory =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp")
    );

G_DEFINE_TYPE (GstRtpatimeParse, gst_rtp_atime_parse, GST_TYPE_ELEMENT);

static void
gst_rtp_atime_parse_class_init (GstRtpatimeParseClass * klass)
{
  GstElementClass *gstelement_class;

  gstelement_class = GST_ELEMENT_CLASS (klass);

  /* register pads */
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template_factory));

  gst_element_class_set_static_metadata (gstelement_class,
      "atime NTP timestamps RTP extension", "Effect/RTP",
      "Add absolute timestamps and flags of recorded data in a playback "
      "session", "Guillaume Desmottes <guillaume.desmottes@collabora.com>");
      
      
  GST_DEBUG_CATEGORY_INIT (rtpatimeparse_debug, "rtpatimeparse",
      0, "atime NTP timestamps RTP extension");      
}

static void
gst_rtp_atime_parse_init (GstRtpatimeParse * self)
{
  self->sinkpad =
      gst_pad_new_from_static_template (&sink_template_factory, "sink");
  gst_pad_set_chain_function (self->sinkpad, gst_rtp_atime_parse_chain);
  gst_element_add_pad (GST_ELEMENT (self), self->sinkpad);
  GST_PAD_SET_PROXY_CAPS (self->sinkpad);

  self->srcpad =
      gst_pad_new_from_static_template (&src_template_factory, "src");
  gst_element_add_pad (GST_ELEMENT (self), self->srcpad);
}

#define EXTENSION_ID 0xABAC
#define EXTENSION_SIZE 3

static gboolean
handle_buffer (GstRtpatimeParse * self, GstBuffer * buf)
{
  GstRTPBuffer rtp = GST_RTP_BUFFER_INIT;
  guint8 *data;
  guint16 bits;
  guint wordlen;
  guint8 flags;

  static GstClockTime timestamp;
  ATimeMeta *atime_meta=NULL;
  guint8 cseq;


  if (!gst_rtp_buffer_map (buf, GST_MAP_READ, &rtp)) {
    GST_ELEMENT_ERROR (self, STREAM, FAILED,
        ("Failed to map RTP buffer"), (NULL));
    return FALSE;
  }

  /* Check if the atime RTP extension is present in the packet */
  if (!gst_rtp_buffer_get_extension_data (&rtp, &bits, (gpointer) & data,
          &wordlen))
    goto out;

  if (bits != EXTENSION_ID || wordlen != EXTENSION_SIZE)
    goto out;

  timestamp = GST_READ_UINT64_BE (data);  /* TODO */
  flags = GST_READ_UINT8 (data + 8);
  cseq = GST_READ_UINT8 (data + 9);  /* TODO */


  /* convert timestamp */
  
  GST_DEBUG_OBJECT (self, "timestamp: %" G_GUINT64_FORMAT, timestamp);
  GST_DEBUG_OBJECT (self, "PTS: %" G_GUINT64_FORMAT, buf->pts);
  
  /* convert back to NTP time. upper 32 bits should contain the seconds
   * and the lower 32 bits, the fractions of a second. 
   * Thus timestamp = time * denom/num
   * */
  timestamp = gst_util_uint64_scale (timestamp, GST_SECOND,
        (G_GINT64_CONSTANT (1) << 32));

  GST_DEBUG_OBJECT (self, "converted timestamp: %" G_GUINT64_FORMAT, timestamp);  

  atime_meta = gst_buffer_add_atime_meta(buf, timestamp);

  GST_DEBUG_OBJECT (self, "Added meta");  

  /* C */
  if (flags & (1 << 7))
    GST_BUFFER_FLAG_UNSET (buf, GST_BUFFER_FLAG_DELTA_UNIT);
  else
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DELTA_UNIT);

  /* E */
  /* if (flags & (1 << 6));  TODO */

  /* D */
  if (flags & (1 << 5))
    GST_BUFFER_FLAG_SET (buf, GST_BUFFER_FLAG_DISCONT);
  else
    GST_BUFFER_FLAG_UNSET (buf, GST_BUFFER_FLAG_DISCONT);

out:
  GST_DEBUG_OBJECT (self, "Gone to out!");
  GST_DEBUG_OBJECT (self, "PTS: %" G_GUINT64_FORMAT, buf->pts);
  atime_meta = gst_buffer_add_atime_meta(buf, timestamp);
  gst_rtp_buffer_unmap (&rtp);
  return TRUE;
}

static GstFlowReturn
gst_rtp_atime_parse_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstRtpatimeParse *self = GST_RTP_atime_PARSE (parent);

  if (!handle_buffer (self, buf)) {
    gst_buffer_unref (buf);
    return GST_FLOW_ERROR;
  }

  return gst_pad_push (self->srcpad, buf);
}
