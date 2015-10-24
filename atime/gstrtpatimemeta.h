

#ifndef _GST_META_ATIME_H_
#define _GST_META_ATIME_H_

#include <gst/video/gstvideometa.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideofilter.h>
#include <glib/gstdio.h>

typedef struct __ATimeMeta ATimeMeta;

struct __ATimeMeta {
  
  GstMeta meta;
  
  GstClockTime absoluteTime; 
};

//registering out metadata API definition	
GType atime_meta_api_get_type (void);
#define ATIME_META_API_TYPE (atime_meta_api_get_type())

//finds and returns the metadata with our new API.
#define gst_buffer_get_atime_meta(b) \
  ((ATimeMeta*)gst_buffer_get_meta((b),ATIME_META_API_TYPE))

//removes the metadata of our new API.  
#define gst_buffer_del_atime_meta(b) (gst_buffer_remove_meta((b), gst_buffer_get_atime_meta(b)))  
  
const GstMetaInfo *atime_meta_get_info (void);
#define ATIME_META_INFO (atime_meta_get_info())

ATimeMeta *gst_buffer_add_atime_meta (GstBuffer      *buffer,
GstClockTime absoluteTime);

#endif //_GST_META_ATIME_H_
