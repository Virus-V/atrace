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
		long ptraceOption = PTRACE_O_TRACECLONE;

		do{
			status = waitid(P_ALL, 0, &siginfo, WSTOPPED);
		}while(status == -1 && errno == EINTR);

		printf("si_pid:%d,si_uid:%d,si_signo:%d,si_status:%d,si_code:%d\n",
			siginfo.si_pid, siginfo.si_uid, siginfo.si_signo, siginfo.si_status, 
			siginfo.si_code
		);

		
		ptrace(PTRACE_SETOPTIONS, child_pid, NULL, ptraceOption); //设置ptrace属性PTRACE_SETOPTIONS
		ptrace(PTRACE_CONT, child_pid, NULL, NULL);
		while (1) {
			do{
				status = waitid(P_ALL, 0, &siginfo, WEXITED|WSTOPPED|WCONTINUED);
			}while(status == -1 && errno == EINTR);
			if(status == -1){
				perror("waitid");
				exit(1);
			}

			printf("si_pid:%d,si_uid:%d,si_signo:%d,si_status:%d,si_code:%d\n",
				siginfo.si_pid, siginfo.si_uid, siginfo.si_signo, siginfo.si_status, 
				siginfo.si_code
			);
			
			switch(siginfo.si_code){
				case CLD_EXITED:
					printf("CLD_EXITED\n");
				break;

				case CLD_KILLED:
					printf("CLD_KILLED\n");
				break;

				case CLD_DUMPED:
					printf("CLD_DUMPED\n");
				break;

				case CLD_STOPPED:
					printf("CLD_STOPPED\n");
				break;

				case CLD_TRAPPED:
					printf("CLD_TRAPPED\n");
					printf("signal %d\n", siginfo.si_status);
				break;

				case CLD_CONTINUED:
				default:
					printf("CLD_CONTINUED\n");
				break;
			}

			//ptrace(PTRACE_CONT, siginfo.si_pid, 0, 9);
		}
	}
	return 0;
}
