#ifndef _OBJECT_H_
#define _OBJECT_H_

#include <elf.h>
#include <pthread.h>
#include "common.h"
#include "list.h"

// 区域属性
enum region_attr {
    REGN_ATTR_PRIVATE = 0x1 << 0,
    REGN_ATTR_EXECUTE = 0x1 << 1,
    REGN_ATTR_WRITE   = 0x1 << 2,
    REGN_ATTR_READ    = 0x1 << 3,

    // 当前区域无法放置触发器,否则会导致运行异常
    REGN_ATTR_NO_BKPT = 0x1 << 4,
};

// 二进制对象
struct object {
    const char *file_name;  // 文件名
    addr_t text_start, text_end;    // 代码段的开始和结束地址

    uint_least8_t text_mode_; // 代码段的属性,一般是rxp

    struct list_head object_chain; // 所有object

    struct list_head breakpoint_chain;  // 该二进制文件对应的断点
    pthread_mutex_t bpc_lock;  // 这个锁保护breakpoint_chain_
};
typedef struct object object_t;

// 创建object对象
object_t *object_new(void);
void object_del(object_t *obj);

// 获取当前的可执行文件路径
char *object_get_self_exe(void);

// 获取可执行文件的入口点
addr_t object_get_exe_entrypoint(const char *name);
addr_t object_get_self_entrypoint(void);

// 加载进程的object
void object_load(void);
void object_unload(void);

// 根据文件名查找object
object_t *object_get_by_file(const char *filename);
// 通过address查找object
object_t *object_get_by_address(addr_t addr);

// 锁定和解锁object对应的代码段区域
int object_memory_lock(object_t *obj);
int object_memory_unlock(object_t *obj);

#endif