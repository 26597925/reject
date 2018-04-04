LOCAL_PATH := $(call my-dir)    
    
#  
#注入程序LibInject  
#  
  
#清除变量  
include $(CLEAR_VARS)    
  
#生成的模块的名称LibInject  
LOCAL_MODULE := LibInject     
  
#需要编译的源码并包含注入的shellcode代码  
LOCAL_SRC_FILES := inject.c shellcode.s    
  
#使用Android的Log日志系统  
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog

LOCAL_SHARED_LIBRARIES += liblog
    
#LOCAL_FORCE_STATIC_EXECUTABLE := true    
  
#编译生成可执行程序  
include $(BUILD_EXECUTABLE)    