#include <assert.h>

#include "include/common.h"
#include "include/breakpoint.h"

// 构造断点指令
instr_t arch_make_breakpoint(instr_t ori_instr){
    return (ori_instr & ~(0xFF)) | 0xCC;
}

// 移除断点指令
instr_t arch_remove_breakpoint(instr_t instr, instr_t ori_instr){
    assert((instr & 0xFF) == 0xCC);
    return (instr & ~(0xFF)) | (ori_instr & 0xFF);
}

// 
addr_t arch_get_breakpoint_pc(addr_t addr){
    return addr - 1;
}