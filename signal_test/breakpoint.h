#ifndef _BREAK_POINT_H_
#define _BREAK_POINT_H_

#include "common.h"
#include "list.h"
#include "rbtree.h"

#include "object.h"

/**
 * 断点其实是一个事件触发器,动态的插在程序中
 * 程序执行流执行到断点时,会触发该断点绑定的相应事件
 *
 **/
struct breakpoint {
  addr_t address;      // 断点位置
  instr_t instruction; // 断点位置原来的指令
  uint_least8_t attr;  // 断点属性

  object_t *obj;                     // 该断点所处的object
  struct rb_node breakpoint_tree;    // 通过红黑树找到该对象
  struct list_head breakpoint_chain; // 通过object找到该对象
};
typedef struct breakpoint breakpoint_t;

/**
 * 断点属性标志
 * event: 事件类型的断点,执行到它时触发,打印触发时间,记录触发次数
 * trace: 跟踪类型的断点,一般跟踪函数执行的时间,该断点一般在函数开头,
 *        函数进入和返回时,打印时间,并计算时间差。（实际上，一定要在函数开头，如果一个函数中出现两个trace的断点，
 *        那么返回的时候将会出现问题，所以要保证函数中只能出现一个trace类型的断点）
 * enable: 是否启用该断点
 **/
#define BKPT_ATTR(F)                                                           \
  F(ENABLE)                                                                    \
  F(EVENT)                                                                     \
  F(TRACE)

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
breakpoint_t *breakpoint_new(void);
// 查找给定地址上面的断点
breakpoint_t *breakpoint_find(addr_t addr);
// 增加一个断点
int breakpoint_add(breakpoint_t *bkpt);
// 移除一个断点
void breakpoint_remove(breakpoint_t *bkpt);

// 启用一个断点对象
int breakpoint_enable(breakpoint_t *bkpt);

// 停用一个断点对象
int breakpoint_disable(breakpoint_t *bkpt);

#endif
