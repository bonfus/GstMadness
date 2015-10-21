#include <gst/gst.h>
#include <glib.h>
#include <gst/gstbuffer.h>
#include <gst/app/gstappsink.h>
#include <gst/rtp/gstrtpbuffer.h>
#include <gst/net/gstnet.h>
#include <gst/net/gstnettimeprovider.h>


void nscb(GstAppSink *appsink, gpointer user_data)
{
    
    static int64_t otimestamp = 0;
    
    GstSample * sample = gst_app_sink_pull_sample (appsink);
    
    GstSegment * segment = gst_sample_get_segment(sample);
    
    GstBuffer * buffer = gst_sample_get_buffer(sample);
    
    if (buffer)
    {
        g_print("APPSINK PTS: %lf\n" , (double) buffer->pts);
        g_print("APPSINK timestamp diff: %lf\n" , (double) (buffer->pts - otimestamp)/1e6);
        //g_print("PTS + basetime : %lf\n" , ((double) buffer->dts) + ((double) GST_ELEMENT_CAST(appsink)->base_time));
        otimestamp = GST_BUFFER_TIMESTAMP(buffer);
        int64_t timestamp = gst_segment_to_stream_time(segment,
                GST_FORMAT_TIME, otimestamp);
        timestamp = GST_TIME_AS_USECONDS(timestamp);
        g_print("APPSINK gst_segment_to_stream_time: %lf\n" , (double) timestamp);
        
    }
}


// The “handle-sync” signal

void jitcb (GstElement *buffer,
               GstStructure       *st,
               gpointer            user_data)
{
    g_print("Got sync\n");
}

int main(int argc, char *argv[])
{
	GMainLoop* loop;

	GstElement* pipeline;
	gst_init(&argc, &argv);

	//pipeline = gst_parse_launch("appsrc name=mysrc ! videoconvert ! timeoverlay ! autovideosink", NULL);
	//pipeline = gst_parse_launch("tcpclientsrc port=8554 host=localhost  ! queue ! tsdemux ! h264parse ! avdec_h264 ! autovideoconvert ! appsink name=mysink", NULL);


    GstClock * global_clock = gst_system_clock_obtain ();
    
    GstClockTime clock_time = gst_clock_get_time (global_clock);
    
    g_print( "time starts from: %" G_GUINT64_FORMAT"\n", clock_time);

    
    gst_net_time_provider_new (global_clock, "0.0.0.0", 8554);
  
    

	//pipeline = gst_parse_launch("gst-launch-1.0 rtspsrc port=8554 location=rtsp://127.0.0.1:8554/test ! application/x-rtp,payload=96 ! rtpjitterbuffer name=buf ! rtph264depay ! appsink name=mysink", NULL);
	pipeline = gst_parse_launch("rtspsrc port=8554 location=rtsp://127.0.0.1:8554/test ! rtpatimeparse ! application/x-rtp, media=(string)video, clock-rate=(int)90000, payload=(int)96 ! rtph264depay ! appsink name=mysink", NULL);
    
    gst_element_set_start_time(pipeline, 1);

    GstElement * appsink = gst_bin_get_by_name_recurse_up (GST_BIN (pipeline), "mysink");
     

	GstAppSinkCallbacks appsinkCallbacks;
	appsinkCallbacks.new_preroll	= NULL;
	appsinkCallbacks.new_sample		= &nscb;
	appsinkCallbacks.eos			= NULL; 
	
	
	gst_app_sink_set_drop			(GST_APP_SINK(appsink), FALSE);
	gst_app_sink_set_max_buffers	(GST_APP_SINK(appsink), 1);
	gst_app_sink_set_callbacks		(GST_APP_SINK(appsink), &appsinkCallbacks, NULL, NULL);		




	loop = g_main_loop_new(NULL, FALSE);

	gst_element_set_state(pipeline, GST_STATE_PLAYING);

	g_main_loop_run(loop);

	gst_element_set_state(pipeline, GST_STATE_NULL);

	gst_object_unref(GST_OBJECT(pipeline));
	g_main_loop_unref(loop);

	return 0;
}
