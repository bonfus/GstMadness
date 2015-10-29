


#include "VideoTest.hpp"



VideoTest::VideoTest(char * _lvideo, char * _rvideo, int _wid): lvideo(_lvideo), rvideo(_rvideo), wid(_wid)
{
  tin = new GstInput();
  tout = new GstOutput();
  tshow = new GstShow();
}
VideoTest::~VideoTest()
{
    delete tin;
    delete tout;
    delete tshow;
}

void VideoTest::Run()
{
    QTimer::singleShot(100, this , SLOT(app()));
}



void VideoTest::show()
{
    tshow->open(wid,lvideo,rvideo);
}

void VideoTest::app()
{

  int sync;
  IplImage * frameL;
  IplImage * frameR;
  
  if(!tin->openRTSP(lvideo,rvideo)) {
      std::cout << "Wrong usage: " <<  std::endl;
      return ;
  }

  int iFrame(0);
  while(1)
  {
      //if (!tin->grabFrames()){
      //    std::cout << " nograb!" << std::endl;
      //    return ;}
      if (!tin->retriveFrames(frameL, frameR, sync)){
          std::cout << " noretrive!" << std::endl;
          return ;}
      std::cout << " Diff " << std::fixed << sync << std::endl;
      if (iFrame == 0)
      {
          tout->openAppSrc(wid,20,cvSize(frameL->width, frameL->height),true);
      }
      tout->feedFrame(frameL, frameR);
      iFrame++;
  }
  cvReleaseImage(&frameL);
  cvReleaseImage(&frameR);

}


void VideoTest::opencv()
{

  int sync;
  IplImage * frameL;
  IplImage * frameR;
  
  // window for showing data with opencv
  cvNamedWindow("1", CV_WINDOW_AUTOSIZE );
  cvNamedWindow("2", CV_WINDOW_AUTOSIZE );
  cvMoveWindow("1",300,300);  
  cvMoveWindow("2",300,300);    
  
  if(!tin->openRTSP(lvideo,rvideo)) {
      std::cout << "Wrong usage: " <<  std::endl;
      return ;
  }

  int iFrame(0);
  cv::Mat L;
  cv::Mat R;
  while(1)
  {
      //if (!tin->grabFrames()){
      //    std::cout << " nograb!" << std::endl;
      //    return ;}
      if (!tin->retriveFrames(frameL, frameR, sync)){
          std::cout << " noretrive!" << std::endl;
          return ;}
      std::cout << " Diff " << std::fixed << sync << std::endl;
      L = cv::Mat(frameL,false);
      R = cv::Mat(frameR,false);
      
      imshow("1", L);
      imshow("2", R);
      char c = cvWaitKey(10);
      if( c == 10 )
      {
          continue;
      } else if ( c == 27 )
      {
          break;
      }
      //// Save the frame into a file
      //if (iFrame % 1 == 0)
      //{
      //  std::ostringstream str;  
      //  str <<"Frame " << iFrame << " Sync: " << std::fixed << sync << std::endl;
      //      
      //  cv::putText(L, str.str(), cv::Point(100,100), CV_FONT_HERSHEY_PLAIN, 3, CV_RGB(0,255,0));
      //  cv::putText(R, str.str(), cv::Point(100,100), CV_FONT_HERSHEY_PLAIN, 3, CV_RGB(0,255,0));
      //  
      //  cv::imwrite(std::string("/tmp/") + std::to_string (iFrame) + std::string("L.jpg"), L); // A JPG FILE IS BEING SAVED
      //  cv::imwrite(std::string("/tmp/") + std::to_string (iFrame) + std::string("R.jpg"), R); // A JPG FILE IS BEING SAVED
      //}
      
      iFrame++;
  }
  cvReleaseImage(&frameL);
  cvReleaseImage(&frameR);

}
