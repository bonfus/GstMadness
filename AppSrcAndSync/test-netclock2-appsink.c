// gcc test-netclock2-appsink.c -o s2appsink `pkg-config --cflags --libs gstreamer-rtsp-server-1.0 gstreamer-1.0 gstreamer-net-1.0 gstreamer-rtsp-1.0`

/* GStreamer
 * Copyright (C) 2008 Wim Taymans <wim.taymans at gmail.com>
 * Copyright (C) 2014 Jan Schmidt <jan@centricular.com>
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
#include <stdlib.h>     /* atoi */
#include <gst/gst.h>

#include <gst/net/gstnettimeprovider.h>
#include <gst/rtsp-server/rtsp-server.h>

GstClock *global_clock;

#define TEST_TYPE_RTSP_MEDIA_FACTORY      (test_rtsp_media_factory_get_type ())
#define TEST_TYPE_RTSP_MEDIA              (test_rtsp_media_get_type ())

GType test_rtsp_media_factory_get_type (void);
GType test_rtsp_media_get_type (void);

static GstRTSPMediaFactory *test_rtsp_media_factory_new (void);
static GstElement *create_pipeline (GstRTSPMediaFactory * factory,
    GstRTSPMedia * media);

typedef struct TestRTSPMediaFactoryClass TestRTSPMediaFactoryClass;
typedef struct TestRTSPMediaFactory TestRTSPMediaFactory;

struct TestRTSPMediaFactoryClass
{
  GstRTSPMediaFactoryClass parent;
};

struct TestRTSPMediaFactory
{
  GstRTSPMediaFactory parent;
};

typedef struct TestRTSPMediaClass TestRTSPMediaClass;
typedef struct TestRTSPMedia TestRTSPMedia;

struct TestRTSPMediaClass
{
  GstRTSPMediaClass parent;
};

struct TestRTSPMedia
{
  GstRTSPMedia parent;
};

GstRTSPMediaFactory *
test_rtsp_media_factory_new (void)
{
  GstRTSPMediaFactory *result;

  result = g_object_new (TEST_TYPE_RTSP_MEDIA_FACTORY, NULL);

  return result;
}

G_DEFINE_TYPE (TestRTSPMediaFactory, test_rtsp_media_factory,
    GST_TYPE_RTSP_MEDIA_FACTORY);

static gboolean custom_setup_rtpbin (GstRTSPMedia * media, GstElement * rtpbin);

static void
test_rtsp_media_factory_class_init (TestRTSPMediaFactoryClass * test_klass)
{
  GstRTSPMediaFactoryClass *mf_klass =
      (GstRTSPMediaFactoryClass *) (test_klass);
  mf_klass->create_pipeline = create_pipeline;
}

static void
test_rtsp_media_factory_init (TestRTSPMediaFactory * factory)
{
}

static GstElement *
create_pipeline (GstRTSPMediaFactory * factory, GstRTSPMedia * media)
{
  GstElement *pipeline;

  pipeline = gst_pipeline_new ("media-pipeline");
  if (global_clock == NULL) {
    g_print ("No clock!!!!\n");
  } else {
    gst_pipeline_use_clock (GST_PIPELINE (pipeline), global_clock);
  }
  gst_rtsp_media_take_pipeline (media, GST_PIPELINE_CAST (pipeline));

  return pipeline;
}

G_DEFINE_TYPE (TestRTSPMedia, test_rtsp_media, GST_TYPE_RTSP_MEDIA);

static void
test_rtsp_media_class_init (TestRTSPMediaClass * test_klass)
{
  GstRTSPMediaClass *klass = (GstRTSPMediaClass *) (test_klass);
  klass->setup_rtpbin = custom_setup_rtpbin;
}

static void
test_rtsp_media_init (TestRTSPMedia * media)
{
}

static gboolean
custom_setup_rtpbin (GstRTSPMedia * media, GstElement * rtpbin)
{
  g_object_set (rtpbin, "ntp-time-source", 3, NULL);
  return TRUE;
}

void
client_connection (GstRTSPServer *gstrtspserver,
               GstRTSPClient *arg1,
               gpointer       user_data)
{
  GstRTSPConnection * conn = (GstRTSPConnection*) gst_rtsp_client_get_connection (arg1);
  g_print ("Local ip %s\n",
        gst_rtsp_connection_get_ip (conn));

  gint * port = (gint*) user_data;

  g_print ("Port %d\n", *port);  

  if (global_clock == NULL) {
    g_print ("No clocks already available\n");
  } else {
    g_print ("A clock was present, unref it!.\n");
  }

  global_clock = gst_net_client_clock_new ("net_clock", gst_rtsp_connection_get_ip (conn), 8554, 0);

  if (global_clock == NULL) {
    g_print ("Failed to create net clock client for %s:%d\n",
        "192.168.0.1", 8554);
    return;
  }

  g_print ("Waiting clock...");
  /* Wait for the clock to stabilise */
  gst_clock_wait_for_sync (global_clock, GST_CLOCK_TIME_NONE);

  g_print ("Synked!\n");        
}

typedef struct
{
  gboolean white;
  GstClockTime timestamp;
} MyContext;


