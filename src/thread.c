#include <stdlib.h>
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

void thread_delete(pid_t pid){
    struct thread *curr, *curr_tmp;

    // 找到pid对应的线程
    list_for_each_entry_safe(curr, curr_tmp, &threads, thread_chain){
        if(curr->pid != pid){
            continue;
        }
        list_del(&curr->thread_chain);
        free(curr);
        return;
    }

    printf("bug: no thread object was found for pid %d!\n", pid);
}

struct thread *thread_wait(pid_t pid){
    int wait_status;
    struct thread *curr;
    pid_t child_waited;

    do {
        wait_status = 0;
        child_waited = waitpid(pid, &wait_status, __WALL);
    } while (child_waited == -1 && errno == EINTR);
    if(child_waited == -1){
        perror("waitpid");
        return NULL;
    }

    assert(child_waited == pid);

    list_for_each_entry(curr, &threads, thread_chain){
        if(curr->pid != child_waited){
            continue;
        }
        curr->state = wait_status;
        return curr;
    }

    return NULL;
}

int thread_all_stop(void){
    struct thread *curr;
    pid_t child_waited;
    int rv;

    // 所有线程进入停机状态
    list_for_each_entry(curr, &threads, thread_chain){
        rv = kill(curr->pid, SIGTRAP);
        if(rv == -1){
            perror("kill");
            return -1;
        }
        
        do {
            child_waited = waitpid(curr->pid, &curr->state, __WALL);
        } while (child_waited == -1 && errno == EINTR);
        if(child_waited == -1){
            perror("waitpid");
            return -2;
        }
    }
    return 0;
}

int thread_all_run(void){
    struct thread *curr;
    pid_t child_waited;
    int rv;

    // 所有线程进入停机状态
    list_for_each_entry(curr, &threads, thread_chain){
        rv = kill(curr->pid, SIGTRAP);
        if(rv == -1){
            perror("kill");
            return -1;
        }
        
        do {
            child_waited = waitpid(curr->pid, &curr->state, __WALL);
        } while (child_waited == -1 && errno == EINTR);
        if(child_waited == -1){
            perror("waitpid");
            return -2;
        }
    }
    return 0;
}