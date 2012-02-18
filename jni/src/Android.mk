LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

LOCAL_C_INCLUDES := \
    ./include

# Add any compilation flags for your project here...
LOCAL_CFLAGS := \
	-DPLAY_MOD -g -O0 -Wstrict-aliasing -Wcast-align -Waddress

# Add your application source files here...
LOCAL_SRC_FILES := froyvisuals.c

LOCAL_SHARED_LIBRARIES := libvisual

LOCAL_LDLIBS := -lGLESv1_CM -lm -llog -ljnigraphics

include $(BUILD_SHARED_LIBRARY)

