#include "GstInput.hpp"
#include "GstOutput.hpp"
#include "GstShow.hpp"
#include <QtGui> 
//#include <cv.h>
#include <highgui.h>

class VideoTest : public QObject
{
    Q_OBJECT
public:
    VideoTest(char * _lvideo, char * _rvideo, int _wid);
    ~VideoTest();
    void Run();
    
public Q_SLOTS:
    void show();
    void app();
    void opencv();
    
private:
  GstInput * tin;
  GstOutput * tout;
  GstShow * tshow;
  char * lvideo;
  char * rvideo;
  int wid;
};
