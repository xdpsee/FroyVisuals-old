LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../../../common \
    $(LOCAL_PATH)/../../../include \
    $(LOCAL_PATH)/../../../src\
    $(LOCAL_PATH)/../../../evaluator
LOCAL_MODULE	:= actor_avs_superscope
LOCAL_SRC_FILES := actor_avs_superscope.c
LOCAL_CFLAGS	:= -Wall -O0 -g
LOCAL_SHARED_LIBRARIES := libvisual common actor_avs
LOCAL_STATIC_LIBRARIES := evaluator
include $(BUILD_SHARED_LIBRARY)

