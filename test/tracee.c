#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

void sigsev(){  
    printf("segment fault!!!\n");  
}  


static void *thread_fun1(void *arg){
    while(1){
        printf("thread 1\n");
        sleep(1);
        //return NULL;
    }
    return NULL;
}

static void *thread_fun2(void *arg){
    while(1){
        printf("thread 2\n");
        sleep(2);
    }
    return NULL;
}

int main(){
    int *a = NULL;
    pthread_t thread1,thread2;

    //signal(SIGSEGV,sigsev);
    pthread_create(&thread1, NULL, &thread_fun1, NULL);
    pthread_create(&thread2, NULL, &thread_fun2, NULL);
    sleep(1);
    printf("hello world\n");
    //*a = 1;
    //raise(SIGSEGV);
    printf("2222\n");
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    return 0;
}