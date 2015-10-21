#include <gst/gst.h>
#include <gst/net/gstnet.h>
#include <glib.h>


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

  /* increment the timestamp every 1/2 second */
  ctx->timestamp = ctx->timestamp +1;
  GST_BUFFER_PTS (buffer) = ctx->timestamp;
  GST_BUFFER_DTS (buffer) = ctx->timestamp-1;
  g_print("PTS: %lf BASETIME %lf SUM %lf\n", (double) ctx->timestamp, (double) gst_element_get_base_time(appsrc),
            ((double) ctx->timestamp) + ( (double) gst_element_get_base_time(appsrc)));
  GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 20);
  ctx->timestamp += GST_BUFFER_DURATION (buffer);
  

  g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
}

int main(int argc, char *argv[])
{
	GMainLoop* loop;
    GstClock *global_clock;
	GstElement* pipeline;
	gst_init(&argc, &argv);

	//pipeline = gst_parse_launch("appsrc name=mysrc ! videoconvert ! timeoverlay ! autovideosink", NULL);
	pipeline = gst_parse_launch("appsrc name=mysrc ! videoconvert ! timeoverlay ! x264enc tune=\"zerolatency\" threads=1 ! queue ! rtph264pay  ! rtpatimetimestamp ! udpsink host=127.0.0.1 port=5001", NULL);


    global_clock = gst_net_client_clock_new ("net_clock", "127.0.0.1", 8554, 0);
    
    if (global_clock == NULL) {
      g_print ("Failed to create net clock client for %s:%d\n",
          "192.168.0.1", 8554);
      return 0;
    }
    
    g_print ("Waiting clock...");
    /* Wait for the clock to stabilise */
    gst_clock_wait_for_sync (global_clock, GST_CLOCK_TIME_NONE);
    
    g_print ("Synked!\n");     
    
    if (global_clock == NULL) {
        g_print ("No clock!!!!\n");
    } else {
        gst_pipeline_use_clock (GST_PIPELINE (pipeline), global_clock);
    }    

    GstElement * appsrc = gst_bin_get_by_name_recurse_up (GST_BIN (pipeline), "mysrc");

    /* this instructs appsrc that we will be dealing with timed buffer */
    gst_util_set_object_arg (G_OBJECT (appsrc), "format", "time");
    /* configure the caps of the video */
    g_object_set (G_OBJECT (appsrc), "caps",
        gst_caps_new_simple ("video/x-raw",
            "format", G_TYPE_STRING, "RGB16",
            "width", G_TYPE_INT, 384,
            "height", G_TYPE_INT, 288,
            "framerate", GST_TYPE_FRACTION, 0, 1,
            "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
             NULL), NULL);
    
    MyContext *ctx;
    ctx = g_new0 (MyContext, 1);
    ctx->white = FALSE;
    ctx->timestamp = 0;

    
    /* install the callback that will be called when a buffer is needed */
    g_signal_connect (appsrc, "need-data", (GCallback) need_data, ctx);
    gst_object_unref (appsrc);

	loop = g_main_loop_new(NULL, FALSE);

	gst_element_set_state(pipeline, GST_STATE_PLAYING);

	g_main_loop_run(loop);

	gst_element_set_state(pipeline, GST_STATE_NULL);

	gst_object_unref(GST_OBJECT(pipeline));
	g_main_loop_unref(loop);

	return 0;
}
