
#include <stdlib.h>
#include <assert.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sys/types.h>

#include "include/common.h"
#include "include/breakpoint.h"

extern pid_t child_pid;

// 使能一个断点
static void breakpoint_enable(struct breakpoint *bp){
    assert(bp);

    bp->ori_instrction = ptrace(PTRACE_PEEKTEXT, child_pid, bp->address, 0);
    ptrace(PTRACE_POKETEXT, child_pid, bp->address, arch_make_breakpoint(bp->ori_instrction));
}


/* Disable the given breakpoint by replacing the byte it points to with
** the original byte that was there before trap insertion.
*/
static void breakpoint_disable(struct breakpoint *bp){
    assert(bp);
    
    instr_t data = ptrace(PTRACE_PEEKTEXT, child_pid, bp->address, 0);
    ptrace(PTRACE_POKETEXT, child_pid, bp->address, arch_remove_breakpoint(data, bp->ori_instrction));
}

struct breakpoint *breakpoint_create(struct object *obj, uint64_t offset){
    struct breakpoint *bp = NULL;

    assert(obj != NULL);
    assert(offset != 0);

    if(obj->text_start + offset > obj->text_end){
        return NULL;
    }

    bp = calloc(1, sizeof(struct breakpoint));
    if(!bp){
        return NULL;
    }
    bp->address = obj->text_start + offset;
    breakpoint_enable(bp);

    // 记录该obj对应的断点
    list_add(&bp->bkp_chain, &obj->breakpoint_chain);

    return bp;
}

// 移除断点
void breakpoint_delete(struct breakpoint *bp){
    // todo
}


int breakpoint_resume(struct breakpoint *bp){
    struct user_regs_struct regs;
    int wait_status;

    ptrace(PTRACE_GETREGS, child_pid, 0, &regs);
    /* Make sure we indeed are stopped at bp */
    /*printf("reg is %ld, bp is %ld\n", regs.eip, (unsigned long) bp->addr + 1);*/
    assert(regs.rip == (unsigned long) bp->address + 1);

    /* Now disable the breakpoint, rewind EIP back to the original instruction
    ** and single-step the process. This executes the original instruction that
    ** was replaced by the breakpoint.
    */
    regs.rip = (long) bp->address;

    ptrace(PTRACE_SETREGS, child_pid, 0, &regs);
    breakpoint_disable(bp);
    if (ptrace(PTRACE_SINGLESTEP, child_pid, 0, 0)) {
        perror("ptrace");
        return -1;
    }
    wait(&wait_status);

    if (WIFEXITED(wait_status)) 
        return 0;

    /* Re-enable the breakpoint and let the process run.
    */
    breakpoint_enable(bp);

    return 0;
}