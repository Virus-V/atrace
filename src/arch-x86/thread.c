#include <assert.h>
#include <stdlib.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <sys/reg.h>

#include "include/common.h"
#include "include/thread.h"

addr_t arch_get_thread_pc(struct thread *t){
    struct user_regs_struct regs;

    assert(t != NULL);

    if(ptrace(PTRACE_GETREGS, t->pid, 0, &regs) < 0){
        perror("ptrace");
        return 0;
    }

    return regs.rip;
}

int arch_set_thread_pc(struct thread *t, addr_t pc){
    struct user_regs_struct regs;

    assert(t != NULL);

    if(ptrace(PTRACE_GETREGS, t->pid, 0, &regs) < 0){
        perror("ptrace");
        return -1;
    }

    regs.rip = (long)pc;

    if(ptrace(PTRACE_SETREGS, t->pid, 0, &regs) < 0){
        perror("ptrace");
        return -1;
    }

    return 0;
}

// 获得当前函数调用的返回值
// X86-64下当前%rbp指向的位置前面一个word，就是函数返回地址
addr_t arch_get_return_pc(struct thread *t){
    struct user_regs_struct regs;

    assert(t != NULL);

    if(ptrace(PTRACE_GETREGS, t->pid, 0, &regs) < 0){
        perror("ptrace");
        return 0;
    }

    
}