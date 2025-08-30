#include "threadpool.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>


/* 管理者线程和工作线程的函数 */
// 管理者线程函数
void* threadpool_manager(void* arg);
// 工作线程函数
void* threadpool_worker(void* arg);
// 线程退出函数
void threadpool_threadExit(threadpool_t* pool);

#define NUM 10  // 一次性最多添加/减少3个线程
// 任务结构体
typedef struct {
    void (*function)(void* arg);
    void* arg;
} task_t;

// 线程池结构体
struct ThreadPool
{
    // 任务队列
    task_t* taskQueue; // 任务队列
    int taskQueueSize; // 任务队列大小
    int taskQueueCapacity; // 任务队列容量
    int taskQueueFront; // 任务队列头
    int taskQueueRear; // 任务队列尾

    // 线程池
    pthread_t *threadIDs; // 线程池
    pthread_t managerThread; // 管理线程
    int minThreadNum; // 最小线程数
    int maxThreadNum; // 最大线程数
    int busyThreadNum; // 忙线程数
    int liveThreadNum; // 存活线程数
    int exitThreadNum; // 退出线程数

    // 信号量
    pthread_mutex_t poolMutex; // 线程池锁
    pthread_mutex_t busyMutex; // 忙线程锁
    pthread_cond_t notEmpty; // 任务队列不为空
    pthread_cond_t notFull; // 任务队列不为满

    int shutdown; // 线程池是否关闭
};


// 创建线程池并初始化
threadpool_t* threadpool_create(int minThreadNum, int maxThreadNum, int taskQueueCapacity)
{
    threadpool_t* pool = (threadpool_t*)malloc(sizeof(threadpool_t)); // 创建线程池结构体
    do
    {
        if (pool == NULL)
        {
            perror("threadpool malloc failed......\n");
            break;
        }

        pool->threadIDs=(pthread_t*)malloc(sizeof(pthread_t)*maxThreadNum); // 创建线程数组
        if (pool->threadIDs == NULL)
        {
            perror("threadpool threadIDs malloc failed......\n");
            break;
        }
        memset(pool->threadIDs, 0, sizeof(pthread_t)*maxThreadNum); // 初始化线程数组
        pool->minThreadNum=minThreadNum; // 最小线程数
        pool->maxThreadNum=maxThreadNum; // 最大线程数
        pool->liveThreadNum=minThreadNum; // 初始化存活线程数
        pool->busyThreadNum=0; // 初始化忙线程数
        pool->exitThreadNum=0; // 初始化退出线程数

        pool->taskQueue=(task_t*)malloc(sizeof(task_t)*taskQueueCapacity); // 创建任务队列
        if (pool->taskQueue == NULL)
        {
            perror("threadpool taskQueue malloc failed......\n");
            break;
        }
        pool->taskQueueSize=0; // 任务队列大小
        pool->taskQueueCapacity=taskQueueCapacity; // 任务队列容量
        pool->taskQueueFront=0; // 任务队列头
        pool->taskQueueRear=0; // 任务队列尾

        // 初始化信号量
        if(pthread_mutex_init(&pool->poolMutex, NULL) != 0||
        pthread_mutex_init(&pool->busyMutex, NULL) != 0||
        pthread_cond_init(&pool->notEmpty, NULL) != 0||
        pthread_cond_init(&pool->notFull, NULL) != 0)
        {
            perror("threadpool mutex or cond init failed......\n");
            break;
        }

        pool->shutdown=0; // 线程池是否关闭标志位

        // 创建管理者线程
        pthread_create(&pool->managerThread, NULL, threadpool_manager, pool); // @todo

        // 创建工作线程组
        for(int i=0;i<minThreadNum;i++)
        {
            pthread_create(&pool->threadIDs[i], NULL, threadpool_worker, pool);
        }
        printf("threadpool create success\n");
        return pool;
    } while (0);
    if(pool && pool->threadIDs)
    {
        free(pool->threadIDs);
        pool->threadIDs=NULL;
    }
    if (pool && pool->taskQueue)
    {
        free(pool->taskQueue);
        pool->taskQueue=NULL;
    }
    if (pool) 
    {
        free(pool);
        pool=NULL;
    }
    return NULL;
}

