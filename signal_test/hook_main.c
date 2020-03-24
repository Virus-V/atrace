#define _GNU_SOURCE 1
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <ucontext.h>

#include "thread.h"
#include "object.h"
#include "breakpoint.h"

#define get_tid() syscall(SYS_gettid)

extern void print_trace(void);

// 设置断点信号处理函数
static void
set_sighandle(void(*handle)(int , siginfo_t *, void *))
{
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
	sigemptyset(&act.sa_mask);

	act.sa_sigaction = handle;
	act.sa_flags = SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGTRAP, &act, NULL) == -1) {
		perror("sigaction()");
		abort();
	}
}

/**
 * 断点指令信号处理函数
 */
void
sigtrap_handler(int signo, siginfo_t *sinfo, void *context)
{
  struct timeval tv;
  ucontext_t *uc = (ucontext_t *)context;
  pid_t thread_id = get_tid();
  thread_t *thread = NULL;

  thread = thread_map_find(thread_id);
  if (!thread) {
    thread = thread_new();
    thread->tid = thread_id;
    thread_map_add(thread);
    printf("new thread: %d\n", thread_id);
  }

  gettimeofday(&tv, NULL);

  printf("%d: in handle: %d.%d\n", get_tid(), tv.tv_sec, tv.tv_usec);
  printf("addr:%p\n", uc->uc_mcontext.pc);
  // 跳过断点指令
  uc->uc_mcontext.pc += 4;
}

/**
 * main入口点断点处理函数
 */
void
entrypoint_trap_handler(int signo, siginfo_t *sinfo, void *context)
{
  struct timeval tv;
  ucontext_t *uc = (ucontext_t *)context;

  // 重新加载object
  object_load();

  // 切换到trap handle
  set_sighandle(sigtrap_handler);

}

/**
 * Profiler 全局初始化
 */
static void
__attribute__ ((constructor))
lib_init(void)
{
  addr_t entry_point;
  breakpoint_t *entry_bp;
  // 初始化信号处理函数
  set_sighandle(entrypoint_trap_handler);

  // 初始化线程map
  thread_map_init();

  // 加载object
  object_load();

  // 在main中打断点
  entry_point = object_get_self_entrypoint();
  printf("app entry point: %p\n", entry_point);

  entry_bp = breakpoint_new();
  entry_bp->address = entry_point;
  breakpoint_attr_set_EVENT(entry_bp);

  // 增加断点
  if (breakpoint_add(entry_bp)) {
    printf("add breakpoint failed!\n");
    abort();
  }

  // 使能断点
  if (breakpoint_enable(entry_bp)) {
    printf("enable breakpoint failed!\n");
    abort();
  }

  printf("Library ready. \n");

  return;
}
