#
# Copyright (c) 2009-2010 Wind River Systems, Inc.
#
# The right to copy, distribute, modify, or otherwise make use
# of this software may be licensed only pursuant to the terms
# of an applicable Wind River license agreement.
#

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	psb.cpp

LOCAL_MODULE := libwrs_omxil_intel_mrst_psb

LOCAL_CPPFLAGS :=

LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
	libwrs_omxil_common

LOCAL_C_INCLUDES := \
	$(WRS_OMXIL_CORE_ROOT)/utils/inc \
	$(WRS_OMXIL_CORE_ROOT)/base/inc \
	$(WRS_OMXIL_CORE_ROOT)/core/inc/khronos/openmax/include

include $(BUILD_SHARED_LIBRARY)