// 销毁线程池
/* 
    1. 先关闭线程池
    2. 阻塞回收管理者线程
    3. 唤醒消费者线程
    4. 释放堆内存
    5. 销毁信号量
*/
int threadpool_destroy(threadpool_t* pool)
{
    // 销毁线程池
    if (pool == NULL)
    {
        return -1;
    }

    // 先关闭线程池
    pool->shutdown=1;
    // 阻塞回收管理者线程
    printf("threadpool destroy, managerThread is %ld\n", pool->managerThread);
    pthread_join(pool->managerThread, NULL);
    // 唤醒消费者线程
    for(int i=0;i<pool->liveThreadNum;i++)
    {
        pthread_cond_signal(&pool->notEmpty);
    }
    // 销毁信号量
    pthread_mutex_destroy(&pool->poolMutex);
    pthread_mutex_destroy(&pool->busyMutex);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);
    // 释放堆内存
    if(pool->taskQueue)
    {
        free(pool->taskQueue);
        pool->taskQueue=NULL;
    }
    if(pool->threadIDs)
    {
        free(pool->threadIDs);
        pool->threadIDs=NULL;
    }
    if(pool)
    {
        free(pool);
        pool=NULL;
    }
    printf("threadpool destroy success\n");
    return 0;
}


// 向线程池中添加任务
/* 
    1. 先加锁
    2. 等待任务队列不为满
    3. 添加任务
    4. 解锁
    5. 通知工作线程
*/
void threadpool_add_task(threadpool_t* pool, void (*function)(void*), void* arg)
{
    pthread_mutex_lock(&pool->poolMutex);
    while (pool->taskQueueSize == pool->taskQueueCapacity && !pool->shutdown)
    {
        pthread_cond_wait(&pool->notFull, &pool->poolMutex);
    }
    if(pool->shutdown)
    {
        pthread_mutex_unlock(&pool->poolMutex);
        return;
    }

    // 添加任务
    pool->taskQueue[pool->taskQueueRear].function=function;
    pool->taskQueue[pool->taskQueueRear].arg=arg;
    pool->taskQueueRear=(pool->taskQueueRear+1)%pool->taskQueueCapacity;
    pool->taskQueueSize++;

    // 通知工作线程
    pthread_cond_signal(&pool->notEmpty);
    pthread_mutex_unlock(&pool->poolMutex);
    printf("threadpool add task, taskQueueSize is %d\n", pool->taskQueueSize);
}

// 获取线程池中工作的线程的个数
int threadpool_getBusyNum(threadpool_t* pool)
{
    int busyNum=0;
    pthread_mutex_lock(&pool->busyMutex);
    busyNum=pool->busyThreadNum;
    pthread_mutex_unlock(&pool->busyMutex);
    return busyNum;
}

// 获取线程池中存活的线程的个数
int threadpool_getLiveNum(threadpool_t* pool)
{
    int  liveNum=0;
    pthread_mutex_lock(&pool->poolMutex);
    liveNum=pool->liveThreadNum;
    pthread_mutex_unlock(&pool->poolMutex);
    return liveNum;
}

