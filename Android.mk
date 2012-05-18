ifeq ($(strip $(USE_INTEL_OMX_COMPONENTS)),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := wrs_omxil_components.list
LOCAL_SRC_FILES := wrs_omxil_components.list
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
VENDORS_INTEL_MRST_COMPONENTS_ROOT := $(LOCAL_PATH)


WRS_OMXIL_CORE_ROOT := hardware/intel/wrs_omxil_core

LIBVA_TOP := hardware/intel/libva

# mrst sst audio
#-include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/sst/Android.mk

#intel video codecs
include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/videocodec/Android.mk

#intel audio codecs
#-include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/sst-stub-base/Android.mk

endif #BOARD_USES_MRST_OMX
