#include <stdio.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

int main() {
	pid_t child_pid;
	int status = 0;
 
	if ((child_pid = fork()) == 0) { //子进程返回
		// 使新进程进入trace状态
		ptrace(PTRACE_TRACEME, 0, NULL, NULL);
		execl("./tracee", "", 0); //这个路径需要更改
		printf("child process start failed...\n");
	}else{
		siginfo_t siginfo;
		long ptraceOption = PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXIT;

		do{
			status = waitid(P_ALL, 0, &siginfo, WSTOPPED);
		}while(status == -1 && errno == EINTR);
		if(status == -1){
			perror("waitid");
			printf("errno:%d\n", errno);
			exit(1);
		}
		WIFEXITED(status);
		printf("si_pid:%d,si_uid:%d,si_signo:%d,si_status:0x%x,si_code:%d\n",
			siginfo.si_pid, siginfo.si_uid, siginfo.si_signo, siginfo.si_status, 
			siginfo.si_code
		);

		
		ptrace(PTRACE_SETOPTIONS, child_pid, NULL, ptraceOption); //设置ptrace属性PTRACE_SETOPTIONS
		ptrace(PTRACE_CONT, child_pid, NULL, NULL);
		while (1) {
			do{
				status = waitid(P_ALL, 0, &siginfo, WEXITED|WSTOPPED);
			}while(status == -1 && errno == EINTR);
			if(status == -1){
				perror("waitid");
				printf("errno:%d\n", errno);
				exit(1);
			}

			printf("si_pid:%d,si_uid:%d,si_signo:%d,si_status:0x%x,si_code:%d\n",
				siginfo.si_pid, siginfo.si_uid, siginfo.si_signo, siginfo.si_status, 
				siginfo.si_code
			);
			
			// status 高8位是PTRACE_EVENT_CLONE这种事件，低8位是信号
			
			switch(siginfo.si_code){
				case CLD_EXITED:
					printf("CLD_EXITED\n");
				break;

				case CLD_KILLED:
					printf("CLD_KILLED\n");
				break;

				
					printf("CLD_DUMPED\n");
				break;

				
					
					printf("CLD_STOPPED\n");
				break;

				case CLD_TRAPPED:
					printf("CLD_TRAPPED\n");
					printf("event:%d,signal %d\n", siginfo.si_status >> 8, siginfo.si_status & 0xFF);
				break;

				case CLD_CONTINUED:	// 这是在STOP转为运行的时候触发，但是在跟踪状态下就不会被触发
				case CLD_STOPPED:	// 子进程设置为被跟踪状态，则不会出现STOPPED状态
				case CLD_DUMPED:	// 产生了coredump文件
				default:
					printf("un-handled status\n");
				break;
			}
			printf("pid %d continue..sig:%d\n", siginfo.si_pid, siginfo.si_status);
			//ptrace(PTRACE_CONT, siginfo.si_pid, 0, 0);
		}
	}
	return 0;
}
