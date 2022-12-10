#include <stdio.h>
#include "pthread.h"

pthread_t tid_;

typedef struct parameters
{
    int arg1;
    int arg2;
} parameters;


int routine(void* args){
    void* retval;
    parameters* param = (parameters*) args;
    printf("%d was returned from join\n", pthread_join(98, &retval));
    int result = 78 * param->arg1 + param->arg2;
    pthread_cancel(tid_);
    return result;
}

int main()
{
    void* retval;
    pthread_t* kernelattack = (pthread_t*) 0xFFFF934200000UL;
    parameters args;
    args.arg1 =  34;
    args.arg2 =  8;
    pthread_create(kernelattack, NULL, (void* (*)(void*))&routine, (void*) &args);
    pthread_cancel(813);
    printf("join returned %d. ",pthread_join(tid_, &retval));
    printf("returnvalue %ld\n", (size_t)retval);
    return 0;
}