libOMXVideoDecoder_source_cpp = \
	OMXComponentCodecBase.cpp \
	OMXVideoDecoderBase.cpp \
	$(NULL)

libOMXVideoDecoderAVC_source_cpp = \
	OMXVideoDecoderAVC.cpp \
	$(NULL)

libOMXVideoDecoderVP8_source_cpp = \
	OMXVideoDecoderVP8.cpp \
	$(NULL)

libOMXVideoDecoder_source_h = \
	vabuffer.h \
	$(NULL)

libOMXVideoDecoder_source_priv_h = \
	OMXComponentCodecBase.h  \
	OMXComponentDefines.h \
	OMXVideoDecoderBase.h  \
	$(NULL)

libOMXVideoDecoderAVC_source_priv_h = \
	OMXVideoDecoderAVC.h \
	$(NULL)

libOMXVideoDecoderVP8_source_priv_h = \
	OMXVideoDecoderVP8.h \
	$(NULL)

libOMXVideoDecoder_ldflags = \
	-version-info 0:6:0 \
	$(OMXIL_UTILS_LIBS) \
	$(OMXIL_BASE_LIBS) \
	$(VAVIDDEC_DEPS_LIBS) \
	$(LIBVA_DEPS_LIBS) \
	-ldl -lpthread \
	$(NULL)

lib_LTLIBRARIES				= libOMXVideoDecoderAVC.la libOMXVideoDecoderVP8.la
libOMXVideoDecoderAVCincludedir		= ${includedir}/omx
libOMXVideoDecoderAVCinclude_HEADERS	= $(libOMXVideoDecoderAVC_source_h) $(libOMXVideoDecoder_source_h)
noinst_HEADERS				= $(libOMXVideoDecoderAVC_source_h_priv) $(libOMXVideoDecoderVP8_source_h_priv) $(libOMXVideoDecoder_source_h_priv) 
libOMXVideoDecoderAVC_la_SOURCES	= $(libOMXVideoDecoderAVC_source_cpp) $(libOMXVideoDecoder_source_cpp)
libOMXVideoDecoderAVC_la_LDFLAGS	= $(libOMXVideoDecoder_ldflags)
libOMXVideoDecoderAVC_la_CPPFLAGS       = $(OMXIL_UTILS_CFLAGS) $(OMXIL_BASE_CFLAGS) $(VAVIDDEC_DEPS_CFLAGS) \
					  $(LIBVA_DEPS_CFLAGS) $(MIXVBP_CFLAGS) $(MIXVBP_H264_CFLAGS)

libOMXVideoDecoderVP8includedir		= ${includedir}/omx
libOMXVideoDecoderVP8include_HEADERS	= $(libOMXVideoDecoderVP8_source_h)
libOMXVideoDecoderVP8_la_SOURCES	= $(libOMXVideoDecoderVP8_source_cpp) $(libOMXVideoDecoder_source_cpp)
libOMXVideoDecoderVP8_la_LDFLAGS	= $(libOMXVideoDecoder_ldflags)
libOMXVideoDecoderVP8_la_CPPFLAGS       = $(OMXIL_UTILS_CFLAGS) $(OMXIL_BASE_CFLAGS) $(VAVIDDEC_DEPS_CFLAGS) \
					  $(LIBVA_DEPS_CFLAGS) $(MIXVBP_CFLAGS) $(MIXVBP_VP8_CFLAGS)

DISTCLEANFILES = Makefile.in
