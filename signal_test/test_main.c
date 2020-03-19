#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include <signal.h> 
#include <sys/syscall.h>
#include <assert.h>

#define get_tid() syscall(SYS_gettid)

#define MAX_THREAD 3

volatile int exit_flag = 0;

pthread_t thread_list[MAX_THREAD];
long int max_delay[MAX_THREAD];
long int min_delay[MAX_THREAD];
long int avg_delay[MAX_THREAD];

void sigroutine(int dunno) {
    exit_flag = 1;
} 

void *thread_fun(void *arg) {
    int i = (int)arg;

    struct timeval  tv_before, tv_after;
    pid_t tid = get_tid();
    int diff;
    uint64_t before, after, divider = 0;

    while(!exit_flag){
        usleep(100);

        gettimeofday(&tv_before, NULL);
        __asm__ __volatile__("int $0x3");
        gettimeofday(&tv_after, NULL);

        after = tv_after.tv_sec*1000000 + tv_after.tv_usec;
        before = tv_before.tv_sec*1000000 + tv_before.tv_usec;

        diff = after - before;

        assert(diff >= 0);

        printf("%d: %d.%d - %d.%d (%dus)\n", i,
            tv_before.tv_sec, tv_before.tv_usec,
            tv_after.tv_sec, tv_after.tv_usec,
            diff);

        avg_delay[i] = (avg_delay[i] + diff) >> divider;

        divider = 1;

        if(diff > max_delay[i]){
            max_delay[i] = diff;
        }

        if(diff < min_delay[i]){
            min_delay[i] = diff;
        }
    }
}


int main() {
    int i;
    long int max = 0, min = 999999;

    for(i=0; i<MAX_THREAD; i++){
        min_delay[i] = min;
    }

    signal(SIGINT, sigroutine);

    for(i=0; i<MAX_THREAD; i++) {
        pthread_create(thread_list+i, NULL, thread_fun, (void *)i);
    }

    for(i=0; i<MAX_THREAD; i++){
        pthread_join(thread_list[i], NULL);
    }

    printf("============================-max delay time-=============================\n");
    for(i=0; i<MAX_THREAD; i++) {
        printf("thread %d: max: %ldus, %lfms, avg: %ldus, %lfms, min: %ldus, %lfms\n", i, 
            max_delay[i], max_delay[i]/1000.0, 
            avg_delay[i], avg_delay[i]/1000.0, 
            min_delay[i], min_delay[i]/1000.0);
        
        if(max_delay[i] > max) {
            max = max_delay[i];
        }
        if(min_delay[i] < min) {
            min = min_delay[i];
        }
    }

    printf("\n max=%dus, min=%dus\n", max, min);

    return 0;
}
