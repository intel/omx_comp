ifeq ($(strip $(BOARD_USES_MRST_OMX)),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

VENDORS_INTEL_MRST_COMPONENTS_ROOT := $(LOCAL_PATH)

COMPONENT_SUPPORT_BUFFER_SHARING := false
COMPONENT_SUPPORT_OPENCORE := false
COMPONENT_SUPPORT_USRPTR_SHARING := false
FAKE_USRPTR_SHARING := false

PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/wrs_omxil_components.list:system/etc/wrs_omxil_components.list
#$(call add-prebuilt-files, ETC, wrs_omxil_components.list)

WRS_OMXIL_CORE_ROOT := hardware/intel/wrs_omxil_core

GLIB_TOP := hardware/intel/glib
LIBVA_TOP := hardware/intel/libva
LIBINFODUMP_TOP := hardware/intel/omx-components/libinfodump

ifeq ($(strip $(COMPONENT_SUPPORT_OPENCORE)), true)
PV_TOP := external/opencore
PV_INCLUDES := \
	$(PV_TOP)/android \
	$(PV_TOP)/extern_libs_v2/khronos/openmax/include \
	$(PV_TOP)/engines/common/include \
	$(PV_TOP)/engines/player/config/core \
	$(PV_TOP)/engines/player/include \
	$(PV_TOP)/nodes/pvmediaoutputnode/include \
	$(PV_TOP)/nodes/pvdownloadmanagernode/config/opencore \
	$(PV_TOP)/pvmi/pvmf/include \
	$(PV_TOP)/fileformats/mp4/parser/config/opencore \
	$(PV_TOP)/oscl/oscl/config/android \
	$(PV_TOP)/oscl/oscl/config/shared \
	$(PV_TOP)/engines/author/include \
	$(PV_TOP)/android/drm/oma1/src \
	$(PV_TOP)/build_config/opencore_dynamic \
	$(TARGET_OUT_HEADERS)/libpv
endif
# mrst sst audio
#-include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/sst/Android.mk

#dump test
#include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/libinfodump/Android.mk
#intel video codecs
include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/psb-dec/Android.mk
include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/avc-enc/Android.mk
include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/h263-enc/Android.mk
include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/m4v-enc/Android.mk

#intel audio codecs
#-include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/sst-stub-base/Android.mk

endif #BOARD_USES_MRST_OMX
