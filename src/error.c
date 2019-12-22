#include <stdlib.h>
#include "include/error.h"

#define MODULE_DECLEAR(m,msg) extern const char **_module_##m##_desc;
MODULE_LIST(MODULE_DECLEAR)
#undef MODULE_DECLEAR

#define MODULE_TABLE_ENTRY(m, msg) [MODULE_##m] = &_module_##m##_desc,
static const char ***module_code_table[] = {
    MODULE_LIST(MODULE_TABLE_ENTRY)
    [MODULE_MAX] = NULL
};
#undef MODULE_TABLE_ENTRY

const char *error_to_msg(error_t errcode){
    int index;
    const char **next;
    
    if(errcode == 0){
        return "";
    }

    index = (errcode >> (ERR_DATA_SIZE - MODULE_SIZE - 1)) & ((0x1 << MODULE_SIZE) - 1);

    next = *module_code_table[index];

    index = errcode & ((0x1 << (ERR_DATA_SIZE - MODULE_SIZE - 1)) - 1);
    
    return next[index];
}
