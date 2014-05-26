
plugin_LTLIBRARIES = libgstsequences.la

libgstsequences_la_SOURCES = \
	gstimagesequencesrc.c   \
    gstimagesequence.c
libgstsequences_la_CFLAGS = $(GST_PLUGINS_BASE_CFLAGS) $(GST_BASE_CFLAGS) $(GST_CFLAGS) $(GIO_CFLAGS)
libgstsequences_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) -lgstvideo-@GST_API_VERSION@ $(GST_BASE_LIBS) $(GST_LIBS) $(GIO_LIBS)
libgstsequences_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstsequences_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstimagesequencesrc.h 


Android.mk: Makefile.am $(BUILT_SOURCES)
	androgenizer \
	-:PROJECT libgstsequences -:SHARED libgstsequences \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgstsequences_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(DEFAULT_INCLUDES) $(libgstsequences_la_CFLAGS) \
	 -:LDFLAGS $(libgstsequences_la_LDFLAGS) \
	           $(libgstsequences_la_LIBADD) \
	           -ldl \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
		       LOCAL_MODULE_PATH:='$$(TARGET_OUT)/lib/gstreamer-0.10' \
	> $@
