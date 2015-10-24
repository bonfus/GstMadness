plugin_LTLIBRARIES = libgstrtpatime.la

libgstrtpatime_la_SOURCES = \
	gstrtpatime.c \
	gstrtpatimetimestamp.c gstrtpatimetimestamp.h \
	gstrtpatimeparse.c gstrtpatimeparse.h \
	gstrtpatimemeta.c gstrtpatimemeta.h
	
libgstrtpatime_@GST_API_VERSION@includedir = \
        $(includedir)/gstreamer-@GST_API_VERSION@/gst/atime
libgstrtpatime_@GST_API_VERSION@include_HEADERS = \
	gstrtpatimemeta.h
	
libgstrtpatime_la_CFLAGS = $(GST_PLUGINS_BAD_CFLAGS) $(GST_PLUGINS_BASE_CFLAGS) \
	$(GST_BASE_CFLAGS) $(GST_CFLAGS)
libgstrtpatime_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) \
	$(GST_BASE_LIBS) $(GST_LIBS) -lgstrtp-$(GST_API_VERSION)
libgstrtpatime_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstrtpatime_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)
