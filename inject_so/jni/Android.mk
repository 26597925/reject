LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := hello
LOCAL_SRC_FILES := hello.c

LOCAL_LDLIBS := -llog

#LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog -lEGL
LOCAL_SHARED_LIBRARIES += liblog libEGL

include $(BUILD_SHARED_LIBRARY)
#include $(BUILD_EXECUTABLE)
