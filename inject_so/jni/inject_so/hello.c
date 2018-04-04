#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <android/log.h>
#include <elf.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <cassert>
#include <stdarg.h>
#include "jni.h"
#include "inlineHook.h"

#define LOG_TAG "DEBUG"
#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##args)

struct soinfo {
	char name[128];
	const Elf32_Phdr* phdr;
	size_t phnum;
	Elf32_Addr entry;
	Elf32_Addr base;
	unsigned size;

	uint32_t unused1;  // DO NOT USE, maintained for compatibility.

	Elf32_Dyn* dynamic;

	uint32_t unused2; // DO NOT USE, maintained for compatibility
	uint32_t unused3; // DO NOT USE, maintained for compatibility

	struct _soinfo* next;
	unsigned flags;

	const char* strtab;
	Elf32_Sym* symtab;

	size_t nbucket;
	size_t nchain;
	unsigned* bucket;
	unsigned* chain;

	unsigned* plt_got;

	Elf32_Rel* plt_rel;
	size_t plt_rel_count;

	Elf32_Rel* rel;
	size_t rel_count;
};

static unsigned elfhash(const char *symbol_name)
{
	const unsigned char *name = (const unsigned char *) symbol_name;
	unsigned h = 0, g;

	while(*name) {
		h = (h << 4) + *name++;
		g = h & 0xf0000000;
		h ^= g;
		h ^= g >> 24;
	}
	return h;
}

static uint32_t findSymbolAddr(struct soinfo *si, const char *symbol_name)
{
	Elf32_Sym *symtab;
	const char *strtab;
	size_t i;

	symtab = si->symtab;
	strtab = si->strtab;
	
	int n = 1;
	unsigned hash = elfhash(symbol_name);
	for (i = si->bucket[hash % si->nbucket]; i != 0; i = si->chain[i]) {
		Elf32_Sym* s = symtab + i;
		n++;
		if (strcmp(strtab + s->st_name, symbol_name) == 0 && ELF32_ST_TYPE(s->st_info) == STT_FUNC) {
			return (s->st_value + si->base);
		}
	}

	/*
	for (i = 0; i < si->nchain; ++i) {
		if (strcmp((char *) (strtab + symtab[i].st_name), symbol_name) == 0 && ELF32_ST_TYPE(symtab[i].st_info) == STT_FUNC) {
			return (symtab[i].st_value + si->base);
		}
	}
	*/

	return 0;	
}

void* get_module_base(pid_t pid, const char* module_name)
{
    FILE *fp;
    long addr = 0;
    char *pch;
    char filename[32];
    char line[1024];

    if (pid < 0) {
        // self process
        snprintf(filename, sizeof(filename), "/proc/self/maps", pid);
    } else {
        snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
    }

    fp = fopen(filename, "r");

    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, module_name)) {
                pch = strtok( line, "-" );
                addr = strtoul( pch, NULL, 16 );

                if (addr == 0x8000)
                    addr = 0;

                break;
            }
        }

        fclose(fp) ;
    }

    return (void *)addr;
}

