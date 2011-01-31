ifeq ($(strip $(BOARD_USES_WRS_OMXIL_CORE)),true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)


LOCAL_SRC_FILES := \
	src/baseEncoder.cpp \
	src/codecHelper.cpp

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libwrs_omxil_base_encoder

LOCAL_LDFLAGS :=

LOCAL_CFLAGS := -DANDROID -DLINUX -DDXVA_VERSION -march=i686 -msse -Wnon-virtual-dtor 

LOCAL_SHARED_LIBRARIES := \
        libva \
	libva-android \
        libva-tpi \
        libutils
	
LOCAL_C_INCLUDES := \
	$(WRS_OMXIL_CORE_ROOT)/utils/inc \
	$(WRS_OMXIL_CORE_ROOT)/base/inc \
	$(WRS_OMXIL_CORE_ROOT)/core/inc/khronos/openmax/include \
	$(LIBVA_TOP) \
	$(LIBBASECODEC_TOP)/inc

include $(BUILD_SHARED_LIBRARY)
endif
