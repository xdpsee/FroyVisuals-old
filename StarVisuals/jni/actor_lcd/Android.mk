LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := ./include
LOCAL_MODULE	:= actor_lcd
LOCAL_SRC_FILES := actor_lcd.c
LOCAL_CFLAGS += $(WARNING_FLAGS)
LOCAL_CFLAGS += $(DEBUG_FLAGS)
LOCAL_CFLAGS += $(OPTIM_FLAGS)
LOCAL_SHARED_LIBRARIES := libvisual visscript-lua
#include $(BUILD_SHARED_LIBRARY)

