#ifndef _ERROR_H_
#define _ERROR_H_

//#include <stdint.h>

// 系统中模块最大个数，2^4
#define MODULE_SIZE 4
#define ERR_DATA_TYPE int
#define ERR_DATA_SIZE 32

// error code
#define MODULE_LIST(_)                                                         \
  _(SYSTEM, "System")                                                          \
  _(THREAD, "Thread Module")                                                   \
  _(BREAKPOINT, "Breakpoint Module")                                           \
  _(CONTEXT, "Context Module")                                                 \
  _(OBJECT, "Object Module")

#define DEFINE_MODULE(m, msg) MODULE_##m,
enum __modules__ { MODULE_LIST(DEFINE_MODULE) MODULE_MAX };
#undef DEFINE_MODULE

// 定义接口错误码
typedef signed ERR_DATA_TYPE error_t;

#define GEN_ERR_CODE(MODULE, CODE)                                             \
  (error_t)((0x1 << (ERR_DATA_SIZE - 1)) |                                     \
            (((MODULE) & ((0x1 << MODULE_SIZE) - 1))                           \
             << (ERR_DATA_SIZE - MODULE_SIZE - 1)) |                           \
            ((unsigned ERR_DATA_TYPE)((CODE) > 0 ? (CODE) : -(CODE))) &        \
                ((0x1 << (ERR_DATA_SIZE - MODULE_SIZE - 1)) - 1))
enum {
#define GENERATE_ERROR_CODE(MODULE, NAME, CODE)                                \
  HBIPC_MSG_##NAME = GEN_ERR_CODE(MODULE, CODE)
  GENERATE_ERROR_CODE(MODULE_SYSTEM, NO_RESOURCE, 1),
  GENERATE_ERROR_CODE(MODULE_SYSTEM, INVAILD_PARAMETER, 2),
  GENERATE_ERROR_CODE(MODULE_SYSTEM, ALREADY_RUNNING, 3),
  GENERATE_ERROR_CODE(MODULE_SYSTEM, INIT_FAILED, 4),
  GENERATE_ERROR_CODE(MODULE_SYSTEM, UNKNOW_HOOK, 5),
  GENERATE_ERROR_CODE(MODULE_SYSTEM, LOAD_DYN_FAILED, 6),

#undef GENERATE_ERROR_CODE
};

#endif