LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

VENDORS_INTEL_MRST_COMPONENTS_ROOT := $(LOCAL_PATH)

$(call add-prebuilt-files, ETC, wrs_omxil_components.list)

WRS_OMXIL_CORE_ROOT := vendor/wrs/addon/libs/wrs-omxil-core

GLIB_TOP := external/glib

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

# mrst sst audio
-include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/sst/Android.mk

# poulsbo
-include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/psb/Android.mk
