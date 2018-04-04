#include <string.h>
#include <stdint.h>
#include <jni.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <sys/limits.h>
#include <sys/poll.h>
#include <sys/stat.h>

#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/input.h>

#include <android/log.h>
#include <cassert>
#include "private/android_filesystem_config.h"
#define TAG "EventInjector::JNI"
#define LOGD(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG  , TAG, fmt, ##__VA_ARGS__) 
#define LOGV(fmt, ...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) __android_log_print(ANDROID_LOG_INFO, TAG, fmt, ##__VA_ARGS__)
#define LOGW(fmt, ...) __android_log_print(ANDROID_LOG_WARN, TAG, fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, TAG, fmt, ##__VA_ARGS__)

#include "EventInjector.h"

static struct typedev {
	struct pollfd ufds;
	char *device_path;
	char *device_name;
} *pDevs = NULL;
struct pollfd *ufds;
static int nDevsCount = 0;

const char *device_path = "/dev";

int g_Polling = 0;
int c;
int i;
int pollres;
int get_time = 0;
char *newline = "\n";
uint16_t get_switch = 0;
struct input_event event;
int version;

int dont_block = -1;
int event_count = 0;
int sync_rate = 0;
int64_t last_sync_time = 0;
const char *device = NULL; 
static char* join_str(char *s1, char *s2)
{  
    char *result = malloc(strlen(s1)+strlen(s2)+1); 
    if (result == NULL) return NULL;
	
    strcpy(result, s1);  
    strcat(result, s2);
	
    return result;  
}

