#ifndef _THREAD_H_
#define _THREAD_H_

#include <stdlib.h>
#include "common.h"
#include "list.h"

/**
 * 线程hash map大小
 **/
#define THREAD_MAP_SIZE 64

/**
 * 线程对象
 **/
typedef struct thread {
    pid_t tid;
    // 当前激活的断点
    breakpoint_t *active_bp;
    // context stack
    struct list_head thread_list_entry_;
    // 存放被断点指令，因为是在用户态，无法执行单步运行，所以
    // 需要构造一段指令和栈，执行单个指令然后return，返回地址由栈
    // 指定。栈复用当前线程
    void *code_cache_;
    long pg_size_;  // 页大小
} thread_t;

// 初始化线程map
void thread_map_init(void);

thread_t *thread_new(void);
void thread_del(thread_t *thread);

// 增加线程map对象
int thread_map_add(thread_t *thread);
// 删除线程对象
thread_t *thread_map_del(pid_t thread_id);
// 查找线程对象
thread_t *thread_map_find(pid_t thread_id);
// 线程绑定一个breakpoint
void thread_active_breakpoint(thread_t *thread);

#endif
