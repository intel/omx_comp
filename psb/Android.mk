LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

WRS_OMXIL_CORE_ROOT := vendor/wrs/addon/libs/wrs-omxil-core

LOCAL_SRC_FILES := \
	psb.cpp

LOCAL_MODULE := libwrs_omxil_intel_mrst_psb

LOCAL_CPPFLAGS :=

LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
	libwrs_omxil_common \
	liblog

LOCAL_C_INCLUDES := \
	$(WRS_OMXIL_CORE_ROOT)/utils/inc \
	$(WRS_OMXIL_CORE_ROOT)/base/inc \
	$(WRS_OMXIL_CORE_ROOT)/core/inc/khronos/openmax/include \
	$(PV_INCLUDES)

include $(BUILD_SHARED_LIBRARY)
