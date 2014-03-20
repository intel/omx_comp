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

libOMXVideoDecoderMJPEG_source_cpp = \
	OMXVideoDecoderMJPEG.cpp \
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

libOMXVideoDecoderMJPEG_source_priv_h = \
	OMXVideoDecoderMJPEG.h \
	$(NULL)


libOMXVideoDecoder_ldflags = \
	-version-info 0:6:0 \
	$(OMXIL_UTILS_LIBS) \
	$(OMXIL_BASE_LIBS) \
	$(VAVIDDEC_DEPS_LIBS) \
	$(LIBVA_DEPS_LIBS) \
	-ldl -lpthread \
	$(NULL)


libOMXVideoDecoderAVCincludedir		= ${includedir}/omx
libOMXVideoDecoderAVCinclude_HEADERS	= $(libOMXVideoDecoderAVC_source_h) $(libOMXVideoDecoder_source_h)
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

libOMXVideoDecoderMJPEGincludedir	= ${includedir}/omx
libOMXVideoDecoderMJPEGinclude_HEADERS	= $(libOMXVideoDecoderMJPEG_source_h)
libOMXVideoDecoderMJPEG_la_SOURCES	= $(libOMXVideoDecoderMJPEG_source_cpp) $(libOMXVideoDecoder_source_cpp)
libOMXVideoDecoderMJPEG_la_LDFLAGS	= $(libOMXVideoDecoder_ldflags)
libOMXVideoDecoderMJPEG_la_CPPFLAGS	= $(OMXIL_UTILS_CFLAGS) $(OMXIL_BASE_CFLAGS) $(VAVIDDEC_DEPS_CFLAGS) \
					  $(LIBVA_DEPS_CFLAGS) $(MIXVBP_CFLAGS) $(MIXVBP_MJPEG_CFLAGS)


#encoder related
libOMXVideoEncoder_source_cpp = \
	OMXComponentCodecBase.cpp \
	OMXVideoEncoderBase.cpp \
	$(NULL)

libOMXVideoEncoder_source_h = \
	vabuffer.h \
	$(NULL)

libOMXVideoEncoder_source_priv_h = \
	OMXComponentCodecBase.h  \
	OMXComponentDefines.h \
	OMXVideoEncoderBase.h  \
	$(NULL)

libOMXVideoEncoderAVC_source_cpp = \
	OMXVideoEncoderAVC.cpp \
	$(NULL)

libOMXVideoEncoderAVC_source_priv_h = \
	OMXVideoEncoderAVC.h \
	$(NULL)

libOMXVideoEncoder_ldflags = \
	-version-info 0:6:0 \
	$(OMXIL_UTILS_LIBS) \
	$(OMXIL_BASE_LIBS) \
	$(VAVIDENC_DEPS_LIBS) \
	$(LIBVA_DEPS_LIBS) \
	-ldl -lpthread \
	$(NULL)




libOMXVideoEncoderAVCincludedir		= ${includedir}/omx
libOMXVideoEncoderAVCinclude_HEADERS	= $(libOMXVideoEncoderAVC_source_h) $(libOMXVideoEncoder_source_h)
libOMXVideoEncoderAVC_la_SOURCES	= $(libOMXVideoEncoderAVC_source_cpp) $(libOMXVideoEncoder_source_cpp)
libOMXVideoEncoderAVC_la_LDFLAGS	= $(libOMXVideoEncoder_ldflags)
libOMXVideoEncoderAVC_la_CPPFLAGS       = $(OMXIL_UTILS_CFLAGS) $(OMXIL_BASE_CFLAGS) $(VAVIDENC_DEPS_CFLAGS) \
					  $(LIBVA_DEPS_CFLAGS) $(MIXVBP_CF LAGS) $(MIXVBP_H264_CFLAGS)

lib_LTLIBRARIES				= libOMXVideoDecoderAVC.la libOMXVideoDecoderVP8.la libOMXVideoDecoderMJPEG.la libOMXVideoEncoderAVC.la
noinst_HEADERS				= $(libOMXVideoDecoderAVC_source_priv_h) $(libOMXVideoDecoderVP8_source_priv_h) $(libOMXVideoDecoder_source_priv_h) $(libOMXVideoDecoderMJPEG_source_priv_h)
						$(libOMXVideoEncoderAVC_source_priv_h) $(libOMXVideoEncoder_source_priv_h) 
DISTCLEANFILES = Makefile.in
