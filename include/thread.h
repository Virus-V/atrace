#ifndef _THREAD_H_
#define _THREAD_H_

#include "include/list.h"
#include "include/common.h"
#include "include/context.h"

// 线程对象
struct thread {
    int tid;    // 线程id
    int state;  // 线程状态
    list_head context_chain;    // 该线程的调用上下文呢
    list_head thread_chain; // 所有线程
};

// 新增一个线程
int thread_add();
// 删除一个线程
int thread_delete();
// 所有线程进入stop模式
int thread_all_stop();
// 所有线程恢复运行模式
int thread_all_run();
// 单个线程进入到单步模式
int thread_one_singlestep();

#endif