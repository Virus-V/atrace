
#include <stdlib.h>
#include <assert.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "include/common.h"
#include "include/breakpoint.h"

extern pid_t leader_pid;

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
int breakpoint_resume(struct breakpoint *bp, pid_t pid){
    struct user_regs_struct regs;
    int wait_status;

    ptrace(PTRACE_GETREGS, pid, 0, &regs);

    // 检查是否停在了指定的断点
    // TODO 与平台相关，也与流水线深度相关
    assert(regs.rip == (unsigned long) bp->address + 1);

    // 恢复原来的值，单步越过断点
    regs.rip = (long) bp->address;
    ptrace(PTRACE_SETREGS, pid, 0, &regs);

    breakpoint_disable(bp);

    if (ptrace(PTRACE_SINGLESTEP, pid, 0, 0)) {
        perror("ptrace");
        return -1;
    }
    // 等待该线程状态变化
    waitpid(pid, &wait_status, __WALL);

    if (WIFEXITED(wait_status)) 
        return 0;

    /* Re-enable the breakpoint and let the process run.
    */
    breakpoint_enable(bp);

    return 0;
}