#include <queue>
#include <cv.h>
#include <vector>
#include <gst/gst.h>
#include <gst/net/gstnet.h>
#include <gst/gstbuffer.h>
#include <gst/video/video.h>
#include <gst/app/gstappsink.h>
#include <gst/pbutils/missing-plugins.h>
#include <gst/atime/gstrtpatimemeta.h>

// for timings
#include <time.h>


void handleMessage(GstElement * pipeline);

typedef struct queuedData
{
    // used to sync receiver
	GMutex mutex;
	GCond cond;
    std::queue<GstBuffer*> buffers;
    gint width;
    gint height;
    gint depth; //=3
    bool stopPushing;
} queuedData_t;

class GstInput
{
  public:
    GstInput();
    ~GstInput();
    bool openRTSP(std::string laddress, std::string raddress);
    bool openFile(std::string laddress, std::string raddress);
    
    bool storeRTSP(std::string laddress, std::string raddress, const char * Lfilename, const char * Rfilename);
    bool grabFrames();
    bool retriveFrames(IplImage *  & OutFrame1, IplImage *  & OutFrame2, int & sync);
    bool getFrameSize(int & w1, int & h1, int & w2, int & h2);
    
  private:
    bool _startClock();
    bool _prepareAppSinks(int, bool);
    bool open(std::string laddress, std::string raddress);
    bool reopen();
    void close();
    bool isPipelinePlaying();
    void startPipeline();
    void stopPipeline();
    void restartPipeline();
    void setFilter(const char* prop, int type, int v1, int v2 = 0);
    void removeFilter(const char *filter);
    static GstFlowReturn storeFrameCallback(GstAppSink *appsink, gpointer user_data);
    static void source_created (GstElement * pipe, GstElement * source);
    static void newPad(GstElement * myelement, GstPad * pad,gpointer data);
    static void on_pad_added (GstElement *src, GstPad *new_pad, GstElement *rtph264depay);
    GstClock *global_clock;
    gchar *server1;
    gchar *server2;
    gint clock_port1;
    gint clock_port2;
    GstElement *pipeline[2];
    GstElement *source[2];
    GstElement *color[2];
    GstElement *sink[2];
    GstElement *sinkf[2];
    
    GstSample *  sample[2];
    GstBuffer * buffer[2];
    GstMapInfo*   info[2];

      
    GstCaps*      caps;
    
    queuedData_t  queue[2];
    
    // for opencv display
    IplImage*     frame[2];
  
    // frame buffers, in the future
    //std::vector<char *, GstClockTime> LFrameBuffer;
    //std::vector<char *, GstClockTime> RFrameBuffer;
    
    // for timings, JUST DEBUG
    struct timespec starttime;
    struct timespec lasttime;  
  
};

