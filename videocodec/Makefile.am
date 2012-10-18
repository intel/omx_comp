libOMXVideoDecoderAVC_source_cpp = \
	OMXComponentCodecBase.cpp \
	OMXVideoDecoderBase.cpp \
	OMXVideoDecoderAVC.cpp \
	$(NULL)

libOMXVideoDecoderAVC_source_h = \
	vabuffer.h \
	$(NULL)

libOMXVideoDecoderAVC_source_priv_h = \
	OMXComponentCodecBase.h  \
	OMXVideoDecoderAVC.h \
	OMXComponentDefines.h \
	OMXVideoDecoderBase.h  \
	$(NULL)
	
libOMXVideoDecoderAVC_ldflags = \
	-version-info 0:1:0 \
	$(NULL)

lib_LTLIBRARIES				= libOMXVideoDecoderAVC.la
libOMXVideoDecoderAVCincludedir		= ${includedir}/omx
libOMXVideoDecoderAVCinclude_HEADERS	= $(libOMXVideoDecoderAVC_source_h)
noinst_HEADERS				= $(libOMXVideoDecoderAVC_source_h_priv)
libOMXVideoDecoderAVC_la_SOURCES	= $(libOMXVideoDecoderAVC_source_cpp)
libOMXVideoDecoderAVC_la_LDFLAGS	= $(libOMXVideoDecoderAVC_ldflags)
libOMXVideoDecoderAVC_la_CPPFLAGS       = $(OMXIL_UTILS_CFLAGS) $(OMXIL_BASE_CFLAGS) $(MIXVBP_CFLAGS) $(MIXVBP_H264_CFLAGS) 

DISTCLEANFILES = Makefile.in
