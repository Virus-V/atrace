#ifndef _COMMON_H_
#define _COMMON_H_

#include <assert.h>
#include <stdint.h>

#undef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE, MEMBER) __compiler_offsetof(TYPE, MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((size_t) & ((TYPE *)0)->MEMBER)
#endif

#define container_of(ptr, type, member)                                        \
  ({                                                                           \
    const typeof(((type *)0)->member) *__mptr = (ptr);                         \
    (type *)((char *)__mptr - offsetof(type, member));                         \
  })

// 页面大小
#define SLOT_PAGE_SIZE 4096
// 指令Slot占用的页个数
#define SLOT_PAGE_NUM 2
// slot大小
#define SLOT_SIZE 8

// 表示一个地址
typedef uintptr_t addr_t;
// 储存一个指令
typedef uint32_t instr_t;
// 原子计数
typedef int atomic_cnt_t;

#endif
