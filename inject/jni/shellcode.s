.global _dlopen_addr_s             @全局变量_dlopen_addr_s保存dlopen函数的调用地址
.global _dlopen_param1_s           @全局变量_dlopen_param1_s保存函数dlopen的第一个参数-加载库文件的路径
.global _dlopen_param2_s           @全局变量_dlopen_param2_s保存函数dlopen的第二个参数-库文件的打开模式

.global _dlsym_addr_s              @全局变量_dlsym_addr_s保存函数dlsym的调用地址  
.global _dlsym_param2_s            @全局变量_dlsym_param2_s保存函数dlsym的第二个参数-获取调用地址的函数的名称

.global _dlclose_addr_s            @全局变量_dlclose_addr_s保存函数dlclose的调用地址

.global _inject_start_s            @全局变量_inject_start_s保存注入代码的起始地址
.global _inject_end_s              @全局变量_inject_end_s保存注入代码的结束地址

.global _inject_function_param_s   @全局变量_inject_function_param_s保存Hook函数的参数

.global _saved_cpsr_s              @全局变量_saved_cpsr_s保存当前程序状态寄存器CPSR的值
.global _saved_r0_pc_s             @全局变量_saved_r0_pc_s保存寄存器环境R0-R15(PC)的值起始地址

@定义数据段.data 
.data                              

@注入代码的起始地址
_inject_start_s:
	@ debug loop
3:
	@sub r1, r1, #0
	@B 3b

	@调用dlopen函数
	ldr r1, _dlopen_param2_s    @库文件的打开模式
	ldr r0, _dlopen_param1_s    @加载库文件的路径字符串即Hook函数所在的模块
	ldr r3, _dlopen_addr_s      @dlopen函数的调用地址
	blx r3                      @调用函数dlopen加载并打开动态库文件
	subs r4, r0, #0             @判断函数返回值r0-是否打开动态库文件成功
	beq	2f                  @打开动态库文件失败跳转标签2的地方执行
				    @r0保存加载库的引用pHandle
				    
	@调用dlsym函数
	ldr r1, _dlsym_param2_s     @获取调用的地址的函数名称字符串 
	ldr r3, _dlsym_addr_s       @dlsym函数的调用地址
	blx r3                      @调用函数dlsym获取目标函数的调用地址
	subs r3, r0, #0             @判断函数的返回值r0
	beq 1f                      @不成功跳转到标签1的地方执行
                                    @r3保存获取到的函数的调用地址

	@调用Hook_Api函数
	ldr r0, _inject_function_param_s   @给Hook函数传入参数r0
	blx r3                             @调用Hook函数Hook远程目标进程的某系统调用函数
	subs r0, r0, #0                    @判断函数的返回值r0
	beq 2f                             @r0=0跳转到标签2的地方执行 ??

1:
	@调用dlclose函数
	mov r0, r4                         @参数r0动态库的应用
	ldr r3, _dlclose_addr_s            @赋值r3为dlclose函数的调用地址
	blx r3                             @调用dlclose函数关闭库文件的引用pHandle          

2:
	@恢复目标进程的原来状态
	ldr r1, _saved_cpsr_s
	msr cpsr_cf, r1                    @恢复目标进程寄存器CPSR的值

	ldr sp, _saved_r0_pc_s        
	ldmfd sp, {r0-pc}                  @恢复目标进程寄存器环境R0-R15(PC)的值且sp不改变   

_dlopen_addr_s:
.word 0x11111111                           @初始化word型全局变量_dlopen_addr_s

_dlopen_param1_s:
.word 0x11111111                           @初始化word型全局变量_dlopen_param1_s

_dlopen_param2_s:                          @初始化word型全局变量_dlopen_param2_s = 0x2
.word 0x2

_dlsym_addr_s:
.word 0x11111111                           @初始化word型全局变量_dlsym_addr_s

_dlsym_param2_s:
.word 0x11111111                           @初始化word型全局变量_dlsym_param2_s

_dlclose_addr_s:                          
.word 0x11111111                           @初始化word型全局变量_dlclose_addr_s

_inject_function_param_s: 
.word 0x11111111                           @初始化word型全局变量_inject_function_param_s

_saved_cpsr_s:
.word 0x11111111                           @初始化word型全局变量_saved_cpsr_s

_saved_r0_pc_s:
.word 0x11111111                           @初始化word型全局变量_saved_r0_pc_s

@注入代码的结束地址
_inject_end_s:                             

.space 0x400, 0                            @申请的代码段内存空间大小

@数据段.data的结束位置 
.end