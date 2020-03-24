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

#define __ACCESS_ONCE(x)                                                       \
  ({                                                                           \
    __maybe_unused typeof(x) __var = (__force typeof(x))0;                     \
    (volatile typeof(x) *)&(x);                                                \
  })
#define ACCESS_ONCE(x) (*__ACCESS_ONCE(x))

// 表示一个地址
typedef uintptr_t addr_t;
// 储存一个指令
typedef uint32_t instr_t;

#endif
