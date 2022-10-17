#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

pthread_t tids[12];

int fastroutine()
{
    int old;
   // pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old);
    printf("graceful exit!\n");
    pthread_exit(99);
}

int stupidroutine(){
    int retval;
    for (size_t i = 0; i < 12; i++)
    {
        int ret = pthread_join(tids[i], retval);
        printf("ret stupid: %d for joining %ld\n", ret, tids[i]);
    }
    return 1;
}


int main()
{
    int retvals[10];
    for (size_t i = 0; i < 10; i++)
    {
        pthread_create(&tids[i], NULL, (void* (*)(void*))&fastroutine, NULL);
    }
    for (size_t i = 0; i < 2; i++)
    {
        pthread_create(&tids[i+10], NULL, (void* (*)(void*))&stupidroutine, NULL);
    }
    for (size_t i = 0; i < 10; i++)
    {
        int ret = pthread_join(tids[i], &retvals[i]);
        printf("join returned: %d was able to join %ld got val %d\n",ret, i, retvals[i]);
    }
    return 0;
}