// 管理者线程函数
/* 
    1. 每间隔5s检查一次
    2. 管理者线程检查线程池中的线程个数、任务数量
    3. 管理者线程检查线程池中忙线程数量
    4. 添加线程
    5. 销毁线程
*/
void* threadpool_manager(void* arg)
{
    threadpool_t* pool = (threadpool_t*)arg;
    while (!pool->shutdown)
    {
        // 每间隔5s检查一次
        sleep(5);

        // 管理者线程检查线程池中的线程个数、任务数量
        pthread_mutex_lock(&pool->poolMutex);
        int liveNum=pool->liveThreadNum;
        int queueSize=pool->taskQueueSize;
        pthread_mutex_unlock(&pool->poolMutex);

        // 管理者线程检查线程池中忙线程数量
        pthread_mutex_lock(&pool->busyMutex);
        int busyNum=pool->busyThreadNum;
        pthread_mutex_unlock(&pool->busyMutex);

        // 添加线程
        // 管理者线程判断是否需要创建线程
        if (liveNum<pool->maxThreadNum && queueSize>liveNum)
        {
            // 加锁
            pthread_mutex_lock(&pool->poolMutex);
            int count=0;
            // 创建线程
            for (int i = 0; i < pool->maxThreadNum  && pool->liveThreadNum<pool->maxThreadNum && queueSize>liveNum && count<NUM; i++)
            {
                if(pool->threadIDs[liveNum]==0)
                {
                    pthread_create(&pool->threadIDs[liveNum], NULL, threadpool_worker, pool);
                    pool->liveThreadNum++;
                    count++;
                }
            }
            // 解锁
            pthread_mutex_unlock(&pool->poolMutex);
        }

        // 销毁线程
        // 管理者线程判断是否需要销毁线程
        if (busyNum*2<liveNum && liveNum>pool->minThreadNum)
        {
            // 加锁
            pthread_mutex_lock(&pool->poolMutex);
            pool->exitThreadNum=NUM;
            // 解锁
            pthread_mutex_unlock(&pool->poolMutex);

            for(int i=0;i<pool->exitThreadNum;++i)
            {
                pthread_cond_signal(&pool->notEmpty);
            }
            
        }
    }
}

// 工作线程函数
/* 
    循环的取出队头任务并执行，若队头无任务则阻塞等待
    1. 先加锁
    2. 等待任务队列不为空
    3. 取出任务
    4. 解锁
    5. 执行任务
    6. 任务执行完成
    7. 释放任务参数
*/
void* threadpool_worker(void* arg)
{
    threadpool_t* pool = (threadpool_t*)arg;
    while (1)
    {
        pthread_mutex_lock(&pool->poolMutex);
        while (pool->taskQueueSize == 0 && !pool->shutdown)
        {
            pthread_cond_wait(&pool->notEmpty, &pool->poolMutex);
            if(pool->exitThreadNum>0)
            {
                pool->exitThreadNum--;
                if(pool->liveThreadNum>pool->minThreadNum)
                {
                    pool->liveThreadNum--;
                    pthread_mutex_unlock(&pool->poolMutex);
                    threadpool_threadExit(pool);
                }
            }
        }
        if (pool->shutdown)
        { 
            pthread_mutex_unlock(&pool->poolMutex);
            threadpool_threadExit(pool);
        }

        // 从队头取出任务函数
        task_t task;
        task=pool->taskQueue[pool->taskQueueFront];
        pool->taskQueueFront=(pool->taskQueueFront+1)%pool->taskQueueCapacity;
        pool->taskQueueSize--;
        // 通知添加任务函数
        pthread_cond_signal(&pool->notFull); // 是任务添加函数的消费者，通知添加任务函数可以添加任务了
        pthread_mutex_unlock(&pool->poolMutex);

        pthread_mutex_lock(&pool->busyMutex);
        pool->busyThreadNum++;
        printf("thread %ld start, busyThreadNum is %d\n", pthread_self(),pool->busyThreadNum);
        pthread_mutex_unlock(&pool->busyMutex);

        task.function(task.arg);
        free(task.arg);
        task.arg=NULL;

        pthread_mutex_lock(&pool->busyMutex);
        pool->busyThreadNum--;
        printf("thread %ld end, busyThreadNum is %d\n", pthread_self(),pool->busyThreadNum);
        pthread_mutex_unlock(&pool->busyMutex);
    }
    return NULL;
}
// 线程退出函数
void threadpool_threadExit(threadpool_t* pool)
{
    pthread_t threadID = pthread_self();
    for(int i=0;i<pool->maxThreadNum;++i)
    {
        if(pool->threadIDs[i]==threadID)
        {
            pool->threadIDs[i]=0;
            break;
        }
    }
    pthread_exit(NULL);
}
