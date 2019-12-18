#ifndef _COMPILE_UNIT_H_
#define _COMPILE_UNIT_H_

#include "include/list.h"
#include "include/common.h"
#include "include/breakpoint.h"

// 二进制对象
struct object {
    int fd; // 打开的文件描述符
    addr_t text_start, text_end;    // 代码段的开始和结束地址
    struct list_head breakpoint_chain;  // 该二进制文件对应的断点
    struct list_head object_chain;
};

// 创建object
int object_add(const char *name);

#endif