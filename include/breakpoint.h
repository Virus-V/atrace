#ifndef _BREAK_POINT_H_
#define _BREAK_POINT_H_

#include "include/common.h"
#include "include/object.h"

// 断点属性
#define BKPT_F_ENTER_CTX	(0x1 << 0)
#define BKPT_F_ENABLE	(0x1 << 1)

// 程序断点
struct breakpoint {
	addr_t address;	// 断点位置
	instr_t ori_instrction;	// 断点位置原来的指令
	struct list_head bkp_chain;	// 断点链表
	uint32_t attr;	// 断点属性
};

// 构造断点指令
instr_t arch_make_breakpoint(instr_t ori_instr);
// 移除断点指令
instr_t arch_remove_breakpoint(instr_t instr, instr_t ori_instr);

// 增加断点
struct breakpoint *breakpoint_create(struct object *obj, uint64_t offset, uint32_t attr);
// 删除断点
void breakpoint_delete(struct breakpoint *bp);
// 越过断点并删除断点
int breakpoint_resume_delete();
// 越过断点并保留断点
int breakpoint_resume(struct breakpoint *bp);
#endif