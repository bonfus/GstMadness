#include <cv.h>
#include <gst/gst.h>

using namespace cv;

static cv::Mutex gst_initializer_mutex;

/*!
 * \brief The gst_initializer class
 * Initializes gstreamer once in the whole process
 */
class gst_initializer
{
public:
    static void init();
private:
    gst_initializer();
};



void handleMessage(GstElement * pipeline);
