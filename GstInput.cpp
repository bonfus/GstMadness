#include "GstInput.hpp"
#include "GstCommon.hpp"
#include "log.h"
#define PLAYBACK_DELAY_MS 50
#define PIPELINE_LATENCY_MS 500
#define APP_SINK_MAX_BUFFERS 1

//const GstMetaInfo *info = ATIME_META_INFO;


GstInput::~GstInput()
{
    close();
}

GstInput::GstInput()
{
    global_clock=NULL;
    server1=NULL;
    server2=NULL;
    gst_initializer::init();
        
    for (int i=0; i < 2; i++)
    {
        pipeline[i]=NULL;
        source[i]=NULL;
    
        color[i]=NULL;
        sink[i]=NULL;
        sinkf[i]=NULL;
        
        queue[i].width=0;
        queue[i].height=0;
        queue[i].depth=0; //=3
        queue[i].depth=0; //=3
        queue[i].stopPushing=false;
        g_mutex_init(&queue[i].mutex);
        
        // current buffer
        buffer[i] = NULL;
        
        info[i] = new GstMapInfo;
        // for opencv display
        frame[i] = NULL;
    }
    
    caps=NULL;

}  

void
GstInput::source_created (GstElement * pipe, GstElement * source)
{
  g_object_set (source, "latency", PLAYBACK_DELAY_MS,
      "ntp-time-source", 3, "buffer-mode", 4, "ntp-sync", TRUE, NULL);
}

