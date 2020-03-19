#ifndef _THREAD_H_
#define _THREAD_H_

#include <stdlib.h>
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
    // context stack
    struct list_head thread_list_entry;
} thread_t;

// 初始化线程map
void thread_map_init(void);

void thread_init(thread_t *thread);

// 增加线程map对象
int thread_map_add(thread_t *thread);
// 删除线程对象
thread_t *thread_map_del(pid_t thread_id);
// 查找线程对象
thread_t *thread_map_find(pid_t thread_id);

#endif
