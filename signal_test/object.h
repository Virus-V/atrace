#ifndef _OBJECT_H_
#define _OBJECT_H_

#include "common.h"
#include "list.h"
#include <elf.h>
#include <pthread.h>

// 二进制对象
struct object {
  const char *file_name;       // 文件名
  addr_t text_start, text_end; // 代码段的开始和结束地址

  uint_least8_t text_attr; // 代码段的属性,一般是rxp

  struct list_head object_chain; // 所有object

  struct list_head breakpoint_chain; // 该二进制文件对应的断点
  pthread_mutex_t bpc_lock;          // 这个锁保护breakpoint_chain, attr
};
typedef struct object object_t;

/**
 * Object属性标志
 * private, execute, write, read代表当前object代码段的属性
 * no_bkpt: 当前区域无法放置触发器,否则会导致运行异常
 **/
#define OBJ_ATTR(F)                                                            \
  F(PRIVATE)                                                                   \
  F(EXECUTE)                                                                   \
  F(WRITE)                                                                     \
  F(READ)                                                                      \
  F(NO_BKPT)

#define OBJ_ATTR_OFFSET(name) OBJ_ATTR_OFFSET_##name,
enum __object_attr_offset { OBJ_ATTR(OBJ_ATTR_OFFSET) OBJ_ATTR_OFFSET_NR };
#undef OBJ_ATTR_OFFSET

#define OBJ_ATTR_FLAG(name)                                                    \
  OBJ_ATTR_FLAG_##name = 0x1u << OBJ_ATTR_OFFSET_##name,
enum __object_attr_flag { OBJ_ATTR(OBJ_ATTR_FLAG) };
#undef OBJ_ATTR_FLAG

#define OBJ_ATTR_FUN(name)                                                     \
  static inline void object_attr_set_##name(object_t *obj) {                   \
    obj->text_attr |= OBJ_ATTR_FLAG_##name;                                    \
  }                                                                            \
  static inline void object_attr_clr_##name(object_t *obj) {                   \
    obj->text_attr &= ~OBJ_ATTR_FLAG_##name;                                   \
  }                                                                            \
  static inline int object_attr_flag_##name(object_t *obj) {                   \
    return !!(obj->text_attr & OBJ_ATTR_FLAG_##name);                          \
  }

OBJ_ATTR(OBJ_ATTR_FUN)
#undef OBJ_ATTR_FUN

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