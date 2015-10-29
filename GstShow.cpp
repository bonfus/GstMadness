#include "log.h"
#include "GstShow.hpp"
#include "GstCommon.hpp"
#include <gst/net/gstnet.h>

#define PLAYBACK_DELAY_MS 50
#define PIPELINE_LATENCY_MS 500


/*!
 * \brief GstOutput::init
 * initialise all variables
 */
void GstShow::init()
{
    pipeline = NULL;
    source[0] = NULL;
    source[1] = NULL;
    scale[0] = NULL;
    scale[1] = NULL;
    scalefilter[0] = NULL;
    scalefilter[1] = NULL;
    queue[0] = NULL;
    queue[1] = NULL;
    
    mixer = NULL;
    videosink = NULL;
    
    buffer = NULL;

    num_frames = 0;
    framerate = 0;
}

/*!
 * \brief GstOutput::close
 * ends the pipeline by sending EOS and destroys the pipeline and all
 * elements afterwards
 */
void GstShow::close()
{
    GstStateChangeReturn status;
    if (pipeline)
    {
        handleMessage(pipeline);
        //
        //for (int i=0; i<2;i++)
        //{
        //    if (gst_app_src_end_of_stream(GST_APP_SRC(source[i])) != GST_FLOW_OK)
        //    {
        //        L_(lwarning) << ("Cannot send EOS to GStreamer pipeline\n");
        //        return;
        //    }
        //}
        //
        ////wait for EOS to trickle down the pipeline. This will let all elements finish properly
        //GstBus* bus = gst_element_get_bus(pipeline);
        //GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
        //if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
        //{
        //    L_(lwarning) << ("Error during GstOut finalization\n");
        //    return;
        //}
        //
        //if(msg != NULL)
        //{
        //    gst_message_unref(msg);
        //    g_object_unref(G_OBJECT(bus));
        //}
        
        status = gst_element_set_state (pipeline, GST_STATE_NULL);
        if (status == GST_STATE_CHANGE_ASYNC)
        {
            // wait for status update
            GstState st1;
            GstState st2;
            status = gst_element_get_state(pipeline, &st1, &st2, GST_CLOCK_TIME_NONE);
        }
        if (status == GST_STATE_CHANGE_FAILURE)
        {
            handleMessage (pipeline);
            gst_object_unref (GST_OBJECT (pipeline));
            pipeline = NULL;
            L_(lwarning) << ("Unable to stop gstreamer pipeline\n");
            return;
        }
        
        status = gst_element_set_state (pipeline, GST_STATE_NULL);
        if (status == GST_STATE_CHANGE_ASYNC)
        {
            // wait for status update
            GstState st1;
            GstState st2;
            status = gst_element_get_state(pipeline, &st1, &st2, GST_CLOCK_TIME_NONE);
        }
        if (status == GST_STATE_CHANGE_FAILURE)
        {
            handleMessage (pipeline);
            gst_object_unref (GST_OBJECT (pipeline));
            pipeline = NULL;
            L_(lwarning) << ("Unable to stop gstreamer pipeline\n");
            return;
        }        

        gst_object_unref (GST_OBJECT (pipeline));
        pipeline = NULL;
    }
}



void
GstShow::source_created (GstElement * pipe, GstElement * source)
{
  g_object_set (source, "latency", PLAYBACK_DELAY_MS,
      "ntp-time-source", 3, "buffer-mode", 4, "ntp-sync", TRUE, NULL);
  g_print("Done setting source!\n")    ;
}

void GstShow::newPad(GstElement *src,
                       GstPad     *new_pad,
                       gpointer    data)
{
    GstPad *sink_pad;
    GstElement *color = (GstElement *) data;
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;
    
    g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));
    
    
    
    new_pad_caps = gst_pad_get_current_caps (new_pad);
    new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
    new_pad_type = gst_structure_get_name (new_pad_struct);
    
    g_print ("  It has type '%s'.\n", new_pad_type);
    
    sink_pad = gst_element_get_static_pad (color, "sink");
    if (!sink_pad){
        L_(lerror) << "Gstreamer: no pad named sink";
        return;
    }


    /* If our converter is already linked, we have nothing to do here */
    if (gst_pad_is_linked (sink_pad)) {
      g_print ("  We are already linked. Ignoring.\n");
      /* Unreference the new pad's caps, if we got them */
      if (new_pad_caps != NULL)
          gst_caps_unref (new_pad_caps);
      
      /* Unreference the sink pad */
      gst_object_unref (sink_pad);
      return;
    }
    
    /* Attempt the link */
    ret = gst_pad_link (new_pad, sink_pad);
    if (GST_PAD_LINK_FAILED (ret)) {
      g_print ("  Type is '%s' but link failed.\n", new_pad_type);
      /* Unreference the new pad's caps, if we got them */
      if (new_pad_caps != NULL)
          gst_caps_unref (new_pad_caps);
      
      /* Unreference the sink pad */
      gst_object_unref (sink_pad);
      return;    
    } else {
      g_print ("  Link succeeded (type '%s').\n", new_pad_type);
    }

}

/*!
 * \brief GstOutput::openAppSrc
 * \param filename filename to output to
 * \param fourcc desired codec fourcc
 * \param fps desired framerate
 * \param frameSize the size of the expected frames
 * \param is_color color or grayscale
 * \return success
 *
 *
 */
