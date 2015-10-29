#include <cv.h>
#include <vector>
#include <gst/gst.h>
#include <gst/gstbuffer.h>
#include <gst/video/video.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/pbutils/missing-plugins.h>


void handleMessage(GstElement * pipeline);
 
class GstShow
{
public:
    GstShow() { init(); }
    ~GstShow() { close(); }

    bool open(int xwinid, std::string Laddress, std::string Raddress);

    void close();
    bool displayFrame(  const IplImage * imageL, const IplImage * imageR );
    
protected:
    void init();
    bool init_pipeline(const int  xwinid);
    bool finish_pipeline();
    
    GstClock *global_clock;
    gchar *server1;
    gchar *server2;
    gint clock_port1;
    gint clock_port2;
    
    
    GstElement* pipeline;
    GstElement* source[2];
    GstElement* scale[2];
    GstElement* scalefilter[2];
    GstElement* queue[2];
    
    GstElement* mixer;
    GstElement* videosink;

    GstBuffer* buffer;
    int input_pix_fmt;
    int num_frames;
    double framerate;
private:
    static void source_created (GstElement * pipe, GstElement * source);
    static void newPad(GstElement * myelement, GstPad * pad,gpointer data);    
};
