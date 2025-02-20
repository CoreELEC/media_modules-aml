LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := esplayer
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_MODULE_TAGS := optional
#LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := \
	esplayer.c \
	vcodec.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../amcodec/include \
	$(JNI_H_INCLUDE)

LOCAL_LDLIBS := -llog
LOCAL_VENDOR_MODULE := true
LOCAL_CFLAGS += -Wno-unused-label -Wno-unused-parameter -Wno-format -Wno-switch
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_MODULE    := dec_slt
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_MODULE_TAGS := optional
#LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := \
	dec_slt.c \
	vcodec.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../amcodec/include \
	$(JNI_H_INCLUDE)

LOCAL_LDLIBS := -llog
LOCAL_VENDOR_MODULE := true
LOCAL_CFLAGS += -Wno-unused-label -Wno-unused-parameter -Wno-format -Wno-switch
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_MODULE    := DecInfo_test
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_MODULE_TAGS := optional
#LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := \
	DecInfo_test.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../amcodec/include \
	$(JNI_H_INCLUDE)

LOCAL_LDLIBS := -llog
LOCAL_VENDOR_MODULE := true
LOCAL_CFLAGS += -Wno-unused-label -Wno-unused-parameter -Wno-format -Wno-switch
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE    := vdec_debug
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_MODULE_TAGS := optional
#LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := \
	vdec_debug_port.c

LOCAL_LDLIBS := -llog
LOCAL_VENDOR_MODULE := true
LOCAL_CFLAGS += -Wno-unused-label -Wno-unused-parameter -Wno-format -Wno-switch
include $(BUILD_EXECUTABLE)

