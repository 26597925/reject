#pragma once

#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

//远程进程注入
int inject_remote_process(pid_t target_pid, const char *library_path, const char *function_name, void *param, size_t param_size);

//根据进程的名字获取进程的PID
int find_pid_of(const char *process_name);

//获取进程加载的模块的基址
void* get_module_base(pid_t pid, const char* module_name);

#ifdef __cplusplus
}
#endif

//进程注入的参数-根据Hook的函数需要自定义该结构体
struct inject_param_t
{
	//进程的PID
	pid_t from_pid;
} ;
