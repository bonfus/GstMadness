#include <gst/pbutils/missing-plugins.h>
#include "GstCommon.hpp"
#include "log.h"


void gst_initializer::init()
{
    gst_initializer_mutex.lock();
    static gst_initializer init;
    gst_initializer_mutex.unlock();
}

gst_initializer::gst_initializer()
{
	GError* e;
	gboolean res;

	res = gst_init_check(NULL, NULL, &e);
	if (!res) {
		L_(lerror) << "GstINIT: " << e->message;
	}
//       gst_debug_set_active(1);
//       gst_debug_set_colored(1);
//       gst_debug_set_default_threshold(GST_LEVEL_INFO);
}



/*!
 * \brief handleMessage
 * Handles gstreamer bus messages. Mainly for debugging purposes and ensuring clean shutdown on error
 */
void handleMessage(GstElement * pipeline)
{

    GError *err = NULL;
    gchar *debug = NULL;
    GstBus* bus = NULL;
    GstStreamStatusType tp;
    GstElement * elem = NULL;
    GstMessage* msg  = NULL;

    bus = gst_element_get_bus(pipeline);

    while(gst_bus_have_pending(bus)) {
        msg = gst_bus_pop(bus);

        //printf("Got %s message\n", GST_MESSAGE_TYPE_NAME(msg));

        if(gst_is_missing_plugin_message(msg))
        {
            L_(lerror) << "GStreamer: your gstreamer installation is missing a required plugin\n";
        }
        else
        {
            switch (GST_MESSAGE_TYPE (msg)) {
            case GST_MESSAGE_STATE_CHANGED:
                GstState oldstate, newstate, pendstate;
                gst_message_parse_state_changed(msg, &oldstate, &newstate, &pendstate);
                //fprintf(stderr, "state changed from %s to %s (pending: %s)\n", gst_element_state_get_name(oldstate),
                //                gst_element_state_get_name(newstate), gst_element_state_get_name(pendstate));
                break;
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &err, &debug);
                L_(lerror) << "GStreamer Plugin: Embedded video playback halted; module " << 
                                gst_element_get_name(GST_MESSAGE_SRC (msg)) << " reported: "<< err->message;

                g_error_free(err);
                g_free(debug);

                gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
                break;
            case GST_MESSAGE_EOS:
                //fprintf(stderr, "reached the end of the stream.");
                break;
            case GST_MESSAGE_STREAM_STATUS:
                gst_message_parse_stream_status(msg,&tp,&elem);
                //fprintf(stderr, "stream status: elem %s, %i\n", GST_ELEMENT_NAME(elem), tp);
                break;
            default:
                //fprintf(stderr, "unhandled message %s\n",GST_MESSAGE_TYPE_NAME(msg));
                break;
            }
        }
        gst_message_unref(msg);
    }

    gst_object_unref(GST_OBJECT(bus));
}
