LOCAL_PATH := $(call my-dir)    
    
#  
#ע�����LibInject  
#  
  
#�������  
include $(CLEAR_VARS)    
  
#���ɵ�ģ�������LibInject  
LOCAL_MODULE := LibInject     
  
#��Ҫ�����Դ�벢����ע���shellcode����  
LOCAL_SRC_FILES := inject.c shellcode.s    
  
#ʹ��Android��Log��־ϵͳ  
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog

LOCAL_SHARED_LIBRARIES += liblog
    
#LOCAL_FORCE_STATIC_EXECUTABLE := true    
  
#�������ɿ�ִ�г���  
include $(BUILD_EXECUTABLE)    