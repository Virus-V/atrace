
#include <stdlib.h>
#include <assert.h>
#include <sys/ptrace.h>
#include <sys/reg.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>

#include "include/common.h"
#include "include/breakpoint.h"

extern pid_t leader_pid;
extern struct list_head objects;

// 使能一个断点
static void breakpoint_enable(struct breakpoint *bp){
    assert(bp);

    if(bp->attr & BKPT_F_ENABLE){
        return;
    }

    bp->ori_instrction = ptrace(PTRACE_PEEKTEXT, leader_pid, bp->address, 0);
    ptrace(PTRACE_POKETEXT, leader_pid, bp->address, arch_make_breakpoint(bp->ori_instrction));

    bp->attr |= BKPT_F_ENABLE;

    return;
}

static void breakpoint_disable(struct breakpoint *bp){
    assert(bp);
    
    if(!(bp->attr & BKPT_F_ENABLE)){
        return;
    }

    instr_t data = ptrace(PTRACE_PEEKTEXT, leader_pid, bp->address, 0);
    ptrace(PTRACE_POKETEXT, leader_pid, bp->address, arch_remove_breakpoint(data, bp->ori_instrction));

    bp->attr &= (~BKPT_F_ENABLE);
    
    return;
}

// 创建断点
struct breakpoint *breakpoint_create(struct object *obj, uint64_t offset, uint32_t attr){
    struct breakpoint *bp = NULL;

    assert(obj != NULL);
    assert(offset != 0);

    if(obj->text_start + offset > obj->text_end){
        printf("break point offset too large!\n");
        return NULL;
    }

    bp = calloc(1, sizeof(struct breakpoint));
    if(!bp){
        return NULL;
    }

    if(attr & BKPT_F_ENABLE){
        breakpoint_enable(bp);
    }

    bp->address = obj->text_start + offset;
    bp->attr = attr;

    // 记录该obj对应的断点
    list_add_tail(&bp->bkp_chain, &obj->breakpoint_chain);

    return bp;
}

// 删除断点
void breakpoint_delete(struct breakpoint *bp){
    // 先取消断点
    breakpoint_disable(bp);
    
    list_del(&bp->bkp_chain);

    free(bp);
}

// 越过断点
int breakpoint_resume(struct breakpoint *bp, struct thread *t){

    assert(bp != NULL);
    assert(t != NULL);

    if(arch_set_thread_pc(t, bp->address) < 0){
        return -1;
    }

    breakpoint_disable(bp);

    if(thread_single_step(t) < 0){
        return -1;
    }

    breakpoint_enable(bp);

    return 0;
}

// 通过当前pc找到breakpoint
struct breakpoint *breakpoint_find(struct thread *t){
    struct object *curr;
    struct breakpoint *bp = NULL;
    addr_t bp_pc, curr_pc;

    curr_pc = arch_get_thread_pc(t);
    if(curr_pc == 0){
        return NULL;
    }
    
    bp_pc = arch_get_breakpoint_pc(curr_pc);

    list_for_each_entry(curr, &objects, object_chain){
        if(curr->text_start > bp_pc || curr->text_end < bp_pc){
            continue;
        }

        // 找到对应的断点对象
        list_for_each_entry(bp, &curr->breakpoint_chain, bkp_chain){
            if(bp->address != bp_pc){
                continue;
            }

            return bp;
        }
    }
    return NULL;
}