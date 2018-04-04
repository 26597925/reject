LOCAL_PATH := $(call my-dir)

cmd-strip = $(TOOLCHAIN_PREFIX)strip --strip-debug -x $1

include $(CLEAR_VARS)

LOCAL_MODULE    := libtest

LOCAL_CFLAGS := -DANDROID_NDK \
	-DDISABLE_IMPORTGL \
	-fvisibility=hidden

LOCAL_SRC_FILES := hello.c inlineHook.c relocate.c

LOCAL_SHARED_LIBRARIES += liblog libdl

include $(BUILD_SHARED_LIBRARY)