static char* jstringTostring(JNIEnv* env, jstring jstr)
{
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

static jstring stoJstring(JNIEnv* env, const char* str)
{
	    int strLen = strlen(str);
		jclass     jstrObj   = (*env)->FindClass(env, "java/lang/String");
		jmethodID  methodId  = (*env)->GetMethodID(env, jstrObj, "<init>", "([BLjava/lang/String;)V");
		jbyteArray byteArray = (*env)->NewByteArray(env, strLen);
		jstring    encode    = (*env)->NewStringUTF(env, "utf-8");
		(*env)->SetByteArrayRegion(env, byteArray, 0, strLen, (jbyte*)str);
		return (jstring)(*env)->NewObject(env, jstrObj, methodId, byteArray, encode);
}

int     *gnu_Unwind_CJKIIM;

char    *gnu_Unwind_AD;

signed int (*odlhook)(int a1, char *a2) __attribute__((fastcall)) = NULL;

signed int newhook(int a1, char *a2){
	LOGD("a1:%d,a2:%s\n", a1, a2);
	odlhook(a1, a2);
	return -1;
}

FILE * (*odlfopen)( char *filename, const char *modes) = NULL;

FILE * newfopen(char *filename, const char *modes){
	LOGD("filename:%s, modes:%s\n", filename, modes);
	//unHookSprintf();
	//sprintf(buffer, format);
	return odlfopen(filename, modes);
}
	
/*int (*old_puts)(char *) = NULL;

int new_puts(char *str)
{
    old_puts(str);
    LOGD("secauo, %s\n", str);
	return 1;
}*/

int hookCJKIIM(uint32_t target_addr)
{
	if (registerInlineHook(target_addr, (uint32_t) newhook, (uint32_t **) &odlhook) != ELE7EN_OK) {
        return -1;
    }
    if (inlineHook(target_addr) != ELE7EN_OK) {
        return -1;
    }
	
    return 0;
}

int unHookCJKIIM(uint32_t target_addr)
{
	if (inlineUnHook(target_addr) != ELE7EN_OK) {
        return -1;
    }
    return 0;
}

int hookFopen()
{
    if (registerInlineHook((uint32_t) fopen, (uint32_t) newfopen, (uint32_t **) &odlfopen) != ELE7EN_OK) {
        return -1;
    }
    if (inlineHook((uint32_t) fopen) != ELE7EN_OK) {
        return -1;
    }

    return 0;
}

int unHookFopen()
{
    if (inlineUnHook((uint32_t) fopen) != ELE7EN_OK) {
        return -1;
    }

    return 0;
}


static jstring native_test(JNIEnv* env,jobject thiz, jstring path){
	struct soinfo *si;
	
	char *tpath = jstringTostring(env, path);
	
	LOGD("tpath:%s\n", tpath);
	
	si = (struct soinfo *) dlopen(tpath, RTLD_NOW);
	if (si == NULL) {
		LOGD("dlopen %s failed\n", tpath);
		return -1;
	}
	char *symbol_name = "__gnu_Unwind_LLAHHDDDJJNMMXS_FDFDS";
	odlhook = dlsym(si,symbol_name);
	LOGD("odlhook:%p\n", odlhook);
	symbol_name = "__gnu_Unwind_CJKIIM";
	gnu_Unwind_CJKIIM = (int *)dlsym(si,symbol_name);
	LOGD("gnu_Unwind_CJKIIM:%p\n", gnu_Unwind_CJKIIM);
	*gnu_Unwind_CJKIIM = 1;
	symbol_name = "__gnu_Unwind_AD";
	gnu_Unwind_AD = (char*)dlsym(si,symbol_name);
	LOGD("gnu_Unwind_AD:%s\n", gnu_Unwind_AD);
	
	
	uint32_t target_addr = odlhook;//findSymbolAddr(si, symbol_name);
	hookCJKIIM(target_addr);
	
	hookFopen();
//	unHook(target_addr);
	
    return stoJstring(env, "");
}

static JNINativeMethod gMethods[] = {
    {"hookTest",	"(Ljava/lang/String;)Ljava/lang/String;",	(void *)native_test},
};
static const char* const kClassPathName = "com/vst/player/Test";

static int register_test(JNIEnv *env)
{
	jclass clazz = (*env)->FindClass(env, kClassPathName);
	if (clazz == NULL) {
		LOGD("Native registration unable to find class '%s'", kClassPathName);
		return JNI_FALSE;
	}
	if ((*env)->RegisterNatives(env, clazz, gMethods,  sizeof(gMethods) / sizeof(gMethods[0])) < 0) {
		LOGD("RegisterNatives failed for '%s'", kClassPathName);
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
           LOGD("ERROR: GetEnv failed");
           goto bail;
    }
    assert(env);

    if (register_test(env) < 0) {
    	LOGD("love native registration failed");
        goto bail;
    }

    /* success -- return valid version number */
    result = JNI_VERSION_1_6;

	bail:
    return result;
}