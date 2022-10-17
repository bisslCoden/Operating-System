#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

pthread_t tid;

int fastroutine()
{
    int old;
   // pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old);
    printf("graceful exit!\n");
    pthread_exit((void*)99);
    return 1;
}


int main()
{
    int ret;
    int returner;
    pthread_create(&tid, NULL, (void* (*)(void*))&fastroutine, NULL);
    returner =  pthread_join(tid, (void**)&ret);
    printf("join returned: %d got val %d\n",returner, ret);
    return 0;
}