
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

int main(int argc, char *argv[]) {
	int status = 0;
    
	// 分叉出子进程
    if ((leader_pid = fork()) == 0) { //子进程返回
		// 使新进程进入trace状态
		ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		execvp(argv[1], argv + 1); //这个路径需要更改
		printf("child process start failed...\n");
	}else{
		int child_status;

		waitpid(-1, &child_status, 0); 

		// 创建线程
		if(thread_add(leader_pid, child_status) == NULL){
			printf("create thread failed!\n");
			exit(1);
		}

		long ptraceOption = PTRACE_O_TRACECLONE;
		ptrace(PTRACE_SETOPTIONS, leader_pid, NULL, ptraceOption); //设置ptrace属性PTRACE_SETOPTIONS

		// 获取子进程的可执行文件路径
		object_load();
		const char *exe_path = object_get_exe();
		printf("exe_path: %s\n", exe_path);

		// 根据路径获取它的入口点
		addr_t entry_point = object_get_exe_entry_point(exe_path);
		// 获取object对象
		struct object *self_obj = object_get_by_file(exe_path);
		if(!self_obj){
			printf("get profilee object failed\n");
			exit(1);
		}
		printf("profilee entry point at: 0x%lX\n", self_obj->text_start + entry_point);

		// 在入口点处打上断点
		instr_t data = ptrace(PTRACE_PEEKTEXT, leader_pid, self_obj->text_start + entry_point, 0);
		printf("entry text:%lX\n", data);

		struct breakpoint *bp = breakpoint_create(self_obj, entry_point, BKPT_F_ENTER_CTX|BKPT_F_ENABLE);	//entry_point

		data = ptrace(PTRACE_PEEKTEXT, leader_pid, self_obj->text_start + entry_point, 0);
		printf("entry text bp:%lX\n", data);

		// 获取子进程的memory map，获取所有动态库的路径和符号表
		ptrace(PTRACE_CONT, leader_pid, NULL, NULL);
		thread_wait(leader_pid);
		// 刷新当前object
		object_load();
		// 越过断点并删除断点
		breakpoint_resume(bp);
		breakpoint_delete(bp);

		ptrace(PTRACE_CONT, leader_pid, NULL, NULL);
		while (1) {
			printf("parent process wait child pid \n");
			// 接收所有child的信号
			pid_t child_waited = waitpid(-1, &status, __WALL); //等待接收信号

			if (WIFSTOPPED(status)) {
				printf("child %ld recvied signal %d\n", (long)child_waited, WSTOPSIG(status));
			}

			if (WIFSIGNALED(status)) {
				printf("child %ld recvied signal %d\n", (long)child_waited, WTERMSIG(status));
			}
			printf("child status change!\n");
			if (WIFSTOPPED(status) && (WSTOPSIG(status) == SIGSTOP || WSTOPSIG(status) == SIGTRAP)){
				//新线程被创建完成后，收到的信号，或者遇到断点时
				printf("task stop\n");
				if (WSTOPSIG(status) == SIGTRAP) {
					printf("reach break point\n");
					breakpoint_resume(bp);
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
	return 0;
}