static int open_device(int index)
{
	if (index >= nDevsCount || pDevs == NULL) return -1;
	LOGD("open_device prep to open");
	char *device = pDevs[index].device_path;
	
	LOGD("open_device call %s", device);
    int version;
    int fd;
    
    char name[80];
    char location[80];
    char idstr[80];
    struct input_id id;

	if(access(device, R_OK | W_OK) != 0) {
		if(setgid(AID_SHELL) ==0 && setuid(AID_SHELL) == 0){
			system("/system/xbin/su");
			char *shell = join_str("chmod 666 ", device);
			LOGD("%s, %s", shell, strerror(errno));
			system(shell);
		}
	}
	
    fd = open(device, O_RDWR);
    if(fd < 0) {
		pDevs[index].ufds.fd = -1;
		
		pDevs[index].device_name = NULL;
		LOGD("could not open %s, %s", device, strerror(errno));
        return -1;
    }
    
	pDevs[index].ufds.fd = fd;
	ufds[index].fd = fd;
	
    name[sizeof(name) - 1] = '\0';
    if(ioctl(fd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
        LOGD("could not get device name for %s, %s", device, strerror(errno));
        name[0] = '\0';
    }
	LOGD("Device %d: %s: %s", nDevsCount, device, name);
	
	pDevs[index].device_name = strdup(name);
    
    
    return 0;
}

int remove_device(int index)
{
	if (index >= nDevsCount || pDevs == NULL ) return -1;
	
	int count = nDevsCount - index - 1;
	LOGD("remove device %d", index);
	free(pDevs[index].device_path);
	free(pDevs[index].device_name);
	
	memmove(&pDevs[index], &pDevs[index+1], sizeof(pDevs[0]) * count);
	nDevsCount--;
	return 0;
}

static int scan_dir(const char *dirname)
{
    char devname[PATH_MAX];
    char *filename;
    DIR *dir;
    struct dirent *de;
	struct stat st;
    dir = opendir(dirname);
    if(dir == NULL)
        return -1;
    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    while((de = readdir(dir))) {
        if(de->d_name[0] == '.' &&
           (de->d_name[1] == '\0' ||
            (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;
        strcpy(filename, de->d_name);
		stat(devname, &st);
		if (S_ISDIR(st.st_mode)) {
			scan_dir(devname);
			continue;
		}
		LOGD("scan_dir:prepare to open:%s", devname);
		// add new filename to our structure: devname
		struct typedev *new_pDevs = realloc(pDevs, sizeof(pDevs[0]) * (nDevsCount + 1));
		if(new_pDevs == NULL) {
			LOGD("out of memory");
			return -1;
		}
		pDevs = new_pDevs;
		
		struct pollfd *new_ufds = realloc(ufds, sizeof(ufds[0]) * (nDevsCount + 1));
		if(new_ufds == NULL) {
			LOGD("out of memory");
			return -1;
		}
		ufds = new_ufds; 
		ufds[nDevsCount].events = POLLIN;
		
		pDevs[nDevsCount].ufds.events = POLLIN;
		pDevs[nDevsCount].device_path = strdup(devname);
		
        nDevsCount++;
    }
    closedir(dir);
    return 0;
}

static jint native_intSendEvent(JNIEnv* env,jobject thiz,
	jint devid, jint type, jint code, jint value){
	if (devid >= nDevsCount || pDevs[devid].ufds.fd == -1) return -1;
	int fd = pDevs[devid].ufds.fd;
	LOGD("SendEvent call (%d,%d,%d,%d)", fd, type, code, value);
	struct uinput_event event;
	int len;

	if (fd <= fileno(stderr)) return -1;

	memset(&event, 0, sizeof(event));
	event.type = type;
	event.code = code;
	event.value = value;

	len = write(fd, &event, sizeof(event));
	LOGD("SendEvent done:%d",len);
	return 1;
}

static jint native_ScanFiles(JNIEnv* env,jobject thiz){
	int res = scan_dir(device_path);
	if(res < 0) {
		LOGD("scan dir failed for %s:", device_path);
		return -1;
	}
	
	return nDevsCount;
}

static jstring native_getDevPath(JNIEnv* env,jobject thiz,jint devid){
	return (*env)->NewStringUTF(env, pDevs[devid].device_path);
}

static jstring native_getDevName(JNIEnv* env,jobject thiz,jint devid){
	if (pDevs[devid].device_name == NULL) return NULL;
	else return (*env)->NewStringUTF(env, pDevs[devid].device_name);
}

static jint native_OpenDev(JNIEnv* env,jobject thiz,jint devid){
	return open_device(devid);
}

static jint native_RemoveDev(JNIEnv* env,jobject thiz,jint devid){
	return remove_device(devid);
}

static jint native_PollDev(JNIEnv* env,jobject thiz,jint devid){
	if (devid >= nDevsCount || pDevs[devid].ufds.fd == -1) return -1;
	int pollres = poll(ufds, nDevsCount, -1);
	if(ufds[devid].revents) {
		if(ufds[devid].revents & POLLIN) {
			int res = read(ufds[devid].fd, &event, sizeof(event));
			if(res < (int)sizeof(event)) {
				return 1;
			} 
			else return 0;
		}
	}
	return -1;
}

static jint native_getType( JNIEnv* env,jobject thiz ) {
	return event.type;
}

static jint native_getCode( JNIEnv* env,jobject thiz ) {
	return event.code;
}

static jint native_getValue( JNIEnv* env,jobject thiz ) {
	return event.value;
}

static JNINativeMethod gMethods[] = {
	{"intSendEvent",	"(IIII)I",	(void *)native_intSendEvent},
	{"ScanFiles",	"()I",	(void *)native_ScanFiles},
	{"getDevPath",	"(I)Ljava/lang/String;",	(void *)native_getDevPath},
	{"getDevName",	"(I)Ljava/lang/String;",	(void *)native_getDevName},
	{"OpenDev",	"(I)I",	(void *)native_OpenDev},
	{"RemoveDev",	"(I)I",	(void *)native_RemoveDev},
	{"PollDev",	"(I)I",	(void *)native_PollDev},
	{"getType",	"()I",	(void *)native_getType},
	{"getCode",	"()I",	(void *)native_getCode},
	{"getValue", "()I",	(void *)native_getValue},
};

//static const char* const kClassPathName = "net/pocketmagic/android/eventinjector/Events";
static const char* const kClassPathName = "com/mylove/removeserver/event/Events";

static int register_remote(JNIEnv *env)
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
    assert(env);

    if (register_remote(env) < 0) {
    	LOGE("love native registration failed");
        goto bail;
    }

    /* success -- return valid version number */
    result = JNI_VERSION_1_6;

bail:
    return result;
}