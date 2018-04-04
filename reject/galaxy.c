#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <android/log.h>
#include <elf.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/mman.h>

#include <stdarg.h>
#include "jni.h"
#include "core.h"

#define LOG_TAG "DEBUG"
#define LOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)

FILE *(*odlfopen)( char *filename, const char *modes) = NULL;
int (*oldopen)(const char * pathname, int flags, mode_t mode) = NULL;

char* replace(const char* src, const char* sub, const char* dst){
	int pos =0;
	int offset =0;
	int srcLen, subLen, dstLen;
	char*pRet = NULL;

	srcLen = strlen(src);
	subLen = strlen(sub);
	dstLen = strlen(dst);
	pRet = (char*)malloc(srcLen + dstLen - subLen +1);//(外部是否该空间)if (NULL != pRet)
	{
		pos = strstr(src, sub) - src;
		memcpy(pRet, src, pos);
		offset += pos;
		memcpy(pRet + offset, dst, dstLen);
		offset += dstLen;
		memcpy(pRet + offset, src + pos + subLen, srcLen - pos - subLen);
		offset += srcLen - pos - subLen;
		*(pRet + offset) ='\0';
	}
	return pRet;
}

static char* jstringTostring(JNIEnv* env, jstring jstr){
	char* rtn = NULL;
	jclass clsstring = (*env)->FindClass(env, "java/lang/String");
	jstring strencode = (*env)->NewStringUTF(env, "utf-8");
	jmethodID mid = (*env)->GetMethodID(env, clsstring, "getBytes", "(Ljava/lang/String;)[B");
	jbyteArray barr= (jbyteArray)(*env)->CallObjectMethod(env, jstr, mid, strencode);
	jsize alen = (*env)->GetArrayLength(env, barr);
	jbyte* ba = (*env)->GetByteArrayElements(env, barr, JNI_FALSE);
	if (alen > 0)
	{
		rtn = (char*)malloc(alen + 1);
		memcpy(rtn, ba, alen);
		rtn[alen] = 0;
	}
	(*env)->ReleaseByteArrayElements(env, barr, ba, 0);
	return rtn;
}

FILE * newfopen(char *filename, const char *modes){
	if(strstr(filename,"com.vst.player")){
		filename = replace(filename,"com.vst.player","com.video.mvideo");
	}
	return odlfopen(filename, modes);
}
int hookFopen() {
	LOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
	if (registerInlineHook((uint32_t) fopen, (uint32_t) newfopen, (uint32_t **) &odlfopen) != ELE7EN_OK) {
		return -1;
	}
	LOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
	if (inlineHook((uint32_t) fopen) != ELE7EN_OK) {
		return -1;
	}
	return 0;
}
int unHookFopen() {
	if (inlineUnHook((uint32_t) fopen) != ELE7EN_OK) {
		return -1;
	}
	return 0;
}

int newopen(const char * pathname, int flags, mode_t mode){
	if(strstr(pathname,"com.vst.player")){
		pathname = replace(pathname,"com.vst.player","com.video.mvideo");
	}
	return oldopen(pathname, flags, mode);
}
int hookOpen(){
	LOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
	if (registerInlineHook((uint32_t) open, (uint32_t) newopen, (uint32_t **) &oldopen) != ELE7EN_OK) {
		return -1;
	}
	LOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
	if (inlineHook((uint32_t) open) != ELE7EN_OK) {
		return -1;
	}
	return 0;
}
int unHookOpen(){
	if (inlineUnHook((uint32_t) open) != ELE7EN_OK) {
		return -1;
	}
	return 0;
}

static jstring native_hook(JNIEnv* env,jobject thiz){
	LOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
	hookFopen();
	LOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
	hookOpen();
	LOGE("call %s %s %d \n",__FILE__,__FUNCTION__,__LINE__);
    return (*env)->NewStringUTF(env, "bomb");
}

static jstring native_unhook(JNIEnv* env,jobject thiz){
	return (*env)->NewStringUTF(env, "end bomb");
}

static JNINativeMethod gMethods[] = {
    {"bomb",	"()Ljava/lang/String;",	(void *)native_hook},
	{"unbomb",	"()Ljava/lang/String;",	(void *)native_unhook},
};
//static const char* const kClassPathName = "com/vst/player/Galaxy";
static const char* const kClassPathName = "com/vst/so/parser/Galaxy";

static int register_test(JNIEnv *env)
{
	jclass clazz = (*env)->FindClass(env, kClassPathName);
	if (clazz == NULL) {
		LOGE("Native registration unable to find class '%s'", kClassPathName);
		return JNI_FALSE;
	}
	if ((*env)->RegisterNatives(env, clazz, gMethods,  sizeof(gMethods) / sizeof(gMethods[0])) < 0) {
		LOGE("RegisterNatives failed for '%s'", kClassPathName);
		(*env)->DeleteLocalRef(env, clazz);
		return JNI_FALSE;
	}
	(*env)->DeleteLocalRef(env, clazz);
	return JNI_TRUE;
}

jint JNI_OnLoad(JavaVM *vm, void* reserved)
{
	JNIEnv* env = NULL;
    jint result = -1;
	if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK)
	{
           LOGE("ERROR: GetEnv failed");
           goto bail;
    }


    if (register_test(env) < 0) {
    	LOGE("love native registration failed");
        goto bail;
    }

    /* success -- return valid version number */
    result = JNI_VERSION_1_6;

	bail:
    return result;
}