#include <cv.h>
#include <vector>
#include <gst/gst.h>
#include <gst/gstbuffer.h>
#include <gst/video/video.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/pbutils/missing-plugins.h>

typedef struct inputData
{
    // used to sync receiver
	GMutex mutex;
	GCond cond;
    GstBuffer* buffer;
    bool haveData;
    int fps;
    unsigned int iFrame;
    bool stop;
    int bufferSize;
} inputData_t;

void handleMessage(GstElement * pipeline);
 
class GstOutput 
{
public:
    GstOutput() { init(); }
    ~GstOutput() { close(); }

    bool openAppSrc( const int xwinid, 
                      double fps, CvSize frameSize, bool is_color );
    //bool join(GstElement * lpipeline, GstElement * rpipeline);
    bool start();
    void close();
    bool feedFrame(  const IplImage * imageL, const IplImage * imageR );
    
protected:
    void init();
    bool init_pipeline(const int  xwinid);
    bool finish_pipeline();
    
    static void cb_need_data(GstAppSrc *appsrc,
                            guint       unused_size,
                            gpointer    user_data);
    
    GstElement* pipeline;
    GstElement* source[2];
    GstElement* scale[2];
    GstElement* scalefilter[2];
    GstElement* queue[2];
    
    GstElement* mixer;
    GstElement* videosink;

    inputData_t data[2];
    
    int input_pix_fmt;
    int num_frames;
    double framerate;
};
