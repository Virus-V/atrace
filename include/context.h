#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include "include/common.h"
#include "include/object.h"

// 上下文
struct context {
    struct breakpoint *bkpts[2];    // 进入点,退出点
    uint64_t utime_start, utime_end; // 用户时间
    uint64_t stime_start, stime_end; // 内核时间
    struct list_head context_chain; // 上下文链表
};

// 进入上下文, 参数:进入点,自动从当前上下文信息中计算出退出点,并打断点
int context_enter();
// 退出上下文
int context_exit();

#endif