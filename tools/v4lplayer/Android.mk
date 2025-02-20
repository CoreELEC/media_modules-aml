LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := v4lplayer
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_MODULE_TAGS := optional
#LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := \
	vcodec_utils.c \
	v4l2_dec.c \
	aml_uvm.c \
	v4lplayer.c

LOCAL_VENDOR_MODULE := true
LOCAL_CFLAGS += -Wno-unused-label -Wno-unused-parameter -Wno-format -Wno-switch
include $(BUILD_EXECUTABLE)

