# Copyright (c) 2009-2010 Wind River Systems, Inc.
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	sst.cpp

LOCAL_MODULE := libwrs_omxil_intel_mrst_sst

LOCAL_CPPFLAGS :=

LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
	libwrs_omxil_common \
	liblog \
	libglib-2.0 \
	libgobject-2.0 \
	libmixcommon \
	libmixaudio

LOCAL_C_INCLUDES := \
	$(WRS_OMXIL_CORE_ROOT)/utils/inc \
	$(WRS_OMXIL_CORE_ROOT)/base/inc \
	$(WRS_OMXIL_CORE_ROOT)/core/inc/khronos/openmax/include \
	$(PV_INCLUDES) \
	$(GLIB_TOP) \
	$(GLIB_TOP)/glib \
	$(GLIB_TOP)/android \
	$(GLIB_TOP)/gobject \
	$(TARGET_OUT_HEADERS)/libmixcommon \
	$(TARGET_OUT_HEADERS)/libmixaudio

include $(BUILD_SHARED_LIBRARY)
