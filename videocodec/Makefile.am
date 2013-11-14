libOMXComponentCodec_source_cpp = \
	OMXComponentCodecBase.cpp \
	$(NULL)

libOMXComponentCodec_source_h = \
	OMXComponentCodecBase.h \
	$(NULL)

libOMXVideoDecoderAVC_source_cpp = \
	$(libOMXComponentCodec_source_cpp) \
	OMXVideoDecoderBase.cpp \
	OMXVideoDecoderAVC.cpp \
	$(NULL)

libOMXVideoDecoderAVC_source_h = \
	vabuffer.h \
	$(NULL)

libOMXVideoDecoderAVC_source_priv_h = \
	$(libOMXComponentCodec_source_h) \
	OMXVideoDecoderAVC.h \
	OMXComponentDefines.h \
	OMXVideoDecoderBase.h  \
	$(NULL)

libOMXVideoDecoderVP8_source_cpp = \
	OMXVideoDecoderVP8.cpp \
	$(NULL)

libOMXVideoDecoderVP8_source_priv_h = \
	OMXVideoDecoderVP8.h \
	$(NULL)

libOMXVideoEncoderAVC_source_cpp = \
	$(libOMXComponentCodec_source_cpp) \
	OMXVideoEncoderBase.cpp \
	OMXVideoEncoderAVC.cpp \
	$(NULL)

libOMXVideoEncoderAVC_source_priv_h = \
	$(libOMXComponentCodec_source_h) \
	OMXVideoEncoderAVC.h \
	OMXVideoEncoderBase.h  \
	$(NULL)

libOMXVideoDecoder_ldflags = \
	-version-info 0:6:0 \
	$(OMXIL_UTILS_LIBS) \
	$(OMXIL_BASE_LIBS) \
	$(VAVIDDEC_DEPS_LIBS) \
	$(LIBVA_DEPS_LIBS) \
	-ldl -lpthread \
	$(NULL)

libOMXVideoEncoderAVC_ldflags = \
	-version-info 0:6:0 \
	$(OMXIL_UTILS_LIBS) \
	$(OMXIL_BASE_LIBS) \
	$(VAVIDENC_DEPS_LIBS) \
	$(LIBVA_DEPS_LIBS) \
	-ldl -lpthread \
	$(NULL)

lib_LTLIBRARIES				= libOMXVideoDecoderAVC.la libOMXVideoEncoderAVC.la libOMXVideoDecoderVP8.la
libOMXVideoDecoderAVCincludedir		= ${includedir}/omx
libOMXVideoDecoderAVCinclude_HEADERS	= $(libOMXVideoDecoderAVC_source_h)
noinst_HEADERS				= $(libOMXVideoDecoderAVC_source_priv_h) $(libOMXVideoDecoderVP8_source_priv_h) \
					  $(libOMXVideoEncoderAVC_source_priv_h)
libOMXVideoDecoderAVC_la_SOURCES	= $(libOMXVideoDecoderAVC_source_cpp)
libOMXVideoDecoderAVC_la_LDFLAGS	= $(libOMXVideoDecoder_ldflags)
libOMXVideoDecoderAVC_la_CPPFLAGS       = $(OMXIL_UTILS_CFLAGS) $(OMXIL_BASE_CFLAGS) $(VAVIDDEC_DEPS_CFLAGS) \
					  $(LIBVA_DEPS_CFLAGS) $(MIXVBP_CFLAGS) $(MIXVBP_H264_CFLAGS)

libOMXVideoDecoderVP8includedir		= ${includedir}/omx
libOMXVideoDecoderVP8include_HEADERS	= $(libOMXVideoDecoderVP8_source_h)
libOMXVideoDecoderVP8_la_SOURCES	= $(libOMXVideoDecoderVP8_source_cpp)
libOMXVideoDecoderVP8_la_LDFLAGS	= $(libOMXVideoDecoder_ldflags)
libOMXVideoDecoderVP8_la_CPPFLAGS       = $(OMXIL_UTILS_CFLAGS) $(OMXIL_BASE_CFLAGS) $(VAVIDDEC_DEPS_CFLAGS) \
					  $(LIBVA_DEPS_CFLAGS) $(MIXVBP_CFLAGS) $(MIXVBP_VP8_CFLAGS)

libOMXVideoEncoderAVCincludedir		= ${includedir}/omx
libOMXVideoEncoderAVCinclude_HEADERS	= $(libOMXVideoEncoderAVC_source_h)
libOMXVideoEncoderAVC_la_SOURCES	= $(libOMXVideoEncoderAVC_source_cpp)
libOMXVideoEncoderAVC_la_LDFLAGS	= $(libOMXVideoEncoderAVC_ldflags)
libOMXVideoEncoderAVC_la_CPPFLAGS       = $(OMXIL_UTILS_CFLAGS) $(OMXIL_BASE_CFLAGS) $(VAVIDENC_DEPS_CFLAGS) \
					  $(LIBVA_DEPS_CFLAGS)
DISTCLEANFILES = Makefile.in
