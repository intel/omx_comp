LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

VENDORS_INTEL_MRST_COMPONENTS_ROOT := $(LOCAL_PATH)

# mrst sst audio
-include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/sst/ebuild.mk