bool GstShow::open(int xwinid, std::string Laddress, std::string Raddress)
{
    
    // init gstreamer
    gst_initializer::init();

    // init vars
    //int  bufsize = 0;

    bool file = false;
    char *uriL = NULL;
    char *uriR = NULL;
    
    init_pipeline(xwinid);
    
    if(!gst_uri_is_valid(Laddress.c_str()) || !gst_uri_is_valid(Raddress.c_str()))
    {
        uriL = realpath(Laddress.c_str(), NULL);
        uriR = realpath(Raddress.c_str(), NULL);
        if(uriL != NULL && uriR != NULL)
        {
            uriL = g_filename_to_uri(uriL, NULL, NULL);
            uriR = g_filename_to_uri(uriR, NULL, NULL);
            if(uriL != NULL && uriR != NULL)
            {
                file = true;
            }
            else
            {
                L_(lerror) << "GStreamer: Error opening file";
                close();
                return false;
            }
        }
    } else {
        file = false;
        uriL = g_strdup(Laddress.c_str());        
        uriR = g_strdup(Raddress.c_str());        
    }
    if (!file)
    {
        global_clock = gst_system_clock_obtain ();
        gst_net_time_provider_new (global_clock, "0.0.0.0", 8554);
        if (global_clock != NULL)
        L_(ldebug2) << ("Clock created!");
        else
        {
        L_(lerror) << ("Could not creaye clock!");
        return false;
        }
        gst_pipeline_use_clock (GST_PIPELINE (pipeline), global_clock);
    }
        
    
    for (int i = 0; i<2; i++)
    {
        source[i] = gst_element_factory_make("uridecodebin", NULL);

        gst_bin_add_many(GST_BIN(pipeline), source[i], NULL);
        
        if (i==0)
            g_object_set (source[i], "uri", uriL, NULL);
        if (i==1)
            g_object_set (source[i], "uri", uriR, NULL);
            
        //sync if not file!
        if (!file)
        {
            g_signal_connect (source[i], "source-setup", G_CALLBACK (source_created), NULL);
        }
        
        //Set latency if we have a stream
        if (!file)
        {
            /* Set this high enough so that it's higher than the minimum latency
            * on all receivers */
            gst_pipeline_set_latency (GST_PIPELINE (pipeline), PIPELINE_LATENCY_MS * GST_MSECOND);
        }        
        
        g_signal_connect(source[i], "pad-added", G_CALLBACK(newPad), queue[i]);
        
    }

    
    return finish_pipeline();


}


bool GstShow::init_pipeline(const int  xwinid)
{
    pipeline = gst_pipeline_new ("xvoverlay");

    
    //create base pipeline elements
    videosink = gst_element_factory_make("xvimagesink", NULL);

    gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (videosink), xwinid);    
    
    mixer = gst_element_factory_make("videomixer", "mix");
    
    
    ///* Manually linking the videoboxes to the mixer */
    GstPadTemplate *mixer_sink_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(mixer), "sink_%u");
    
    if(mixer_sink_pad_template == NULL) {
      g_printerr("Could not get mixer pad template.\n");
      // gst_object_unref(something);
      return false;
    }    
    
    GstPad* mixerpads[2];
    
    mixerpads[0] = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);
    mixerpads[1] = gst_element_request_pad(mixer, mixer_sink_pad_template, NULL, NULL);
    
    
    g_object_set(mixerpads[0], "xpos",   0, NULL);
    g_object_set(mixerpads[0], "ypos",   0, NULL);
    g_object_set(mixerpads[0], "alpha",1.0, NULL);
    
    g_object_set(mixerpads[1], "xpos", 640, NULL);
    g_object_set(mixerpads[1], "ypos",   0, NULL);
    g_object_set(mixerpads[1], "alpha",1.0, NULL);    
    
    
    gst_object_unref(mixerpads[0]);
    gst_object_unref(mixerpads[1]);
    
    // prepare queue and scale
    
    for (int i = 0; i<2; i++)
    {
        queue[i] = gst_element_factory_make("queue", NULL);
        scale[i] = gst_element_factory_make("videoscale", NULL);
        scalefilter[i] = gst_element_factory_make("capsfilter", NULL);
    
        GstCaps *caps =  gst_caps_new_simple("video/x-raw",
            "width", G_TYPE_INT, 640,
            "height", G_TYPE_INT, 480, 
            //"format", G_TYPE_STRING, "BGR",
            NULL);
        caps = gst_caps_fixate(caps);
        
        g_object_set(G_OBJECT(scalefilter[i]), "caps", caps, NULL);
        gst_caps_unref(caps);
    }
    
    
    
    gst_bin_add_many(GST_BIN(pipeline), queue[0], queue[1], scale[0], scale[1], 
                    scalefilter[0], scalefilter[1], mixer, videosink, NULL);
    
    
    
    return true;
}

bool GstShow::finish_pipeline()
{

    GError *err = NULL;
    GstStateChangeReturn stateret;

    for (int i = 0; i<2; i++)
    {
        if(!gst_element_link_many(queue[i], scale[i], scalefilter[i], mixer, NULL)) {
                L_(lerror) << ("GStreamer: cannot link elements in finish\n");
                return false;
        }
    }
    
    if(!gst_element_link(mixer , videosink)) {
            L_(lerror) << ("GStreamer: cannot link mixer to sink\n");
            return false;
    }
    
    
    // prepare the pipeline
    
    stateret = gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
    if(stateret  == GST_STATE_CHANGE_FAILURE) {
        handleMessage(pipeline);
        L_(lerror) << ( "GStreamer: cannot put pipeline to play\n");
    }

    handleMessage(pipeline);

    GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "pipeline");

    return true;    
}
