LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := ./include
LOCAL_MODULE	:= actor_avs
LOCAL_SRC_FILES := actor_AVS.c
LOCAL_CFLAGS    += $(ARCH_CFLAGS)
LOCAL_SHARED_LIBRARIES := libvisual common
LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../common/\
	$(LOCAL_PATH)/../include/
include $(BUILD_SHARED_LIBRARY)


