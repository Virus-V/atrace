#ifndef _BREAK_POINT_H_
#define _BREAK_POINT_H_

#include "include/common.h"
#include "include/object.h"

// 断点类型
enum breakpoint_type {
	// 进入上下文断点
	BKPT_ENTER_CTX,
	// 退出上下文断点
	BKPT_EXIT_CTX,
};

// 程序断点
struct breakpoint {
	addr_t address;	// 断点位置
	instr_t ori_instrction;	// 断点位置原来的指令
	struct list_head bkp_chain;	// 断点链表
	enum breakpoint_type type;	// 是上下文入口断点还是出口断点
	const char *desc;	// 该断点的描述信息
};

// 构造断点指令
instr_t arch_make_breakpoint(instr_t ori_instr);
// 移除断点指令
instr_t arch_remove_breakpoint(instr_t instr, instr_t ori_instr);

// 增加断点
struct breakpoint *breakpoint_create(struct object *obj, uint64_t offset);
// 删除断点
void breakpoint_delete(struct breakpoint *bp);
// 越过断点并删除断点
int breakpoint_resume_delete();
// 越过断点并保留断点
int breakpoint_resume(struct breakpoint *bp);
#endif