#include "log.h"
#include "GstOutput.hpp"
#include "GstCommon.hpp"



/*!
 * \brief GstOutput::init
 * initialise all variables
 */
void GstOutput::init()
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
    
    for (int i =0; i< 2; i++)
    {
        data[i].buffer = NULL;
        data[i].buffer = NULL;
        data[i].haveData=0;
        data[i].fps=0;
        data[i].iFrame = 0;
        data[i].stop = false;
        data[i].bufferSize=0;
    }


    num_frames = 0;
    framerate = 0;
}

/*!
 * \brief GstOutput::close
 * ends the pipeline by sending EOS and destroys the pipeline and all
 * elements afterwards
 */
void GstOutput::close()
{
    

    
    GstStateChangeReturn status;
    if (pipeline)
    {
        handleMessage(pipeline);

        for (int i=0; i<2;i++)
        {
            g_mutex_lock(&(data[i].mutex));
            data[i].haveData = 0;
            data[i].stop = true;
            g_mutex_unlock(&(data[i].mutex));
            g_cond_signal(&data[i].cond);
        }
            
        for (int i=0; i<2;i++)
        {
            if (gst_app_src_end_of_stream(GST_APP_SRC(source[i])) != GST_FLOW_OK)
            {
                L_(lwarning) << ("Cannot send EOS to GStreamer pipeline\n");
                return;
            }
        }
        
        //wait for EOS to trickle down the pipeline. This will let all elements finish properly
        GstBus* bus = gst_element_get_bus(pipeline);
        GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
        {
            L_(lwarning) << ("Error during GstOut finalization\n");
            return;
        }
        
        if(msg != NULL)
        {
            gst_message_unref(msg);
            g_object_unref(G_OBJECT(bus));
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
    
    L_(ldebug2) << ("GstOut cleaned");
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
bool GstOutput::openAppSrc( const int  xwinid, double fps, CvSize frameSize, bool is_color)
{
    
    // check arguments
    assert (frameSize.width > 0  &&  frameSize.height > 0);

    // init gstreamer
    gst_initializer::init();
    for (int i = 0; i < 2; i++)
    {
        g_mutex_init(&data[i].mutex);
    }

    init_pipeline(xwinid);
    
    
    for (int i = 0; i<2; i++)
    {
        source[i] = gst_element_factory_make("appsrc", (i==0) ? "src0" : "src1");
        data[i].fps = int(fps);

        GstCaps* caps = NULL;
        
        if (is_color)
        {
            input_pix_fmt = GST_VIDEO_FORMAT_BGR;
            //bufsize = frameSize.width * frameSize.height * 3;
        
        
            caps = gst_caps_new_simple("video/x-raw",
                                    "format", G_TYPE_STRING, "BGR",
                                    "width", G_TYPE_INT, frameSize.width,
                                    "height", G_TYPE_INT, frameSize.height,
                                    "framerate", GST_TYPE_FRACTION, data[i].fps, 1,
                                    "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
                                    NULL);
            caps = gst_caps_fixate(caps);
        
        
        }
        else
        {
            input_pix_fmt = GST_VIDEO_FORMAT_GRAY8;
            //bufsize = frameSize.width * frameSize.height;
        
        
            caps = gst_caps_new_simple("video/x-raw",
                                    "format", G_TYPE_STRING, "GRAY8",
                                    "width", G_TYPE_INT, frameSize.width,
                                    "height", G_TYPE_INT, frameSize.height,
                                    "framerate", GST_TYPE_FRACTION, data[i].fps, 1,
                                    "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
                                    NULL);
            caps = gst_caps_fixate(caps);
        
        }
        
        gst_app_src_set_caps(GST_APP_SRC(source[i]), caps);
        gst_app_src_set_stream_type(GST_APP_SRC(source[i]), GST_APP_STREAM_TYPE_STREAM);
        //g_signal_connect(GST_APP_SRC(source[i]), "need-data", G_CALLBACK(cb_need_data), &data);
        GstAppSrcCallbacks callbacks;
        memset(&callbacks, 0, sizeof(callbacks));
        callbacks.need_data = cb_need_data;
        //callbacks.seek_data = (gboolean (*)(GstAppSrc*,guint64,void*))seekDataWrap;
        gst_app_src_set_callbacks(GST_APP_SRC(source[i]), &callbacks, &data[i], 0);
        
        
        //gst_app_src_set_size (GST_APP_SRC(source[i]), -1);
        //
        //
        g_object_set(G_OBJECT(source[i]), "format", GST_FORMAT_TIME, NULL);
        //if (isLive == false)
        //{
        //    g_object_set(G_OBJECT(source[i]), "block", 1, NULL);
        //    g_object_set(G_OBJECT(source[i]), "is-live", 0, NULL);
        //} else {
        //    g_object_set(G_OBJECT(source[i]), "block", 0, NULL);
        //    g_object_set(G_OBJECT(source[i]), "is-live", 1, NULL);            
        //}

        //conv[i] = gst_element_factory_make ("videoconvert", NULL);
        gst_bin_add_many(GST_BIN(pipeline), source[i], NULL);
        //if(!gst_element_link_filtered(source[i], queue[i], caps)) {
        if(!gst_element_link(source[i], queue[i])) {
            L_(lerror) << ("GStreamer: cannot link elements\n");
        }        
    }



    

    framerate = fps;
    num_frames = 0;
    
    return finish_pipeline();


}


bool GstOutput::init_pipeline(const int  xwinid)
{
    pipeline = gst_pipeline_new ("xvoverlay");

    
    //create base pipeline elements
    videosink = gst_element_factory_make("xvimagesink", NULL);

    gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (videosink), xwinid);    
    
    mixer = gst_element_factory_make("videomixer", "mix");
    
    
    ///* Manually linking the videoboxes to the mixer */
    GstPadTemplate *mixer_sink_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(mixer), "sink_%u");
    
    if(mixer_sink_pad_template == NULL) {
      L_(lerror) << "Could not get mixer pad template.";
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

bool GstOutput::finish_pipeline()
{

    GError *err = NULL;
    GstStateChangeReturn stateret;

    //GstCaps* videocaps = NULL;

    //GstCaps* containercaps = NULL;

    for (int i = 0; i<2; i++)
    {
        if(!gst_element_link_many(queue[i], scale[i], scalefilter[i], mixer, NULL)) {
                L_(lerror) << ("GStreamer: cannot link elements in finish\n");
                return false;
        }
    }
    //
    //
    //
    
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



void
GstOutput::cb_need_data(GstAppSrc *appsrc,
guint       unused_size,
gpointer    user_data)
{
	static guint white = FALSE, i = 0;
	static GstClockTime timestamp = 0;
	GstBuffer *buffer = NULL;
	GstFlowReturn ret;
	gboolean resize;

    inputData_t * data = (inputData_t *) user_data;
    
    //TODO ! ! ! check ifbase time is not 0

    //g_print("Dump of data: \n");
    //g_print("fps %d\n",data->fps);
    //g_print("haveData %d\n",data->haveData);
    //g_print("stop %d\n",data->stop);

    g_mutex_lock(&(data->mutex));
    if (data->haveData == false) {
        //g_print("Wait for data...\n");
        g_cond_wait(&(data->cond), &(data->mutex));
        //g_print("Done.\n");
    } else {
        //g_print("Callback: have data %d\n",data->haveData);
    }

    buffer = gst_buffer_copy(data->buffer);
    data->haveData = false;
    (data->iFrame)++;
    g_mutex_unlock(&(data->mutex));

	if (data->stop) {
		gst_app_src_end_of_stream(appsrc);
		return;
	}
	
	if (!buffer)
		return;

    //GST_BUFFER_PTS(buffer) = timestamp;
	//GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, data->fps);
    //if (iSource == 0 )
    //{
    //    timestamp += GST_BUFFER_DURATION(buffer);
    //    (data->iFrame)++;
    //}
	
	GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, data->fps);
	GST_BUFFER_PTS(buffer) = GST_BUFFER_DURATION(buffer)*data->iFrame;
    GST_BUFFER_OFFSET(buffer) = data->iFrame;
    
    //timestamp += GST_BUFFER_DURATION(buffer);
    //g_print("iSource PTS: %lf\n", (double) GST_BUFFER_PTS(buffer));
	ret = gst_app_src_push_buffer((GstAppSrc*)appsrc, buffer);
	if (ret != GST_FLOW_OK) {
        L_(lwarning) << ("Error pushing buffer to GStreamer pipeline");
	}
    
}



////////////////////////// aAAAAAAAAAAAAaaa
//void streamer_feed_sync(guint w, guint h, guint8* frame) {
//	gssize size;
//	if (w != app.in_width || h != app.in_height) {
//		app.resize = TRUE;
//		app.in_width = w;
//		app.in_height = h;
//	}
//	size = app.in_width * app.in_height * 3;
//
//	if (app.resize) {
//		/* This doesn't work if size is more than buffer's size */
//		//gst_buffer_resize(app.buffer, 0, size);
//		if (app.buffer)
//			gst_buffer_unref(app.buffer);
//		app.buffer = gst_buffer_new_and_alloc(size);
//	}
//
//	gst_buffer_fill(app.buffer, 0, frame, size);
//}
//
//void streamer_feed(guint w, guint h, guint8* frame) {
//	g_mutex_lock(&app.mutex);
//
//	streamer_feed_sync(w, h, frame);
//	app.have_data = TRUE;
//
//	g_mutex_unlock(&app.mutex);
//	g_cond_signal(&app.cond);
//}
/////////////////////////////// AAAAAAAaaaaaaaaaa



/*!
 * \brief GstOutput::displayFrame
 * \param imageL 
 * \param imageR
 * \return
 * Pushes the given frames on the pipeline.
 * The timestamp for the buffer is generated from the framerate set in open
 * and ensures a smooth video. This should probably be changed.
 */
bool GstOutput::feedFrame( const IplImage * imageL, const IplImage * imageR )
{


    int size;

    handleMessage(pipeline);


    //return true;
    if (input_pix_fmt == GST_VIDEO_FORMAT_BGR) {
        if (imageL->nChannels != 3 || imageL->depth != IPL_DEPTH_8U || 
            imageR->nChannels != 3 || imageR->depth != IPL_DEPTH_8U) {
            L_(lerror) << ( "GstOutput::diplayFrame() needs images with depth = IPL_DEPTH_8U and nChannels = 3.");
        }
    }
    else if (input_pix_fmt == GST_VIDEO_FORMAT_GRAY8) {
        if (imageL->nChannels != 1 || imageL->depth != IPL_DEPTH_8U ||
            imageR->nChannels != 1 || imageR->depth != IPL_DEPTH_8U) {
            L_(lerror) << ("GstOutput::diplayFrame() needs images with depth = IPL_DEPTH_8U and nChannels = 1.");
        }
    }


    size = imageL->imageSize; // TODO! CHECK SIZE!!!
    
    //g_print("Acq mutex...\n");
 
    //g_print("done\n");
    //g_print("Have data: %d\n", data.haveData);
    
    
    
    

    for (int i=0;i<2;i++)
    {
        //gst_app_src_push_buffer takes ownership of the buffer, so we need to supply it a copy

        if (data[i].bufferSize != size)
        {
            
            data[i].buffer = gst_buffer_new_allocate (NULL, size, NULL);
            data[i].bufferSize = size;
        }    

        if (data[i].haveData == true)
        {
            continue;
        }

        g_mutex_lock(&data[i].mutex);       
        GstMapInfo info;
        gst_buffer_map(data[i].buffer, &info, (GstMapFlags)GST_MAP_READ);
        if (i==0)
        {
            //memcpy(info.data, (guint8*)imageL->imageData, size);
            gst_buffer_fill(data[i].buffer, 0, (guint8*)imageL->imageData, size);
        }
        if (i==1)
        {
            //memcpy(info.data, (guint8*)imageR->imageData, size);
            gst_buffer_fill(data[i].buffer, 0, (guint8*)imageR->imageData, size);
        }
            
        gst_buffer_unmap(data[i].buffer, &info);
        
        
        //GST_BUFFER_DURATION(buffer) = duration;
        //GST_BUFFER_PTS(buffer) = timestamp;
        //GST_BUFFER_DTS(buffer) = timestamp;
    
        //set the current number in the frame
        //GST_BUFFER_OFFSET(buffer) =  num_frames;
    
        //ret = gst_app_src_push_buffer(GST_APP_SRC(source[i]), buffer);
        //if (ret != GST_FLOW_OK) {
        //    L_(lwarning) << ("Error pushing buffer to GStreamer pipeline");
        //    return false;
        //}
        data[i].haveData = true;
        g_mutex_unlock(&data[i].mutex);
        g_cond_signal(&data[i].cond);
    }


    return true;
}
