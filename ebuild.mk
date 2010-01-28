#
# Copyright (c) 2009-2010 Wind River Systems, Inc.
#
# The right to copy, distribute, modify, or otherwise make use
# of this software may be licensed only pursuant to the terms
# of an applicable Wind River license agreement.
#

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

VENDORS_INTEL_MRST_COMPONENTS_ROOT := $(LOCAL_PATH)

# mrst sst audio
-include $(VENDORS_INTEL_MRST_COMPONENTS_ROOT)/sst/ebuild.mk
