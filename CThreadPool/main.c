#include "threadpool.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

void taskFunc(void* arg)
{
    int num = *(int*)arg;
    printf("thread %ld is working, number = %d\n",
        pthread_self(), num);
    sleep(1);
}



int main(int argc, char const *argv[])
{
    printf("threadpool test\n");
    threadpool_t* pool = threadpool_create(5,10,15);
    if (pool == NULL)
    {
        printf("threadpool create failed\n");
        return 0;
    }
    // 向线程池中添加任务
    for(int i=0;i<100;i++)
    {
        int* num = (int*)malloc(sizeof(int));
        *num = i+50;
        threadpool_add_task(pool, (void*)taskFunc, (void*)num);
    }
    sleep(20);
    // 销毁线程池
    threadpool_destroy(pool);
    return 0;
}