void GstInput::newPad(GstElement *src,
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

void GstInput::on_pad_added (GstElement *src, GstPad *new_pad, GstElement *rtph264depay) {
  GstPad *sink_pad = gst_element_get_static_pad (rtph264depay, "sink");
  GstPadLinkReturn ret;
  GstCaps *new_pad_caps = NULL;
  GstStructure *new_pad_struct = NULL;
  const gchar *new_pad_type = NULL;

  //this is wrong
  //g_object_set (G_OBJECT(src), "latency", PLAYBACK_DELAY_MS,
  //    "ntp-time-source", 3, "buffer-mode", 4, "ntp-sync", TRUE, NULL);	

  g_print ("Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

  /* Check the new pad's type */
  new_pad_caps = gst_pad_get_current_caps (new_pad);
  new_pad_struct = gst_caps_get_structure (new_pad_caps, 0);
  new_pad_type = gst_structure_get_name (new_pad_struct);
  if (!g_str_has_prefix (new_pad_type, "application/x-rtp")) {
    g_print ("  It has type '%s' which is not RTP. Ignoring.\n", new_pad_type);
    /* Unreference the new pad's caps, if we got them */
    if (new_pad_caps != NULL)
        gst_caps_unref (new_pad_caps);
    
    /* Unreference the sink pad */
    gst_object_unref (sink_pad);
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



bool GstInput::openFile(std::string Laddress, std::string Raddress)
{
    return open(Laddress, Raddress);
}

bool GstInput::openRTSP(std::string Laddress, std::string Raddress)
{
    gst_initializer::init();
  
    if(!gst_uri_is_valid(Laddress.c_str()) || !gst_uri_is_valid(Raddress.c_str()))
    {
        return false;
    }
    
    
    if (!_startClock()) {
        L_(lerror) << "Cannot start clock";
        return false;
    }   
    
    
    
    L_(ldebug4) << ("Creating pipelines");
    for (int i =0; i<2; i++)
    {
        pipeline[i] = gst_pipeline_new(NULL);

        if(!pipeline[i])
            return false;
    
        source[i] = gst_element_factory_make ("rtspsrc", NULL);
        
        if (i==0)
            g_object_set (source[i], "location", Laddress.c_str(), NULL);
        else
            g_object_set (source[i], "location", Raddress.c_str(), NULL);
    
    
        //sync    
        g_object_set (source[i], "latency", PLAYBACK_DELAY_MS,
            "ntp-time-source", 3, "buffer-mode", 4, "ntp-sync", TRUE, NULL);
    
        //Set latency if we have a stream
            /* Set this high enough so that it's higher than the minimum latency
            * on all receivers */
        gst_pipeline_set_latency (GST_PIPELINE (pipeline[i]), PIPELINE_LATENCY_MS * GST_MSECOND);
    
        GstElement * atime = gst_element_factory_make("rtpatimeparse", NULL);
    
        GstElement * rtph264depay = gst_element_factory_make("rtph264depay", NULL);
        
        GstElement * decode = gst_element_factory_make("decodebin", NULL);
        
        GstElement * queue = gst_element_factory_make("queue", NULL);
    
        GstElement * color = gst_element_factory_make("autovideoconvert", NULL);
    
        _prepareAppSinks(i, false);
    
        gst_bin_add_many(GST_BIN(pipeline[i]), source[i], atime, rtph264depay, decode, queue, color, sink[i], NULL);
    
        gst_pipeline_use_clock (GST_PIPELINE (pipeline[i]), global_clock);
            
    
        if(!gst_element_link_many (atime, rtph264depay, decode, NULL))
        {
            L_(lerror) << "Could not link pipeline " << i;
            gst_object_unref(pipeline[i]);
            pipeline[i] = NULL;
            return false;
        }
        
        //caps = gst_caps_from_string("video/x-raw, format=(string)BGR; video/x-bayer,format=(string){rggb,bggr,grbg,gbrg}");
        
        if(!gst_element_link(color, sink[i]))
        {
            L_(lerror) << "Could not link pipeline " << i;
            gst_object_unref(pipeline[i]);
            pipeline[i] = NULL;
            return false;
        }
        g_signal_connect(source[i], "pad-added", G_CALLBACK(on_pad_added), atime);
        g_signal_connect(decode, "pad-added", G_CALLBACK(newPad), color);
    }
    
    // start the pipeline if it was not in playing state yet
    if(!this->isPipelinePlaying())
    {
        L_(ldebug4) << ("Starting pipelines");
        this->startPipeline();
    }
    for (int i=0; i<2;i++)
        handleMessage(pipeline[i]);
        
    return true;
}

bool GstInput::open(std::string Laddress, std::string Raddress)
{
    bool file = false;
    char *uriL = NULL;
    char *uriR = NULL;
  
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
        _startClock();
    }
    
    L_(ldebug2) << ("Creating pipelines");
    
    for (int i=0; i<2; i++)
    {
        GstElement * atime=NULL;
        pipeline[i] = gst_pipeline_new(NULL);
        

        source[i] = gst_element_factory_make ("uridecodebin", NULL);
        
        if (i==0)
            g_object_set (source[i], "uri", uriL, NULL);
        else
            g_object_set (source[i], "uri", uriR, NULL);
        
        color[i] = gst_element_factory_make("autovideoconvert", NULL);
    
        //sync if not file!
        if (!file)
        {
            gst_pipeline_use_clock (GST_PIPELINE (pipeline[i]), global_clock);
            
            //g_signal_connect (source[i], "source-setup", G_CALLBACK (source_created), NULL);
        /* Set this high enough so that it's higher than the minimum latency
        * on all receivers */
            gst_pipeline_set_latency (GST_PIPELINE (pipeline[i]), PIPELINE_LATENCY_MS * GST_MSECOND);
            
            
        } 
        g_signal_connect(source[i], "pad-added", G_CALLBACK(newPad), color[i]);
        
        _prepareAppSinks(i, file);
    
        gst_bin_add_many(GST_BIN(pipeline[i]), source[i], color[i], sink[i], NULL);

        //link elements
        if(!gst_element_link(color[i], sink[i])) {
            g_print( "GStreamer: cannot link color -> sink\n");
            gst_object_unref(pipeline[i]);
            pipeline[i] = NULL;
            return false;
        }
    }
    return true;
}


bool GstInput::_prepareAppSinks(int i, bool file)
{
    sink[i] = gst_element_factory_make("appsink", NULL);    
    ////TODO: is 1 single buffer really high enough?
    //if (!file)
    //{
    //    gst_app_sink_set_max_buffers (GST_APP_SINK(sink[i]), APP_SINK_MAX_BUFFERS);
    //    gst_app_sink_set_drop (GST_APP_SINK(sink[i]), FALSE);
    //} else {
    //    gst_app_sink_set_max_buffers (GST_APP_SINK(sink[i]), 1);
    //    gst_app_sink_set_drop (GST_APP_SINK(sink[i]), FALSE);
    //}
    //
    ////do not emit signals: all calls will be synchronous and blocking
    //// but callback possibility is interesting! 
    //gst_app_sink_set_emit_signals (GST_APP_SINK(sink[i]), 0);
    

	GstAppSinkCallbacks appsinkCallbacks;
	appsinkCallbacks.new_preroll	= NULL;
	appsinkCallbacks.new_sample		= &storeFrameCallback;
	appsinkCallbacks.eos			= NULL; 
	
	
	gst_app_sink_set_drop			(GST_APP_SINK(sink[i]), FALSE);
	gst_app_sink_set_max_buffers	(GST_APP_SINK(sink[i]), 1);
	gst_app_sink_set_callbacks		(GST_APP_SINK(sink[i]), &appsinkCallbacks, &queue[i], NULL);		

    

    caps = gst_caps_from_string("video/x-raw, format=(string){BGR, GRAY8}; video/x-bayer,format=(string){rggb,bggr,grbg,gbrg}");

    gst_app_sink_set_caps(GST_APP_SINK(sink[i]), caps);
    
    gst_caps_unref(caps);    
}

bool GstInput::_startClock()
{
    global_clock = gst_system_clock_obtain ();
    gst_net_time_provider_new (global_clock, "0.0.0.0", 8554);
    if (global_clock != NULL)
        L_(ldebug2) << ("Clock created!");
    else
    {
        L_(lerror) << ("Could not create clock!");
        return false;
    }
    return true; 
} 

bool GstInput::storeRTSP(std::string Laddress, std::string Raddress, const char * Lfilename, const char * Rfilename)
{    
    gst_initializer::init();
  
    if(!gst_uri_is_valid(Laddress.c_str()) || !gst_uri_is_valid(Raddress.c_str()))
    {
        return false;
    }
    
    
    if (!_startClock()) {
        L_(lerror) << "Cannot start clock";
        return false;
    }   
    
    L_(ldebug4) << ("Creating pipelines");
    for (int i =0; i<2; i++)
    {
        pipeline[i] = gst_pipeline_new(NULL);

        if(!pipeline[i])
            return false;
    
        source[i] = gst_element_factory_make ("rtspsrc", NULL);
        
        if (i==0)
            g_object_set (source[i], "location", Laddress.c_str(), NULL);
        else
            g_object_set (source[i], "location", Raddress.c_str(), NULL);
    
    
        //sync    
        g_object_set (source[i], "latency", PLAYBACK_DELAY_MS,
            "ntp-time-source", 3, "buffer-mode", 4, "ntp-sync", TRUE, NULL);
    
        //Set latency if we have a stream
            /* Set this high enough so that it's higher than the minimum latency
            * on all receivers */
        gst_pipeline_set_latency (GST_PIPELINE (pipeline[i]), PIPELINE_LATENCY_MS * GST_MSECOND);
    
        GstElement * atime = gst_element_factory_make ("rtpatimeparse", NULL);
    
        GstElement * rtph264depay = gst_element_factory_make ("rtph264depay", NULL);
    
        GstElement * mpegtsmux = gst_element_factory_make ("mpegtsmux", NULL);
    
        // saves from delays IN WRITING!   
        GstElement * queue = gst_element_factory_make ("queue", NULL);
    

    
        GstElement * filesink = gst_element_factory_make ("filesink", NULL);
    
        g_assert (filesink);
        
        if(i==0)
            g_object_set (G_OBJECT (filesink), "location", Lfilename, NULL);
        else
            g_object_set (G_OBJECT (filesink), "location", Rfilename, NULL);  
    
        gst_bin_add_many(GST_BIN(pipeline[i]), source[i], atime, rtph264depay, mpegtsmux, queue, filesink, NULL);
    
    
        gst_pipeline_use_clock (GST_PIPELINE (pipeline[i]), global_clock);
            
    
        if(!gst_element_link_many (atime, rtph264depay, mpegtsmux, queue, filesink, NULL))
        {
            L_(lerror) << "Could not link pipeline " << i;
            gst_object_unref(pipeline[i]);
            pipeline[i] = NULL;
            return false;
        }

    
        g_signal_connect(source[i], "pad-added", G_CALLBACK(on_pad_added), atime);
    }
    
    // start the pipeline if it was not in playing state yet
    if(!this->isPipelinePlaying())
    {
        L_(ldebug4) << ("Starting pipelines");
        this->startPipeline();
    }
    for (int i=0; i<2;i++)
        handleMessage(pipeline[i]);
        
    return true;
}

//void saveRTSPFrame()
//{
//  gst_bin_iterate (GST_BIN (pipeline1)));
//  gst_bin_iterate (GST_BIN (pipeline2)));
//}



/*!
 * \brief GstInput::grabFrame
 * \return
 * Grabs a sample from the pipeline, awaiting consumation by retreiveFrame.
 * The pipeline is started if it was not running yet
 */
bool GstInput::grabFrames()
{
    for (int i=0; i<2; i++)
    {
        if(!pipeline[i]) // || !pipeline2)
            return false;
    }

    // start the pipeline if it was not in playing state yet
    if(!this->isPipelinePlaying())
    {
        this->startPipeline();
    }

    for (int i=0; i<2; i++)
    {
        // bail out if EOS
        if(gst_app_sink_is_eos(GST_APP_SINK(sink[i])))
            return false;
    }

    for (int i=0; i<2; i++)
    {
        // get frame
        if(sample[i])
            gst_sample_unref(sample[i]);


        sample[i] = gst_app_sink_pull_sample(GST_APP_SINK(sink[i]));
        
        if(!sample[i])
            return false;
    }
    
    return true;
}

GstFlowReturn GstInput::storeFrameCallback(GstAppSink *appsink, gpointer user_data)
{
    GstSample * sample = gst_app_sink_pull_sample (appsink);
    
    queuedData_t * queue = static_cast<queuedData_t *>(user_data);
    
    g_print("Storing buffer\n");
    
    if (0 != sample) {
        //GstSegment * segment = gst_sample_get_segment(sample);
            
        GstBuffer * buffer = gst_sample_get_buffer(sample);
        
        if (0 != buffer) {
            
            // set frame info if missing!
            if(queue->width<=0)
            {
        
                GstCaps* buffer_caps = gst_sample_get_caps(sample);
        
                // bail out in no caps
                assert(gst_caps_get_size(buffer_caps) == 1);
                GstStructure* structure = gst_caps_get_structure(buffer_caps, 0);
        
                // bail out if width or height are 0
                if(!gst_structure_get_int(structure, "width", &(queue->width)) ||
                        !gst_structure_get_int(structure, "height", &(queue->height)))
                {
                    gst_caps_unref(buffer_caps);
                    return GST_FLOW_ERROR;
                }
        
                uint depth = 3;
        
                depth = 0;
                const gchar* name = gst_structure_get_name(structure);
                const gchar* format = gst_structure_get_string(structure, "format");
        
                if (!name || !format)
                    return GST_FLOW_ERROR;
        
                // we support 3 types of data:
                //     video/x-raw, format=BGR   -> 8bit, 3 channels
                //     video/x-raw, format=GRAY8 -> 8bit, 1 channel
                //     video/x-bayer             -> 8bit, 1 channel
                // bayer data is never decoded, the user is responsible for that
                // everything is 8 bit, so we just test the caps for bit depth
            
                if (strcasecmp(name, "video/x-raw") == 0)
                {
                    if (strcasecmp(format, "BGR") == 0) {
                        depth = 3;
                    }
                    else if(strcasecmp(format, "GRAY8") == 0){
                        depth = 1;
                    }
                }
                else if (strcasecmp(name, "video/x-bayer") == 0)
                {
                    depth = 1;
                }
            
                if (depth > 0) {
                    queue->depth = depth;
                } else {
                    gst_caps_unref(buffer_caps);
                    return GST_FLOW_ERROR;
                }
            
                gst_caps_unref(buffer_caps);
            }
            
            
            // take owonership
            
            g_mutex_lock(&(queue->mutex));
            if (!queue->stopPushing) {
                gst_buffer_ref(buffer);
                queue->buffers.push(buffer);
            } else {
                // anything we should do? Like eos?
            }
            
            g_mutex_unlock(&(queue->mutex));
            g_cond_signal(&(queue->cond));
            
            //gst_sample_unref(sample);
            g_print("Pulled buffer from appsink, pushed to queue\n");
        }
        gst_sample_unref(sample);
    }
    return GST_FLOW_OK;
}

bool GstInput::retriveFrames(IplImage *  & OutFrame1, IplImage *  & OutFrame2, int & sync)
{
    GstClockTime pts[2];

    const GstMetaInfo *metainfo = ATIME_META_INFO;

    for (int i=0; i<2; i++)
    {
        g_mutex_lock(&(queue[i].mutex));
        if (queue[i].buffers.empty() == true) {
            //g_print("Wait for data...\n");
            g_cond_wait(&(queue[i].cond), &(queue[i].mutex));
            //g_print("Done.\n");
        } else {
            //g_print("Callback: have data %d\n",data->haveData);
        }
        
        // unref old buffer
        if(buffer[i]){
            gst_buffer_unref(buffer[i]);
        }
        
        // grab buffer
        if (queue[i].buffers.empty() == false) {
            buffer[i] = queue[i].buffers.front();
            queue[i].buffers.pop();
            g_mutex_unlock(&(queue[i].mutex));
        } else {
            g_mutex_unlock(&(queue[i].mutex));
            return false;
        }

        if (!buffer[i])
            return false;
        
        pts[i] = GST_CLOCK_TIME_NONE;
        gpointer state = NULL;
        
        
        while (GstMeta *meta = gst_buffer_iterate_meta(buffer[i], &state)) {
            //g_print("META GType: %d\n", meta->info->api);
            if (meta->info->api != metainfo->api)
                continue;
            g_print("Got my meta!\n");
            ATimeMeta *em = reinterpret_cast<ATimeMeta *>(meta);
            pts[i] = em->absoluteTime;
        }
        
        // g_print("Basetime %lf\n", (double) gst_element_get_base_time(pipeline[i]));
        
        //construct a frame header if we did not have any yet
        if(!frame[i])
        {
            frame[i] = cvCreateImageHeader(cvSize(queue[i].width, queue[i].height), IPL_DEPTH_8U, queue[i].depth);
        }
        
        // gstreamer expects us to handle the memory at this point
        // so we can just wrap the raw buffer and be done with it
        
        // the data ptr in GstMapInfo is only valid throughout the mapifo objects life.
        // TODO: check if reusing the mapinfo object is ok.
        gboolean success = gst_buffer_map(buffer[i],info[i], (GstMapFlags)GST_MAP_READ);
        if (!success){
            //something weird went wrong here. abort. abort.
            //fprintf(stderr,"GStreamer: unable to map buffer");
            if(buffer[i])
                gst_buffer_unref(buffer[i]);
            return false;
        }
        frame[i]->imageData = (char*)info[i]->data;
        // do not unmap! We unmap later!
        gst_buffer_unmap(buffer[i],info[i]);
        
        if (i==0)
            OutFrame1 = frame[i];
        else
            OutFrame2 = frame[i];

    }
    //clock_gettime(CLOCK_MONOTONIC, &lasttime);
    //g_print ("Frames %d sync: %lf ms start_time %lf s FPS 1: ~%lf FPS 2: ~%lf FPS prog: ~%lf \n", frames , ((double)((int)(pts2-pts1))/1000000.), ((double)((int)(curtime2-curtime1))/1000000000.), 1./(((double)(pts1-prev_pts1))/1000000000.), 1./(((double)(pts2-prev_pts2))/1000000000.), 
    //    ((double)frames)/(((double)lasttime.tv_sec + 1.0e-9*lasttime.tv_nsec) - ((double)starttime.tv_sec + 1.0e-9*starttime.tv_nsec)) );
    
    //g_print ("PTS1 %lu, PTS2 %lu, PTS1 - PREV2 %f, PTS2 - PREV1 %f \n", pts1, pts2, ((double)(((int) pts1)-((int) prev_pts2)))/1000000., ((double)(((int) pts2)-((int)prev_pts1)))/1000000.);
    
    sync = ((double)((int)(pts[1]-pts[0]))/1000000.);
    
    //fps1 = 1./(((double)(pts1-prev_pts1))/1000000000.);
    //fps2 = 1./(((double)(pts2-prev_pts2))/1000000000.);
    
    g_print("PUSHED FRAME~!!!!\n");
    return true;
}

bool GstInput::getFrameSize(int & w1, int & h1, int & w2, int & h2)
{
    g_mutex_lock(&(queue[0].mutex));
    w1 = queue[0].width;
    h1 = queue[0].height;
    g_mutex_unlock(&(queue[0].mutex));
    
    g_mutex_lock(&(queue[1].mutex));
    w2 = queue[1].width;
    h2 = queue[1].height;
    g_mutex_unlock(&(queue[1].mutex));
    if (w1>0 && h1>0 && w2>0 && h2>0)
        return true;
    else
        return false;
}

/*!
 * \brief GstInput::isPipelinePlaying
 * \return if the pipeline is currently playing.
 */
bool GstInput::isPipelinePlaying()
{
    bool playing = false;
    for (int i = 0; i<2; i++)
    {
        GstState current, pending;
        
        GstClockTime timeout = 5*GST_SECOND;
        if(!GST_IS_ELEMENT(pipeline[i])) {
            return false;
        }
    
        GstStateChangeReturn ret = gst_element_get_state(GST_ELEMENT(pipeline[i]),&current, &pending, timeout);
        if (!ret){
            //fprintf(stderr, "GStreamer: unable to query pipeline state\n");
            return false;
        }
        playing = (current == GST_STATE_PLAYING);
        if (playing == false)
            return false;
    }
    return playing; //(current == GST_STATE_PLAYING);
}

/*!
 * \brief GstInput::startPipeline
 * Start the pipeline by setting it to the playing state
 */
void GstInput::startPipeline()
{

    
    
    for (int i =0; i<2;i++)
    {
        
        if(!pipeline[i])
        {
            stopPipeline();
            return;
        }
        
        GstStateChangeReturn status = gst_element_set_state(GST_ELEMENT(pipeline[i]), GST_STATE_PLAYING);
    
        if (status == GST_STATE_CHANGE_ASYNC)
        {
            // wait for status update
            GstState st1;
            GstState st2;
            status = gst_element_get_state(pipeline[i], &st1, &st2, GST_CLOCK_TIME_NONE);
        }
        if (status == GST_STATE_CHANGE_FAILURE) 
        {
            handleMessage(pipeline[i]);
            gst_object_unref(pipeline[i]);
            pipeline[i] = NULL;
            
            L_(lerror) << "GStreamer: unable to start pipeline";
            L_(lerror) << "GStreamer: stopping pipelines";
            stopPipeline();
            return;
        }
        handleMessage(pipeline[i]);
    }
    //if (error)
    //{
    //    L_(lerror) << "GStreamer: could not start. Stopping!";
    //    stopPipeline();
    //    handleMessage(pipeline1);
    //    handleMessage(pipeline1);
    //    return;
    //}
    L_(ldebug4) << ("Pipelines started!");
    
    //printf("state now playing\n");
}


/*!
 * \brief GstInput::stopPipeline
 * Stop the pipeline by setting it to NULL
 */
void GstInput::stopPipeline()
{
    //bool error = false;
    //fprintf(stderr, "restarting pipeline, going to ready\n");
    for (int i =0; i<2;i++)
    {    
        if (pipeline[i] != NULL)
        {
            if(gst_element_set_state(GST_ELEMENT(pipeline[i]), GST_STATE_NULL) ==
                    GST_STATE_CHANGE_FAILURE) {
                L_(lerror) << "GStreamer: unable to stop pipeline\n";
                gst_object_unref(pipeline[i]);
                pipeline[i] = NULL;
            }
        }
    }
}

/*!
 * \brief GstInput::restartPipeline
 * Restart the pipeline
 */
void GstInput::restartPipeline()
{
    
    for (int i =0; i<2;i++)
    {      
        handleMessage(pipeline[i]);
    }
    this->stopPipeline();
    this->startPipeline();
}

/*!
 * \brief GstInput::close
 * Closes the pipeline and destroys all instances
 */
void GstInput::close()
{
    L_(ldebug2) << ("Closing GstInput!");

    if (isPipelinePlaying())
        this->stopPipeline();    

    for (int i =0; i<2;i++)
    {
        // clean remeaning buffers
        g_mutex_lock(&(queue[i].mutex));
        queue[i].stopPushing = true;
        while (!queue[i].buffers.empty())
        {
            GstBuffer * buf = queue[i].buffers.front();
            if(buf) {
                gst_buffer_unref(buf);
                buf = NULL;
            }
            queue[i].buffers.pop();
        }
        g_mutex_unlock(&(queue[i].mutex));
        g_cond_signal(&queue[i].cond);
    
        if(buffer[i]) {
            gst_buffer_unref(buffer[i]);
        }  
    }  

    for (int i =0; i<2;i++)
    {  
        if(pipeline[i]) {
            gst_element_set_state(GST_ELEMENT(pipeline[i]), GST_STATE_NULL);
            gst_object_unref(GST_OBJECT(pipeline[i]));
            pipeline[i] = NULL;
        }
    }
    
    // is this needed? Or the pipeline destroys the clock?
    L_(ldebug2) << ("Deleting global clock");
    if(global_clock!=NULL)
        g_object_unref(global_clock);



}

