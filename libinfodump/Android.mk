ifeq ($(strip $(BOARD_USES_WRS_OMXIL_CORE)),true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	libinfodump.c

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libinfodump

LOCAL_CPPFLAGS :=

LOCAL_LDFLAGS :=

LOCAL_SHARED_LIBRARIES := \
	libutils

LOCAL_C_INCLUDES := .


include $(BUILD_STATIC_LIBRARY)
endif
