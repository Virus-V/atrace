#ifndef _THREAD_H_
#define _THREAD_H_

#include <sys/types.h>

#include "include/common.h"
#include "include/context.h"

// 线程对象
struct thread {
    pid_t pid;    // 线程id
    int state;  // 线程状态
    struct list_head context_chain;    // 该线程的调用上下文呢
    struct list_head thread_chain; // 所有线程
};

// 新增一个线程
struct thread *thread_add(pid_t pid, int status);
// 删除一个线程
void thread_delete(pid_t pid);

// 所有线程进入stop模式
int thread_all_stop(void);
// 所有线程恢复运行模式
int thread_all_run(void);
// 单个线程进入到单步模式
int thread_one_singlestep(struct thread *t);
// 等待线程状态变化
struct thread *thread_wait(pid_t pid);

#endif