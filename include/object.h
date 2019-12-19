#ifndef _COMPILE_UNIT_H_
#define _COMPILE_UNIT_H_

#include <elf.h>

#include "include/common.h"
#include "include/breakpoint.h"

// 区域属性
enum region_attr {
    REGN_ATTR_PRIVATE = 0x1 << 0,
    REGN_ATTR_EXECUTE = 0x1 << 1,
    REGN_ATTR_WRITE   = 0x1 << 2,
    REGN_ATTR_READ    = 0x1 << 3,
};

// 二进制对象
struct object {
    const char *file_name;  // 文件名
    addr_t text_start, text_end;    // 代码段的开始和结束地址
    struct list_head breakpoint_chain;  // 该二进制文件对应的断点
    struct list_head object_chain;
};

// 获取指定pid的可执行文件路径
char *object_get_exe(int pid);
// 获取可执行文件的入口点
addr_t object_get_exe_entry_point(const char *name);
// 加载进程的object
int object_load(int pid);
// 根据文件名查找object
struct object *object_get_by_file(const char *filename);

#endif