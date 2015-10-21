#include <gst/gst.h>
#include <glib.h>
#include <gst/gstbuffer.h>
#include <gst/app/gstappsink.h>

void nscb(GstAppSink *appsink, gpointer user_data)
{
    GstSample * sample = gst_app_sink_pull_sample (appsink);
    
    GstSegment * segment = gst_sample_get_segment(sample);
    
    GstBuffer * buffer = gst_sample_get_buffer(sample);
    
    if (buffer)
    {
        g_print("PTS: %lf\n" , (double) buffer->pts);
        g_print("PTS + basetime : %lf\n" , ((double) buffer->dts) + ((double) GST_ELEMENT_CAST(appsink)->base_time));
        int64_t timestamp = GST_BUFFER_TIMESTAMP(buffer);
        timestamp = gst_segment_to_stream_time(segment,
                GST_FORMAT_TIME, timestamp);
        timestamp = GST_TIME_AS_USECONDS(timestamp);
        g_print("gst_segment_to_stream_time: %lf\n" , (double) timestamp);
    }
}

int main(int argc, char *argv[])
{
	GMainLoop* loop;

	GstElement* pipeline;
	gst_init(&argc, &argv);

	//pipeline = gst_parse_launch("appsrc name=mysrc ! videoconvert ! timeoverlay ! autovideosink", NULL);
	//pipeline = gst_parse_launch("tcpclientsrc port=8554 host=localhost  ! queue ! tsdemux ! h264parse ! avdec_h264 ! autovideoconvert ! appsink name=mysink", NULL);
	pipeline = gst_parse_launch("tcpclientsrc port=8554 host=localhost  ! queue ! tsdemux ! appsink name=mysink", NULL);


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
