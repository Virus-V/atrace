#ifndef _COMPILE_UNIT_H_
#define _COMPILE_UNIT_H_

#include <elf.h>

#include "include/common.h"
#include "include/breakpoint.h"

// 二进制对象
struct object {
    int fd; // 打开的文件描述符
    const char *file_name;  // 文件名
    addr_t text_start, text_end;    // 代码段的开始和结束地址
    struct list_head breakpoint_chain;  // 该二进制文件对应的断点
    struct list_head object_chain;
};

// 获取指定pid的可执行文件路径
char *object_get_exe(int pid);
// 获取可执行文件的入口点
addr_t object_get_exe_entry_point(const char *name);

int object_init(int pid);

#endif