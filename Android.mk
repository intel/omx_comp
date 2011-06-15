ifeq ($(strip $(BOARD_USES_MRST_OMX)),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

VENDORS_INTEL_MRST_COMPONENTS_ROOT := $(LOCAL_PATH)

PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/wrs_omxil_components.list:system/etc/wrs_omxil_components.list
#$(call add-prebuilt-files, ETC, wrs_omxil_components.list)

WRS_OMXIL_CORE_ROOT := hardware/intel/wrs_omxil_core

LIBVA_TOP := hardware/intel/libva

# mrst sst audio
#-include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/sst/Android.mk

#intel video codecs
include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/psb-dec/Android.mk
include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/avc-enc/Android.mk
include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/h263-enc/Android.mk
include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/m4v-enc/Android.mk
include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/videocodec/Android.mk

#intel audio codecs
#-include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/sst-stub-base/Android.mk

endif #BOARD_USES_MRST_OMX
