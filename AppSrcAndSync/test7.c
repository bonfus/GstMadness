//gcc test7.c -o test7 `pkg-config --cflags --libs gstreamer-1.0 gstreamer-net-1.0 gstreamer-app-1.0`
#include <time.h> 
#include <gst/gst.h>
#include <glib.h>
#include <gst/gstbuffer.h>
#include <gst/app/gstappsink.h>

GstClock *global_clock;

typedef struct
{
  gboolean white;
  GstClockTime timestamp;
  GstClockTime begin;
} MyContext;

/* called when we need to give data to appsrc */
static void
need_data (GstElement * appsrc, guint unused, MyContext * ctx)
{
  GstBuffer *buffer;
  guint size;
  GstFlowReturn ret;

  static uint frame_count = 0;

  size = 385 * 288 * 2;
  
  
  
  buffer = gst_buffer_new_allocate (NULL, size, NULL);

  /* this makes the image black/white */
  gst_buffer_memset (buffer, 0, ctx->white ? 0xff : 0x0, size);

  ctx->white = !ctx->white;

  if (frame_count==0)
  {
    ctx->begin = gst_clock_get_time (global_clock);
    g_print("SRC: ctx begin set to: %"G_GUINT64_FORMAT"ms (%" GST_TIME_FORMAT") \n", GST_TIME_AS_MSECONDS(ctx->begin), GST_TIME_ARGS(ctx->begin)); 
  }
  //
  ctx->timestamp = gst_util_uint64_scale_int (1, GST_SECOND, 2) * frame_count;
  
  // wait for camera to produce frame
  struct timespec a;
  a.tv_sec=0;
  a.tv_nsec=100;
  while (ctx->timestamp > (gst_clock_get_time (global_clock)-ctx->begin))
  {
    nanosleep(&a, NULL);
  }


  GstClockTime b = gst_clock_get_time (global_clock);
  g_print("SRC: function begins, time is: %"G_GUINT64_FORMAT"ms (%" GST_TIME_FORMAT") \n", GST_TIME_AS_MSECONDS(b), GST_TIME_ARGS(b));
  /* increment the timestamp every 1/2 second */
  GST_BUFFER_PTS (buffer) = ctx->timestamp;
  GST_BUFFER_DTS (buffer) = ctx->timestamp;
  
  g_print("SRC: PTS: %"G_GUINT64_FORMAT" BASETIME %"G_GUINT64_FORMAT" SUM %"G_GUINT64_FORMAT"\n", GST_BUFFER_PTS (buffer), gst_element_get_base_time(appsrc),
            GST_BUFFER_PTS (buffer) + ( gst_element_get_base_time(appsrc)));
            
  GST_BUFFER_DURATION (buffer) = gst_util_uint64_scale_int (1, GST_SECOND, 2);
    
  b = gst_clock_get_time (global_clock);
  g_print("SRC: function ends, time is: %"G_GUINT64_FORMAT"ms (%" GST_TIME_FORMAT") \n\n", GST_TIME_AS_MSECONDS(b), GST_TIME_ARGS(b));
    
  g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
  frame_count++;
}



void nscb(GstAppSink *appsink, gpointer user_data)
{
    
    MyContext * ctx = (MyContext *) user_data;
    
    GstSample * sample = gst_app_sink_pull_sample (appsink);
    
    GstSegment * segment = gst_sample_get_segment(sample);
    
    GstBuffer * buffer = gst_sample_get_buffer(sample);
    
    if (buffer)
    {
        g_print("SINK: PTS: %"G_GUINT64_FORMAT"\n" , buffer->pts);
        g_print("SINK: PTS + begin : %"G_GUINT64_FORMAT" (%" GST_TIME_FORMAT")\n" , 
            buffer->pts + ctx->begin, GST_TIME_ARGS(buffer->pts + ctx->begin));
        
        GstClockTime timestamp = GST_BUFFER_TIMESTAMP(buffer);
        timestamp = gst_segment_to_running_time(segment,
                GST_FORMAT_TIME, timestamp);
                
        
        g_print("SINK: gst_segment_to_stream_time: %"G_GUINT64_FORMAT"\n" , GST_TIME_AS_MSECONDS(timestamp));
        g_print("SINK: gst_segment_to_stream_time + base_time: %"G_GUINT64_FORMAT" (%" GST_TIME_FORMAT")\n\n" , 
            timestamp +GST_ELEMENT_CAST(appsink)->base_time,
            GST_TIME_ARGS(timestamp + gst_element_get_base_time (GST_ELEMENT(appsink)) ));
    }
}

int main(int argc, char *argv[])
{
	GMainLoop* loop;

	GstElement* pipeline;
	gst_init(&argc, &argv);
    
    //setup clock
    global_clock = gst_system_clock_obtain ();
    GstClockTime clock_time = gst_clock_get_time (global_clock);
    g_print( "Application starts at: %" G_GUINT64_FORMAT"\n", clock_time);    

	pipeline = gst_parse_launch("appsrc name=mysrc ! autovideoconvert ! x264enc tune=\"zerolatency\" ! appsink name=mysink", NULL);


    GstElement * appsink = gst_bin_get_by_name_recurse_up (GST_BIN (pipeline), "mysink");
    GstElement * appsrc = gst_bin_get_by_name_recurse_up (GST_BIN (pipeline), "mysrc");

    // setup appsrc
    
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
    ctx->begin = 0;

    
    /* install the callback that will be called when a buffer is needed */
    g_signal_connect (appsrc, "need-data", (GCallback) need_data, ctx);
    gst_object_unref (appsrc);
    gst_object_unref (appsink);


    // setup appsink
    
	GstAppSinkCallbacks appsinkCallbacks;
	appsinkCallbacks.new_preroll	= NULL;
	appsinkCallbacks.new_sample		= &nscb;
	appsinkCallbacks.eos			= NULL; 
	
	
	gst_app_sink_set_drop			(GST_APP_SINK(appsink), FALSE);
	gst_app_sink_set_max_buffers	(GST_APP_SINK(appsink), 1);
	gst_app_sink_set_callbacks		(GST_APP_SINK(appsink), &appsinkCallbacks, ctx, NULL);		



    


	loop = g_main_loop_new(NULL, FALSE);

	gst_element_set_state(pipeline, GST_STATE_PLAYING);

	g_main_loop_run(loop);

	gst_element_set_state(pipeline, GST_STATE_NULL);

	gst_object_unref(GST_OBJECT(pipeline));
	g_main_loop_unref(loop);

	return 0;
}
