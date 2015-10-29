QT       += core gui widgets 
CONFIG   += no_keywords

HEADERS     = VideoTest.hpp
SOURCES     = GstCommon.cpp GstShow.cpp GstInput.cpp GstOutput.cpp VideoTest.cpp main.cpp

LIBS += -L/lib64 -lopencv_calib3d -lopencv_contrib -lopencv_core -lopencv_features2d -lopencv_flann -lopencv_gpu -lopencv_highgui -lopencv_imgproc -lopencv_legacy -lopencv_ml -lopencv_nonfree -lopencv_objdetect -lopencv_ocl -lopencv_photo -lopencv_stitching -lopencv_superres -lopencv_ts -lopencv_video -lopencv_videostab -ltbb -lGL -lGLU -lrt -lpthread -lm -ldl -lgstnet-1.0 -lgstrtsp-1.0 -lgstsdp-1.0 -lgio-2.0 -lgstapp-1.0 -lgstvideo-1.0 -lgstbase-1.0 -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lgstpbutils-1.0 -lgstrtpatimemeta

QMAKE_CXXFLAGS += -DHAVE_X11 -g -O0 -std=c++11 -pthread -pthread -I/usr/include/opencv -I/usr/include/gstreamer-1.0 -I/usr/lib/gstreamer-1.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include

# install
#target.path = myqtapp
#sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS *.pro
#sources.path = .

#INSTALLS += target sources
