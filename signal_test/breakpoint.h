#ifndef _BREAK_POINT_H_
#define _BREAK_POINT_H_

#include "common.h"
#include "list.h"
#include "rbtree.h"

typedef struct object object_t;

// 每个集合中有多少个指令槽
#define SLOT_NUM ((SLOT_PAGE_NUM * SLOT_PAGE_SIZE) / SLOT_SIZE)
// unsigned int 有多少位
#define UINT_BIT_NUM (sizeof(unsigned int) << 3)
// 集合中代表每个slot状态的hash表个数
#define SLOT_MAP_LEN ((SLOT_NUM + UINT_BIT_NUM - 1) / UINT_BIT_NUM)

// 指令槽集合
struct slot_set {
  unsigned int free_slot[SLOT_MAP_LEN]; // 1代表空闲
  int used_slot_count;
  uintptr_t slots;
  pthread_mutex_t lock;
  struct list_head slot_set_list;
};
typedef struct slot_set slot_set_t;

// 初始化slot set
void slot_set_init(void);
// 增加一个slot_set
void slot_set_add(void);
// 释放一个slot_set
int slot_set_remove(slot_set_t *slot);

// 分配一个slot
void slot_alloc(slot_set_t **slot, unsigned int *index);

// 释放指定索引的slot
void slot_free(slot_set_t *slot, unsigned int index);

// 刷新相关slot的icache
void slot_invalid_cache(slot_set_t *slot, unsigned int index);

/**
 * 断点其实是一个事件触发器,动态的插在程序中
 * 程序执行流执行到断点时,会触发该断点绑定的相应事件
 *  __builtin_ffs
 **/
struct breakpoint {
  addr_t address;                 // 断点位置
  uint32_t attr;                  // 断点属性
  struct rb_node breakpoint_tree; // 通过红黑树找到该对象
  // XXX 是否要增加锁?
};
typedef struct breakpoint breakpoint_t;

typedef struct breakpoint_return breakpoint_return_t;

struct breakpoint_normal {
  // breakpoint header
  breakpoint_t header_;

  instr_t instruction; // 断点位置原来的指令
  unsigned int instr_length; // 指令长度,x86下需要, ARM和AARCH64不需要该字段

  // 指令slot的索引
  slot_set_t *slot_set;
  unsigned int slot_index;

  object_t *obj;                     // 该断点所处的object
  breakpoint_return_t *bp_return;    // trace模式下的函数返回hook
  struct list_head breakpoint_chain; // 通过object找到该对象
};
typedef struct breakpoint_normal breakpoint_normal_t;

// 函数trace的返回断点
struct breakpoint_return {
  // breakpoint header
  breakpoint_t header_;

  breakpoint_t *bp_enter; // 该trace的进入点
};

/**
 * 断点属性标志
 * no_slot: 该断点没有指令slot, 需要模拟指令行为
 * event: 事件类型的断点,执行到它时触发,打印触发时间,记录触发次数
 * trace: 跟踪类型的断点,一般跟踪函数执行的时间,该断点一般在函数开头,
 *        函数进入和返回时,打印时间,并计算时间差。（实际上，一定要在函数开头，如果一个函数中出现两个trace的断点，
 *        那么返回的时候将会出现问题，所以要保证函数中只能出现一个trace类型的断点）
 * return: 是否是breakpoint_return_类型
 * enable: 是否启用该断点
 **/
#define BKPT_ATTR(F)                                                           \
  F(ENABLE)                                                                    \
  F(RETURN)                                                                    \
  F(EVENT)                                                                     \
  F(NO_SLOT)

#define BKPT_ATTR_OFFSET(name) BKPT_ATTR_OFFSET_##name,
enum __breakpoint_attr_offset {
  BKPT_ATTR(BKPT_ATTR_OFFSET) BKPT_ATTR_OFFSET_NR
};
#undef BKPT_ATTR_OFFSET

#define BKPT_ATTR_FLAG(name)                                                   \
  BKPT_ATTR_FLAG_##name = 0x1u << BKPT_ATTR_OFFSET_##name,
enum __breakpoint_attr_flag { BKPT_ATTR(BKPT_ATTR_FLAG) };
#undef BKPT_ATTR_FLAG

#define BKPT_ATTR_FUN(name)                                                    \
  static inline void breakpoint_attr_set_##name(breakpoint_t *bp) {            \
    bp->attr |= BKPT_ATTR_FLAG_##name;                                         \
  }                                                                            \
  static inline void breakpoint_attr_clr_##name(breakpoint_t *bp) {            \
    bp->attr &= ~BKPT_ATTR_FLAG_##name;                                        \
  }                                                                            \
  static inline int breakpoint_attr_flag_##name(breakpoint_t *bp) {            \
    return !!(bp->attr & BKPT_ATTR_FLAG_##name);                               \
  }

BKPT_ATTR(BKPT_ATTR_FUN)
#undef BKPT_ATTR_FUN

// 创建一个断点对象
breakpoint_normal_t *breakpoint_normal_new(void);
breakpoint_return_t *breakpoint_return_new(void);
// 查找给定地址上面的断点
breakpoint_t *breakpoint_find(addr_t addr);
// 增加一个断点
int breakpoint_add(breakpoint_t *bkpt);
// 移除一个断点
int breakpoint_remove(breakpoint_t *bkpt);

// 启用一个断点对象
int breakpoint_enable(breakpoint_t *bkpt);

// 停用一个断点对象
int breakpoint_disable(breakpoint_t *bkpt);

#endif
