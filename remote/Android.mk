LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_LDLIBS := -llog
LOCAL_MODULE    := libEventInjector
LOCAL_SRC_FILES := EventInjector.c
LOCAL_SHARED_LIBRARIES += liblog
include $(BUILD_SHARED_LIBRARY)