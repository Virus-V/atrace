#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

int main(){
    printf("hello world\n");
    raise(SIGKILL);
    printf("2222\n");
    return 0;
}