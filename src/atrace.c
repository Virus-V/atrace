
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "include/common.h"
#include "include/object.h"
#include "include/thread.h"
#include "include/breakpoint.h"


// 子线程pid
pid_t leader_pid;

// 运行分析器
int run_profiler(struct thread *t){
	struct thread *curr;
	const char *exe_path;
	addr_t entry_point;
	struct object *profilee_obj;
	struct breakpoint *bp;

	object_load();

	exe_path = object_get_exe();
	if(!exe_path){
		printf("can't get profilee execute file path\n");
		exit(1);
	}

	// 根据路径获取它的入口点
	entry_point = object_get_exe_entry_point(exe_path);

	profilee_obj = object_get_by_file(exe_path);
	if(!profilee_obj){
		printf("get profilee object failed\n");
		exit(1);
	}

	free(exe_path);

	printf("profilee entry point at: 0x%lX\n", profilee_obj->text_start + entry_point);

	// 在入口点处打上断点
	bp = breakpoint_create(profilee_obj, entry_point, BKPT_F_ENTER_CTX|BKPT_F_ENABLE);
	if(!bp){
		printf("add breakpoint failed!\n");
		exit(1);
	}

	// 获取子进程的memory map，获取所有动态库的路径和符号表
	if(thread_run(t)){
		perror("thread_run");
		exit(1);
	}

	// 等待运行到断点
	if(thread_wait(t->pid, 0) != t){
		perror("thread_wait");
		exit(1);
	}
	if(!(WIFSTOPPED(t->state) && WSTOPSIG(t->state) == SIGTRAP)){
		printf("not in main breakpoint!\n");
		exit(1);
	}

	// 刷新当前object
	object_load();

	// TODO 对相关函数打断点

	// 越过断点并删除断点
	if(breakpoint_resume(bp, t) < 0){
		perror("breakpoint_resume");
		exit(1);
	}
	breakpoint_delete(bp);

	// 任务运行
	if(thread_run(t) < 0){
		perror("thread_run");
		exit(1);
	}

	while (1) {

		// 接收所有child的信号
		curr = thread_wait(-1, 0);
		if(!curr){
			perror("thread_wait");
			exit(1);
		}

		// 暂停所有其他线程
		if(thread_all_stop()){
			printf("thread_all_stop failed!\n");
			exit(1);
		}

		// 处理线程退出
		if(WIFEXITED(curr->state)){
			printf("child %ld exit with %d\n", (long)curr->pid, WEXITSTATUS(curr->state));
			// TODO 打印相关调用信息？
			thread_delete(curr);
			continue;
		}

		if (WIFSTOPPED(curr->state)) {
			printf("child %ld recvied signal %d\n", (long)curr->pid, WSTOPSIG(curr->state));
		}

		if (WSTOPSIG(curr->state) == SIGTRAP){
			pid_t new_pid;
			// 创建线程
			if (((curr->state >> 16) & 0xffff) == PTRACE_EVENT_CLONE) {
				struct thread *new;
				if (ptrace(PTRACE_GETEVENTMSG, curr->pid, 0, &new_pid) < 0) {
					perror("ptrace");
					continue;
				}

				// 创建线程
				new = thread_add(new_pid, 0);
				if(!new){
					printf("add new thread failed!\n");
					exit(1);
				}
			} else {
				struct breakpoint *bp = NULL;
				addr_t curr_pc;

				curr_pc = arch_get_thread_pc(curr);
				if(curr_pc == 0){
					printf("get current pc failed!\n");
					exit(1);
				}

				// 找到当前断点
				bp = breakpoint_find(curr_pc);
				if(bp == NULL){
					printf("find breakpoint failed!\n");
					exit(1);
				}
				
				// 进入上下文
				if(bp->attr & BKPT_F_ENTER_CTX){
					
				}
				
				// 判断该断点是进入上下文还是退出上下文
				// 如果是进入则创建新的上下文，如果是退出则打印上下文数据，删除该上下文

			}

			// 遇到断点
			if (WSTOPSIG(status) == SIGTRAP) {
				printf("reach break point\n");
				breakpoint_resume(bp, leader_thread);
			};
			ptrace(PTRACE_CONT, child_waited, 1, NULL);
			continue;
		}

		if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) {
			//当一个线程创建另一个线程返回时，收到的信号
			pid_t new_pid;
			if (((status >> 16) & 0xffff) == PTRACE_EVENT_CLONE) {
				if (ptrace(PTRACE_GETEVENTMSG, child_waited, 0, &new_pid)!= -1) {
					printf("thread %d created\n", new_pid);
				}
			}
		}

		if (child_waited == -1)
			break;

		if (WIFEXITED(status)) {
			//线程结束时，收到的信号
			printf("thread %d exited with status %d\t\n", child_waited, WEXITSTATUS(status));
		}
		ptrace(PTRACE_CONT, child_waited, 1, NULL);
	}

}

int main(int argc, char *argv[]) {
	int status = 0;
    
	// 分叉出子进程
    if ((leader_pid = fork()) == 0) { //子进程返回
		// 使新进程进入trace状态
		ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		execvp(argv[1], argv + 1); //这个路径需要更改
		printf("child process start failed...\n");
	}else{
		int child_status, rv = 0;
		long ptraceOption = PTRACE_O_TRACECLONE;
		struct thread *leader_thread;

		waitpid(leader_pid, &child_status, 0);

		ptrace(PTRACE_SETOPTIONS, leader_pid, NULL, ptraceOption);

		// 创建线程
		leader_thread = thread_add(leader_pid, child_status);
		if(leader_thread == NULL){
			printf("create thread failed!\n");
			exit(1);
		}

		rv = run_profiler(leader_thread);
		if(rv){
			printf("run profiler failed!\n");
			exit(1);
		}
	}
	return 0;
}