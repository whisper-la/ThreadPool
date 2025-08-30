#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

typedef struct ThreadPool threadpool_t;
// 创建线程池并初始化
threadpool_t* threadpool_create(int minThreadNum, int maxThreadNum, int taskQueueCapacity);

// 销毁线程池
int threadpool_destroy(threadpool_t* pool);

// 向线程池中添加任务
void threadpool_add_task(threadpool_t* pool, void (*function)(void*), void* arg);

// 获取线程池中工作的线程的个数
int threadpool_getBusyNum(threadpool_t* pool);

// 获取线程池中存活的线程的个数
int threadpool_getLiveNum(threadpool_t* pool);

#endif /* THREADPOOL_H */
