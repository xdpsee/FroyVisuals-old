LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE	:= morph_flash
LOCAL_SRC_FILES := morph_flash.c
LOCAL_CFLAGS	:= -Wall -O0 -g -Wstrict-aliasing -Wcast-align -Wpointer-arith -Waddress
LOCAL_STATIC_LIBRARIES := libvisual
LOCAL_C_INCLUDES := ./include
include $(BUILD_SHARED_LIBRARY)


