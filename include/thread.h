#ifndef _THREAD_H_
#define _THREAD_H_

#include <sys/types.h>
#include <signal.h> 

#include "include/common.h"
#include "include/context.h"

// 线程对象
struct thread {
    pid_t pid;    // 线程id
    int code;  // 线程状态码，0正在运行
    int status; // 传递给进程的信号+事件，高8位是事件，低8位是信号
    struct list_head context_chain;    // 该线程的调用上下文呢
    struct list_head thread_chain; // 所有线程
};

// 获取当前线程的pc寄存器值
addr_t arch_get_thread_pc(struct thread *t);
// 设置当前线程的pc
int arch_set_thread_pc(struct thread *t, addr_t pc);
// 获取当前调用的返回地址
addr_t arch_get_return_pc(struct thread *t);

// 新增一个线程
struct thread *thread_add(pid_t pid, int code, int status);

// 通过pid查找线程
struct thread *thread_get_by_pid(pid_t pid);

// 删除一个线程
void thread_delete(struct thread *t);

// 线程进入stop模式
int thread_stop(struct thread *t);
int thread_all_stop(void);

// 线程恢复运行模式
int thread_run(struct thread *t);
int thread_all_run(void);
// 单个线程进入到单步模式
int thread_one_singlestep(struct thread *t);
// 等待线程状态变化
int thread_wait(struct thread *t, int option);

#endif