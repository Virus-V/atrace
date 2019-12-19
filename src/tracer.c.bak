#include <stdio.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

//http://www.xuebuyuan.com/1572720.html
int main() {
	pid_t child_pid;
	int status = 0;
 
	if ((child_pid = fork()) == 0) { //子进程返回
		// 使新进程进入trace状态
		ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		execl("./tracee", "", 0); //这个路径需要更改
		printf("child process start failed...\n");
	}else{
		// 等同于 waitpid(-1, &wstatus, 0); 
		wait(NULL); //接收SIGTRAP信号
		long ptraceOption = PTRACE_O_TRACECLONE;
		ptrace(PTRACE_SETOPTIONS, child_pid, NULL, ptraceOption); //设置ptrace属性PTRACE_SETOPTIONS
		ptrace(PTRACE_CONT, child_pid, NULL, NULL);
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

			if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP){
				//新线程被创建完成后，收到的信号，或者遇到断点时
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
