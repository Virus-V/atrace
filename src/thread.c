#include <stdlib.h>
#include <assert.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#include "include/common.h"
#include "include/thread.h"

// profilee的线程列表
LIST_HEAD(threads);

struct thread *thread_add(pid_t pid, int status){
    struct thread *t = NULL;

    if((t = calloc(1, sizeof(struct thread))) == NULL){
        perror("calloc");
        return NULL;
    }

    t->pid = pid;
    t->state = status;
    INIT_LIST_HEAD(&t->context_chain);
    INIT_LIST_HEAD(&t->thread_chain);
    
    list_add_tail(&t->thread_chain, &threads);
    
    return t;
}

void thread_delete(struct thread *t){
    assert(t != NULL);
    
    list_del(&t->thread_chain);
    // TODO 释放context chain
    free(t);
}

// 通过pid查找线程对象
struct thread *thread_get_by_pid(pid_t pid){
    struct thread *curr;

    list_for_each_entry(curr, &threads, thread_chain){
        if(curr->pid != pid){
            continue;
        }
        return curr;
    }

    return NULL;
}

// 释放pid对应的线程
void thread_delete_pid(pid_t pid){
    struct thread *curr;

    // 找到pid对应的线程
    curr = thread_get_by_pid(pid);
    if(!curr){
        printf("bug: no thread object was found for pid %d!\n", pid);
    }else{
        thread_delete(curr);
    }
}

// 检测线程状态,记录线程状态
// pid参数参考waitpid的手册
// 返回null请检查errno
struct thread *thread_wait(pid_t pid, int option){
    int wait_status;
    struct thread *curr;
    pid_t child_waited;

    curr = thread_get_by_pid(pid);
    if(!curr){
        errno = ECHILD;
        printf("bug: pid %d not exit.\n", pid);
        return NULL;
    }

    do {
        wait_status = 0;
        child_waited = waitpid(pid, &wait_status, option | __WALL);
    } while (child_waited == -1 && errno == EINTR);
    if(child_waited == -1){
        perror("waitpid");
        return NULL;
    }

    // 记录线程状态
    curr->state = wait_status;

    return curr;
}

// 暂停一个线程
// 失败返回-1，检查errno
int thread_stop(struct thread *t){
    pid_t child_waited;
    int rv;

    assert(t != NULL);

    if(WIFEXITED(t->state)){
        printf("pid %d exited with %d\n", WEXITSTATUS(t->state));
        return 0;
    }

    if(WIFSTOPPED(t->state)){
        return 0;
    }

    rv = kill(t->pid, SIGTRAP);
    if(rv == -1){
        // 没有对应的这个子进程
        errno = errno == ESRCH ? ECHILD : errno;
        perror("kill");
        return -1;
    }
    
    if(thread_wait(t->pid, 0) == NULL){
        return -1;
    }

    assert(WIFSTOPPED(t->state));

    return 0;
}

int thread_all_stop(void){
    struct thread *curr, *curr_tmp;
    pid_t child_waited;
    int rv;

    // 所有线程进入停机状态
    list_for_each_entry_safe(curr, curr_tmp, &threads, thread_chain){
        rv = thread_stop(curr);
        if(rv == 0){
            continue;
        }
        
        // 没有这个子进程
        if(errno == ECHILD){
            printf("no such child pid %d\n", curr->pid);
            thread_delete(curr);
        }
    }
    return 0;
}

// 运行一个线程
int thread_run(struct thread *t){
    int rv;
    int wait_status;
    pid_t child_waited;

    assert(t != NULL);

    if(WIFEXITED(t->state)){
        printf("pid %d exited with %d\n", WEXITSTATUS(t->state));
        return 0;
    }

    if(WIFCONTINUED(t->state)){
        return 0;
    }

    // TODO Signal injection
    rv = ptrace(PTRACE_CONT, t->pid, NULL, t->signal);
    if(rv == -1){
        // 没有对应的这个子进程
        errno = errno == ESRCH ? ECHILD : errno;
        perror("ptrace");
        return -1;
    }

    if(thread_wait(t->pid, WCONTINUED) == NULL){
        return -1;
    }

    assert(WIFCONTINUED(t->state));

    return 0;
}

int thread_all_run(void){
    struct thread *curr;
    pid_t child_waited;
    int rv;

    // 所有线程进入运行状态
    list_for_each_entry(curr, &threads, thread_chain){
        rv = thread_run(curr);
        if(rv == 0){
            continue;
        }
        
        // 没有这个子进程
        if(errno == ECHILD){
            printf("no such child pid %d\n", curr->pid);
            thread_delete(curr);
        }
    }
    return 0;
}

// 单一线程进入单步模式
int thread_single_step(struct thread *t){

    if (ptrace(PTRACE_SINGLESTEP, t->pid, 0, 0)) {
        perror("ptrace");
        return -1;
    }

    // 等待该线程状态变化
    if(thread_wait(t->pid, 0) == NULL){
        perror("thread_wait");
        return -1;
    }
    
    return 0;
}