/* called when we need to give data to appsrc */
static void
need_data (GstElement * appsrc, guint unused, MyContext * ctx)
{
  GstBuffer *buffer;
  guint size;
  GstFlowReturn ret;


  size = 385 * 288 * 2;

  buffer = gst_buffer_new_allocate (NULL, size, NULL);

  /* this makes the image black/white */
  gst_buffer_memset (buffer, 0, ctx->white ? 0xff : 0x0, size);

  ctx->white = !ctx->white;

  /* get timestamp from clock */
  //ctx->timestamp = gst_clock_get_time (global_clock);
  
  g_print("Time is: %"G_GUINT64_FORMAT" \n", ctx->timestamp);
  
  //remove basetime
  //ctx->timestamp -= gst_element_get_base_time (GST_ELEMENT (appsrc));
  
  
  GST_BUFFER_PTS (buffer) = ctx->timestamp;
  g_print("PTS: %"G_GUINT64_FORMAT" BASETIME %"G_GUINT64_FORMAT" SUM %"G_GUINT64_FORMAT " \n", ctx->timestamp, gst_element_get_base_time(appsrc),
            ( ctx->timestamp) + (  gst_element_get_base_time(appsrc)));

  
  
  GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 2);
  
  ctx->timestamp += GST_BUFFER_DURATION (buffer);
  

  g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
}

/* called when a new media pipeline is constructed. We can query the
 * pipeline and configure our appsrc */
static void
media_configure (GstRTSPMediaFactory * factory, GstRTSPMedia * media,
    gpointer user_data)
{
  GstElement *element, *appsrc;
  MyContext *ctx;

  /* get the element used for providing the streams of the media */
  element = gst_rtsp_media_get_element (media);

  //gst_element_set_base_time(GST_ELEMENT(element), 0);

  /* get our appsrc, we named it 'mysrc' with the name property */
  appsrc = gst_bin_get_by_name_recurse_up (GST_BIN (element), "mysrc");

  /* this instructs appsrc that we will be dealing with timed buffer */
  gst_util_set_object_arg (G_OBJECT (appsrc), "format", "time");
  /* configure the caps of the video */
  g_object_set (G_OBJECT (appsrc), "caps",
      gst_caps_new_simple ("video/x-raw",
          "format", G_TYPE_STRING, "RGB16",
          "width", G_TYPE_INT, 384,
          "height", G_TYPE_INT, 288,
          "framerate", GST_TYPE_FRACTION, 0, 1, NULL), NULL);

  ctx = g_new0 (MyContext, 1);
  ctx->white = FALSE;
  ctx->timestamp = 0;
  /* make sure ther datais freed when the media is gone */
  g_object_set_data_full (G_OBJECT (media), "my-extra-data", ctx,
      (GDestroyNotify) g_free);

  /* install the callback that will be called when a buffer is needed */
  g_signal_connect (appsrc, "need-data", (GCallback) need_data, ctx);
  gst_object_unref (appsrc);
  gst_object_unref (element);
}


int
main (int argc, char *argv[])
{
  GMainLoop *loop;
  GstRTSPServer *server;
  GstRTSPMountPoints *mounts;
  GstRTSPMediaFactory *factory;

  gst_init (&argc, &argv);

  if (argc < 1) {
    g_print ("usage: %s [portnum] \n"
        "example: %s 8555 \n"
        "Pipeline is fixed. Default port 8554\n",
        argv[0], argv[0]);
    return -1;
  }

  loop = g_main_loop_new (NULL, FALSE);

  //global_clock = gst_system_clock_obtain ();
  //gst_net_time_provider_new (global_clock, "0.0.0.0", 8554);

  
  /* create a server instance */
  server = gst_rtsp_server_new ();
  
  gint * portnum;
  portnum = malloc(sizeof(gint));
  *portnum = 8554;
  if (argc == 2)
  {
    /* set server listening port*/
    gst_rtsp_server_set_service (server,argv[1]);
    *portnum = atoi(argv[1]);  
  }
  
  
  /* callback for clients */
  g_signal_connect (server, "client-connected", G_CALLBACK (client_connection), portnum);
  

  /* get the mount points for this server, every server has a default object
   * that be used to map uri mount points to media factories */
  mounts = gst_rtsp_server_get_mount_points (server);

  /* make a media factory for a test stream. The default media factory can use
   * gst-launch syntax to create pipelines.
   * any launch line works as long as it contains elements named pay%d. Each
   * element with pay%d names will be a stream */
  factory = test_rtsp_media_factory_new ();
  gst_rtsp_media_factory_set_launch (factory,
      "appsrc name=mysrc ! videoconvert ! x264enc tune=\"zerolatency\" ! rtph264pay pt=96 ! rtpatimetimestamp name=pay0 ntp-offset=0");

  g_signal_connect (factory, "media-configure", (GCallback) media_configure,
      NULL);      
      
  gst_rtsp_media_factory_set_shared (GST_RTSP_MEDIA_FACTORY (factory), TRUE);
  gst_rtsp_media_factory_set_media_gtype (GST_RTSP_MEDIA_FACTORY (factory),
      TEST_TYPE_RTSP_MEDIA);

  /* attach the test factory to the /test url */
  gst_rtsp_mount_points_add_factory (mounts, "/test", factory);

  /* don't need the ref to the mapper anymore */
  g_object_unref (mounts);

  /* attach the server to the default maincontext */
  gst_rtsp_server_attach (server, NULL);

  /* start serving */
  if (argc < 2)
    g_print ("stream ready at rtsp://127.0.0.1:8554/test\n");
  else
  {
    g_print ("stream ready at rtsp://127.0.0.1:");
    g_print (argv[1]);
    g_print ("/test\n");
  }
  g_main_loop_run (loop);

  return 0;
}
