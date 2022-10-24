#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

pthread_t tid;

int fastroutine()
{
   // pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old);
    printf("graceful exit!\n");
    pthread_exit((void*)99);
    return 1;
}


int main()
{
    int retval;
    pthread_create(&tid, NULL, (void* (*)(void*))&fastroutine, NULL);
    //sleep(1);
    
    if(pthread_cancel(tid) != 0)
        printf("cancel didnt work!\n");
    pthread_join(tid, (void**) &retval);
    printf("got the retval: %d\n", retval);
    return 0;
}